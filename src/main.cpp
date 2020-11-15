#include <iostream>
#include <boost/filesystem.hpp>
#include <memory>
#include "CInputFile.h"

namespace fs = boost::filesystem;

int main(int argc, const char* argv[]) {
    fs::path inputPath(argv[1]);

    auto inputFile = std::make_unique<CInputFile>(inputPath);

    inputFile->parse();

    return 0;
}
