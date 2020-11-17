//
// Created by Rick Mark on 11/15/20.
//

#ifndef DEUSBMUX_PCAPNGINPUTFILE_H
#define DEUSBMUX_PCAPNGINPUTFILE_H

#include "deusbmux.hpp"
#include "InputFile.h"

#include <utility>

namespace deusbmux {

    class PcapNGInputFile : public InputFile {
    public:
        explicit PcapNGInputFile(fs::path input) : InputFile(std::move(input)) {}
        ~PcapNGInputFile() override;

        void parse() override;
    };
}

#endif //DEUSBMUX_PCAPNGINPUTFILE_H
