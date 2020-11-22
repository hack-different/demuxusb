//
// Created by Rick Mark on 11/16/20.
//

#ifndef demuxusb_INTERFACEEXPERT_H
#define demuxusb_INTERFACEEXPERT_H

#include "../demuxusb.hpp"

class InterfaceExpert {
public:
    InterfaceExpert(uint8_t configuration, uint8_t interface): m_configuration{configuration}, m_interface{interface} {};
    virtual ~InterfaceExpert() = default;
    virtual void processControlOut(uint8_t interfaceId, byte_array data) { this->m_controlPackets++; this->m_controlBytes += data.second; };
    virtual void processControlIn(uint8_t interfaceId, byte_array data) { this->m_controlPackets++; this->m_controlBytes += data.second; };
    virtual void processBulkOut(uint8_t interfaceId, byte_array data) { this->m_packets++; this->m_bytes += data.second; };
    virtual void processBulkIn(uint8_t interfaceId, byte_array data) { this->m_packets++; this->m_bytes += data.second; };

    virtual bool isEmpty() { return (this->m_controlPackets == 0) && (this->m_packets == 0); }
    virtual std::string toString();
protected:
    uint8_t m_configuration;
    uint8_t m_interface;
    uint32_t m_controlPackets{};
    uint32_t m_packets{};
    uint64_t m_controlBytes{};
    uint64_t m_bytes{};
};


#endif //demuxusb_INTERFACEEXPERT_H
