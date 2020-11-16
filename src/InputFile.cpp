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

    void InputFile::parse() {
        auto *location = (uint8_t *) this->m_mappedInputFile.data();
        const auto *endLocation = location + this->m_mappedInputFile.size();

        while (location < endLocation) {
            auto *block = (block_t *) location;

            if (block->type == TYPE_EPB) {
                auto *epb = (pcapng_epb_t *) location;

                auto *usbHeader = (pcap_usb_header_t *) (location + sizeof(pcapng_epb_t));

                auto deviceId = usbHeader->device_address;

                auto &device = this->m_devices[deviceId];

                auto *data = (uint8_t *) (usbHeader + sizeof(pcap_usb_header_t));

                if (device == nullptr) {
                    device = std::make_unique<Device>(deviceId);

                    device->processPacket(usbHeader, data);

                    this->m_devices[deviceId] = std::move(device);
                } else {
                    device->processPacket(usbHeader, data);
                }
            }

            auto *trailingSize = (uint32_t *) (location + block->length - sizeof(uint32_t));
            if (block->length != *trailingSize) {
                std::cerr << "Block length != trailingLength" << std::endl;
                exit(-1);
            }

            location += block->length;
        }
    }

    std::map<uint32_t, std::shared_ptr<Device>> InputFile::getDevices() {
        return this->m_devices;
    }
}
