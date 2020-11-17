//
// Created by Rick Mark on 11/14/20.
//

#include "InputFile.h"


namespace demuxusb {
    InputFile::InputFile(fs::path path) {
        this->m_path = std::move(path);

        this->m_mappedInputFile.open(this->m_path);
    }

    std::vector<Device *> InputFile::getDevices() {
        auto result = std::vector<Device *>{};

        for (const auto &device : this->m_devices) {
            result.push_back(device.second->device());
        }

        return result;
    }
}
