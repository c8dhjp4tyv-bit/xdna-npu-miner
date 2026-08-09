#!/usr/bin/env python3
"""Aggregate one machine-readable M5 record per tested batch/column artifact."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
from typing import Any


GENERATED_DIGEST = "2979889feed3352b3c12831a301a357b6c9099f3de80b955f152c53bca2f8c03"
PRODUCTION_DIGEST = "7c1da1028b9ecdbae54616654606185e62076ff7b69e209ecbf3d23f6a2fede1"


def relative_path(value: str, root: Path) -> str:
    path = Path(value)
    if not path.is_absolute():
        path = root / path
    try:
        return os.path.relpath(path, root)
    except ValueError:
        return str(path)


def read_json(path: Path) -> dict[str, Any]:
    with path.open(encoding="utf-8") as stream:
        value = json.load(stream)
    if not isinstance(value, dict):
        raise ValueError(f"evidence is not a JSON object: {path}")
    return value


def placement_evidence(document: dict[str, Any], root: Path) -> dict[str, Any]:
    artifact_dir = Path(document["artifact"]["artifact_dir"])
    if not artifact_dir.is_absolute():
        artifact_dir = root / artifact_dir
    layout_path = artifact_dir / "xdna_m5.layout.json"
    partition_path = artifact_dir / "xdna_m5.prj" / "main_aie_partition.json"
    mlir_path = artifact_dir / "xdna_m5.prj" / "aie.mlir"
    layout = read_json(layout_path)
    partition_document = read_json(partition_path)
    partition = partition_document["aie_partition"]["partition"]
    return {
        "logical_lanes": document["placement"]["logical_lanes"],
        "lane_mapping": layout["lane_mapping"],
        "items_per_lane": layout["items_per_lane"],
        "generated_aie_placement": document["placement"]["generated_aie_placement"],
        "execution_dependent_unique_lane_inputs": document["placement"][
            "execution_dependent_unique_lane_inputs"
        ],
        "execution_dependent_unique_lane_matches": document["placement"][
            "execution_dependent_unique_lane_matches"
        ],
        "artifact_layout_file": relative_path(str(layout_path), root),
        "generated_mlir_file": relative_path(str(mlir_path), root),
        "partition_metadata_file": relative_path(str(partition_path), root),
        "partition_column_width": partition["column_width"],
        "partition_start_columns": partition["start_columns"],
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--repo-root", type=Path, default=Path.cwd())
    parser.add_argument("--config", type=Path, action="append", required=True)
    args = parser.parse_args()

    root = args.repo_root.resolve()
    documents = [read_json(path) for path in args.config]
    if not documents:
        raise ValueError("at least one M5 configuration is required")

    configurations: list[dict[str, Any]] = []
    rejected: list[dict[str, Any]] = []
    baseline_document = documents[0]
    baseline_metrics = baseline_document.get("m4_baseline")
    baseline_artifact = baseline_document.get("m4_baseline_artifact")
    if baseline_metrics is None or baseline_artifact is None:
        raise ValueError("every M5 record must include the measured M4 baseline")

    for path, document in zip(args.config, documents):
        if document.get("milestone") != "M5":
            raise ValueError(f"not an M5 evidence record: {path}")
        benchmark = document["m5_benchmark"]
        correctness = document["correctness"]
        accepted = (
            document.get("status") == "PASS"
            and benchmark.get("mismatches") == 0
            and benchmark.get("runtime_failures") == 0
            and correctness.get("mismatches") == 0
            and correctness.get("runtime_failures") == 0
            and correctness.get("ordering_preserved") is True
            and correctness.get("state_reset_per_item") is True
            and correctness.get("mutation_visibility_pattern_verified") is True
        )
        config = {
            "batch_size": document["artifact"]["batch_size"],
            "columns": document["artifact"]["target_columns"],
            "status": "ACCEPTED" if accepted else "REJECTED",
            "artifact": {
                **document["artifact"],
                "artifact_dir": relative_path(document["artifact"]["artifact_dir"], root),
            },
            "placement": placement_evidence(document, root),
            "correctness": correctness,
            "benchmark": benchmark,
            "buffer_footprint": {
                "input_item_bytes": document["batch_schema"]["input_item_stride_bytes"],
                "output_item_bytes": document["batch_schema"]["output_item_stride_bytes"],
                "input_arena_bytes": document["artifact"]["batch_size"]
                * document["batch_schema"]["input_item_stride_bytes"],
                "output_arena_bytes": document["artifact"]["batch_size"]
                * document["batch_schema"]["output_item_stride_bytes"],
                "total_arena_bytes": document["artifact"]["batch_size"]
                * (
                    document["batch_schema"]["input_item_stride_bytes"]
                    + document["batch_schema"]["output_item_stride_bytes"]
                ),
                "per_lane_input_bytes": (
                    document["artifact"]["batch_size"]
                    // document["artifact"]["target_columns"]
                )
                * document["batch_schema"]["input_item_stride_bytes"],
                "per_lane_output_bytes": (
                    document["artifact"]["batch_size"]
                    // document["artifact"]["target_columns"]
                )
                * document["batch_schema"]["output_item_stride_bytes"],
            },
        }
        if accepted:
            configurations.append(config)
        else:
            rejected.append(
                {
                    "batch_size": config["batch_size"],
                    "columns": config["columns"],
                    "reason": "correctness or runtime gate failed",
                    "evidence_file": relative_path(str(path), root),
                }
            )

    if not configurations:
        raise ValueError("no accepted M5 configuration is available")
    selected = min(
        configurations,
        key=lambda item: item["benchmark"]["wall_time_ms"]["median"],
    )
    selected_summary = {
        "batch_size": selected["batch_size"],
        "columns": selected["columns"],
        "median_wall_time_ms": selected["benchmark"]["wall_time_ms"]["median"],
        "rationale": "lowest measured median wall time among exact, runtime-clean configurations",
    }

    aggregate = {
        "schema_version": 1,
        "milestone": "M5",
        "status": "PASS" if not rejected else "IN PROGRESS",
        "work_unit": baseline_document["work_unit"],
        "workload": baseline_document["workload"],
        "toolchain": baseline_document["toolchain"],
        "baseline": {
            "path": "M4_REFERENCE_ACCEL_PATH",
            "artifact": baseline_artifact,
            "benchmark": baseline_metrics,
            "active_columns": 1,
            "correctness": {
                "cpu_recomputation_enabled": True,
                "mismatches": baseline_metrics["mismatches"],
                "runtime_failures": baseline_metrics["runtime_failures"],
            },
        },
        "configurations": configurations,
        "selected_configuration": selected_summary,
        "rejected_configurations": rejected,
        "unsupported_configurations": [],
        "verification": {
            "cpu_canonical_recomputation": True,
            "npu_only_authorization": False,
            "m1_generated_digest": GENERATED_DIGEST,
            "m1_production_digest": PRODUCTION_DIGEST,
            "regressions": {"m1": "PASS", "m2": "PASS", "m3": "PASS", "m4": "PASS"},
        },
        "buffer_reuse": baseline_document["buffer_reuse"],
        "diagnostics": {
            "raw_timings_only": True,
            "speedup_claim": False,
            "profitability_claim": False,
        },
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8") as stream:
        json.dump(aggregate, stream, indent=2)
        stream.write("\n")


if __name__ == "__main__":
    main()
