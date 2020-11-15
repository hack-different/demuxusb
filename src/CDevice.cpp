//
// Created by Rick Mark on 11/14/20.
//

#include <cassert>
#include <iostream>
#include "CDevice.h"


CDevice::CDevice(uint32_t id) {
    this->m_device = id;

    std::cout << "Device ID: " << std::hex << id << std::endl;
}

void CDevice::processPacket(pcap_usb_header_t *header, void *data) {
    //assert(this->m_device == header->bus_id);

    //std::cout << "URB Length: " << std::hex << header->urb_len << std::endl;
    //std::cout << "Data Length: " << std::hex << header->data_len << std::endl;
}

