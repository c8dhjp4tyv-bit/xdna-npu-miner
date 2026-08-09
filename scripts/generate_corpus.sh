#!/usr/bin/env bash
set -euo pipefail

build_dir="${1:-build}"
"${build_dir}/bpp9000_tests" --corpus
