#include <iostream>
#include <fstream>
#include <boost/filesystem.hpp>
#include <memory>
#include "file/InputFile.h"
#include "protocol/USBMUXInterfaceExpert.h"


using namespace demuxusb;

static void usage(const char *program) {
    std::cerr << "Usage: " << program << " [options] <capture_file.pcapng|capture_file.pcap>\n"
              << "\n"
              << "Options:\n"
              << "  -v, --verbose        Show decoded protocol details (recovery commands, plist summaries,\n"
              << "                       DFU requests and anomalies)\n"
              << "  -p, --plists         Print every decoded USBMUX property list as XML\n"
              << "  -x, --extract <dir>  Write DFU images, recovery uploads and plist transcripts to <dir>\n"
              << "  -w, --warnings       Print packets that could not be decoded\n"
              << "  -h, --help           Show this help\n";
}

static void extract(const fs::path &directory, size_t index, Device *device) {
    auto deviceDirectory = directory / (boost::format("device-%1%-%2$x") % index % device->getIdentifier()).str();

    for (const auto &expert: device->getExperts()) {
        for (const auto &artifact: expert->getArtifacts()) {
            fs::create_directories(deviceDirectory);
            auto path = deviceDirectory / artifact.name;
            std::ofstream output(path.string(), std::ios::binary);
            output.write(reinterpret_cast<const char *>(artifact.data.data()),
                         static_cast<std::streamsize>(artifact.data.size()));
            std::cout << "  Extracted " << path.string() << " (" << artifact.data.size() << " bytes)" << std::endl;
        }
    }
}

int main(int argc, const char *argv[]) {
    bool verbose = false, plists = false, warnings = false;
    std::string extractDirectory;
    std::string inputPath;

    for (int i = 1; i < argc; i++) {
        std::string argument = argv[i];
        if (argument == "-v" || argument == "--verbose") {
            verbose = true;
        } else if (argument == "-p" || argument == "--plists") {
            plists = true;
        } else if (argument == "-w" || argument == "--warnings") {
            warnings = true;
        } else if (argument == "-x" || argument == "--extract") {
            if (i + 1 >= argc) {
                usage(argv[0]);
                return 1;
            }
            extractDirectory = argv[++i];
        } else if (argument == "-h" || argument == "--help") {
            usage(argv[0]);
            return 0;
        } else if (!argument.empty() && argument[0] == '-') {
            std::cerr << "Unknown option " << argument << std::endl;
            usage(argv[0]);
            return 1;
        } else if (inputPath.empty()) {
            inputPath = argument;
        } else {
            usage(argv[0]);
            return 1;
        }
    }

    if (inputPath.empty()) {
        usage(argv[0]);
        return 1;
    }

    std::unique_ptr<InputFile> input;
    try {
        input = InputFile::open(fs::path(inputPath));
        input->parse();
    } catch (const ParseError &error) {
        std::cerr << "Error: " << error.what() << std::endl;
        if (input == nullptr) { return 1; }
        std::cerr << "Reporting what was parsed before the error." << std::endl;
    }

    size_t index = 0;
    for (const auto &device: input->getDevices()) {
        std::cout << "\n\nDevice ID: " << std::hex << device->getIdentifier() << std::dec << std::endl;
        if (device->hasDeviceDescriptor()) {
            std::cout << "  VID/PID: " << boost::format("%1$04x:%2$04x") % device->getVendorId() %
                                          device->getProductId()
                      << " (" << device->getMode() << " mode)" << std::endl;
        }
        std::cout << "  Manufacturer: " << device->getManufacturer() << std::endl;
        std::cout << "  Product: " << device->getProduct() << std::endl;
        std::cout << "  Serial: " << device->getSerial() << std::endl;
        std::cout << "  Statistics: " << device->getPacketCount() << " packets and " <<
                  device->getByteCount() << " bytes. (control " << device->getControlPacketCount() << " packets, " <<
                  device->getControlByteCount() << " bytes)." << std::endl;

        if (verbose) {
            for (const auto &anomaly: device->getAnomalies()) {
                std::cout << "  ! " << anomaly << std::endl;
            }
        }

        std::cout << "  Interface Experts:\n";
        for (const auto &expert: device->getExperts()) {
            if (expert->isEmpty()) { continue; }

            std::cout << "    " << expert->toString() << std::endl;
            if (verbose) {
                std::cout << expert->details();
            }

            auto *usbmux = dynamic_cast<USBMUXInterfaceExpert *>(expert.get());
            if (plists && usbmux != nullptr) {
                for (const auto &[id, stream]: usbmux->getStreams()) {
                    for (const auto &message: stream->getMessages()) {
                        std::cout << boost::format("    --- stream %1$x %2% ---\n") % id %
                                     (message.outbound ? "host -> device" : "device -> host");
                        std::cout << message.xml();
                    }
                }
            }
        }

        if (!extractDirectory.empty()) {
            extract(fs::path(extractDirectory), index, device);
        }
        index++;
    }

    if (input->getWarningCount() > 0) {
        std::cerr << "\n" << input->getWarningCount() << " packets could not be fully decoded"
                  << (warnings ? ":" : " (use --warnings to list them)") << std::endl;
        if (warnings) {
            for (const auto &warning: input->getWarnings()) {
                std::cerr << "  " << warning << std::endl;
            }
            if (input->getWarningCount() > input->getWarnings().size()) {
                std::cerr << "  ..." << std::endl;
            }
        }
    }

    return 0;
}
