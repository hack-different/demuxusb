#include <iostream>
#include <boost/filesystem.hpp>
#include <memory>
#include "InputFile.h"

namespace fs = boost::filesystem;

using namespace deusbmux;

int main(int argc, const char* argv[]) {
    fs::path inputPath(argv[1]);

    auto inputFile = std::make_unique<InputFile>(inputPath);

    inputFile->parse();

    return 0;
}
