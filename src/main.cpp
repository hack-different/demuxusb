#include <iostream>
#include <boost/filesystem.hpp>
#include <memory>
#include "InputFile.h"
#include "PcapNGInputFile.h"

namespace fs = boost::filesystem;

using namespace deusbmux;

int main(int argc, const char* argv[]) {
    fs::path input_path(argv[1]);

    std::unique_ptr<InputFile> input = nullptr;

    if (input_path.extension() == "pcapng") {
        input = std::make_unique<PcapNGInputFile>(input_path);
    } else {
        std::cerr << "Input file type is not supported." << std::endl;
        exit(-1);
    }

    input->parse();

    return 0;
}
