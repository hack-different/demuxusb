//
// Created by Rick Mark on 11/14/20.
//

#include "InputFile.h"
#include <utility>
#include <iostream>
#include <cassert>

namespace deusbmux {
    InputFile::InputFile(fs::path path) {
        std::cout << "File Size: " << fs::file_size(path) << std::endl;

        this->m_path = std::move(path);

        this->m_mappedInputFile.open(this->m_path);

        std::cout << "Mapped File Size: " << this->m_mappedInputFile.size() << std::endl;
    }



    std::map<uint32_t, std::shared_ptr<Device>> InputFile::getDevices() {
        return this->m_devices;
    }
}
