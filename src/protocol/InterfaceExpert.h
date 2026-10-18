//
// Created by Rick Mark on 11/16/20.
//

#ifndef demuxusb_INTERFACEEXPERT_H
#define demuxusb_INTERFACEEXPERT_H

#include "../demuxusb.hpp"
#include "pcapng.h"
#include "usb.h"

// A file-like product of protocol analysis (a DFU image, a recovery mode upload, ...) that can be written to disk.
struct ExtractedArtifact {
    std::string name;
    std::vector<std::byte> data;
};

class InterfaceExpert {
public:
    InterfaceExpert(uint8_t configuration, uint8_t interface) : m_configuration{configuration},
                                                                m_interface{interface} {};

    virtual ~InterfaceExpert() = default;

    virtual void processControlOut(const usb_setup_t &setup, byte_array data) {
        this->m_controlPackets++;
        this->m_controlBytes += data.second;
    };

    virtual void processControlIn(const usb_setup_t &setup, byte_array data) {
        this->m_controlPackets++;
        this->m_controlBytes += data.second;
    };

    virtual void processBulkOut(byte_array data) {
        this->m_packets++;
        this->m_bytes += data.second;
    };

    virtual void processBulkIn(byte_array data) {
        this->m_packets++;
        this->m_bytes += data.second;
    };

    virtual bool isEmpty() { return (this->m_controlPackets == 0) && (this->m_packets == 0); }

    // One line (or short block) summary of the interface.
    virtual std::string toString();

    // Verbose decode of everything the expert understood (commands, property lists, ...).  Empty when there is none.
    virtual std::string details() { return {}; }

    virtual std::vector<ExtractedArtifact> getArtifacts() { return {}; }

    [[nodiscard]] uint8_t getConfiguration() const { return this->m_configuration; }

    [[nodiscard]] uint8_t getInterface() const { return this->m_interface; }

    [[nodiscard]] uint32_t getControlPacketCount() const { return this->m_controlPackets; }

    [[nodiscard]] uint32_t getPacketCount() const { return this->m_packets; }

    [[nodiscard]] uint64_t getControlByteCount() const { return this->m_controlBytes; }

    [[nodiscard]] uint64_t getByteCount() const { return this->m_bytes; }

protected:
    uint8_t m_configuration;
    uint8_t m_interface;
    uint32_t m_controlPackets{};
    uint32_t m_packets{};
    uint64_t m_controlBytes{};
    uint64_t m_bytes{};
};


#endif //demuxusb_INTERFACEEXPERT_H
