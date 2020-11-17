#include <iostream>
#include <boost/filesystem.hpp>
#include <memory>
#include "InputFile.h"
#include "PcapNGInputFile.h"



using namespace deusbmux;

int main(int argc, const char* argv[]) {
    fs::path input_path(argv[1]);

    std::unique_ptr<InputFile> input;

    if (input_path.extension() == ".pcapng") {
        input = std::make_unique<PcapNGInputFile>(input_path);
    } else {
        std::cerr << "Input file type is not supported." << std::endl;
        exit(-1);
    }

    input->parse();

    for (const auto& device : input->getDevices()) {
        std::cout << "Device ID: " << device->getIdentifier() << " had " << device->getPacketCount() << " packets and " <<
        device->getByteCount() << " bytes. (control " << device->getControlPacketCount() << " packets, " <<
        device->getControlByteCount() << " bytes)." << std::endl;

    }

    return 0;
}
