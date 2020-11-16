//
// Created by Rick Mark on 11/14/20.
//

#include <cassert>
#include <iostream>
#include "Device.h"

namespace deusbmux {

    Device::Device(uint32_t id) {
        this->m_device = id;
    }

    void Device::processPacket(void *data) {

        // Endpoint 0 is special - for descriptors
        //if ((header->endpointId & 0x7F) == 0x0) {

        //}
        //assert(this->m_device == header->bus_id);

        //std::cout << "URB Length: " << std::hex << header->urb_len << std::endl;
        //std::cout << "Data Length: " << std::hex << header->data_len << std::endl;

        this->m_packetCount++;
    }

    uint64_t Device::getPacketCount() const {
        return this->m_packetCount;
    }

}