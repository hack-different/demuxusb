#include <iostream>
#include <boost/filesystem.hpp>
#include <memory>
#include "file/InputFile.h"
#include "file/PcapNGInputFile.h"


using namespace demuxusb;

int main(int argc, const char *argv[]) {
    fs::path input_path(argv[1]);

    std::unique_ptr<InputFile> input;

    if (input_path.extension() == ".pcapng") {
        input = std::make_unique<PcapNGInputFile>(input_path);
    } else {
        std::cerr << "Input file type is not supported." << std::endl;
        exit(-1);
    }

    input->parse();

    for (const auto &device : input->getDevices()) {
        std::wcout << L"Device ID: " << std::hex << device->getIdentifier() << std::endl;
        std::wcout << L"  Manufacturer: " << device->getManufacturer() << std::endl;
        std::wcout << L"  Product: " << device->getProduct() << std::endl;
        std::wcout << L"  Statistics: " << std::dec << device->getPacketCount() << L" packets and " <<
                   device->getByteCount() << L" bytes. (control " << device->getControlPacketCount() << L" packets, " <<
                   device->getControlByteCount() << L" bytes)." << std::endl;

    }

    return 0;
}
