//
// Created by Rick Mark on 11/14/20.
//

#ifndef demuxusb_CMUXSTREAM_H
#define demuxusb_CMUXSTREAM_H

#include "../demuxusb.hpp"

class MuxStream {
public:
    MuxStream(uint16_t host, uint16_t device) : m_host{host}, m_device{device} {};

    uint16_t getHostPort() { return this->m_host; }
    uint16_t getDevicePort() { return this->m_device; }
    uint64_t getBytes() { return this->m_bytes; }
protected:
    uint16_t m_host;
    uint16_t m_device;
    uint64_t m_bytes;
};


#endif //demuxusb_CMUXSTREAM_H
