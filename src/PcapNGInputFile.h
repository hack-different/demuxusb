//
// Created by Rick Mark on 11/15/20.
//

#ifndef demuxusb_PCAPNGINPUTFILE_H
#define demuxusb_PCAPNGINPUTFILE_H

#include "demuxusb.hpp"
#include "InputFile.h"

#include <utility>

namespace demuxusb {

    class PcapNGInputFile : public InputFile {
    public:
        explicit PcapNGInputFile(fs::path input) : InputFile(std::move(input)) {}

        ~PcapNGInputFile() override;

        void parse() override;
    };
}

#endif //demuxusb_PCAPNGINPUTFILE_H
