#include "xdna/device.hpp"

#include <iostream>
#include <string>

namespace {

void print_usage(const char* program)
{
    std::cerr << "usage: " << program << " [--selector DEVICE]\n";
}

} // namespace

int main(int argc, char** argv)
{
    std::string selector = "0";
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--selector" && index + 1 < argc) {
            selector = argv[++index];
        } else if (argument == "--help") {
            print_usage(argv[0]);
            return 0;
        } else {
            print_usage(argv[0]);
            return 2;
        }
    }

    const xdna::runtime::CapabilityReport report = xdna::runtime::probe_device(selector);
    std::cout << "status=" << xdna::runtime::capability_status_name(report.status) << '\n'
              << "selector=" << report.selector << '\n'
              << "device_name=" << report.device_name << '\n'
              << "architecture=" << report.architecture << '\n'
              << "bdf=" << report.bdf << '\n'
              << "device_node=" << report.device_node << '\n'
              << "firmware_version=" << report.firmware_version << '\n'
              << "xrt_version=" << report.xrt_version << '\n'
              << "xrt_hash=" << report.xrt_hash << '\n'
              << "amdxdna_version=" << report.amdxdna_version << '\n'
              << "detail=" << report.detail << '\n';
    return report.supported() ? 0 : 1;
}
