#ifndef demuxusb_PCAPINPUTFILE_H
#define demuxusb_PCAPINPUTFILE_H

#include "../demuxusb.hpp"
#include "InputFile.h"

#include <utility>

namespace demuxusb {

    // Reads classic libpcap captures (micro or nanosecond timestamps, either byte order).
    class PcapInputFile : public InputFile {
    public:
        explicit PcapInputFile(fs::path input) : InputFile(std::move(input)) {}

        ~PcapInputFile() override = default;

        void parse() override;
    };
}

#endif //demuxusb_PCAPINPUTFILE_H
