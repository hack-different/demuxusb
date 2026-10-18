//
// Created by Rick Mark on 11/16/20.
//

#ifndef demuxusb_USBMUXINTERFACEEXPERT_H
#define demuxusb_USBMUXINTERFACEEXPERT_H

#include "../demuxusb.hpp"
#include "usbmux.h"
#include "InterfaceExpert.h"
#include "MuxStream.h"

#define STREAM_PORT(host, device) (((uint32_t)host << 16U) | (uint32_t)device)
#define STREAM_ID(host, device, index) ((((uint64_t)STREAM_PORT(host, device)) << 32U) | (uint64_t)index)

// Demultiplexes the usbmux protocol carried on the Apple USBMUX bulk endpoints into the TCP streams it tunnels.
//  usbmux packets may be split across, or packed into, USB transfers so each direction is reassembled by the
//  length in the usbmux header before being parsed.
class USBMUXInterfaceExpert : public InterfaceExpert {
public:
    USBMUXInterfaceExpert(uint8_t configuration, uint8_t interface) : InterfaceExpert(configuration, interface) {};

    std::string toString() override;

    std::string details() override;

    std::vector<ExtractedArtifact> getArtifacts() override;

    bool isEmpty() override { return this->m_streams.empty() && InterfaceExpert::isEmpty(); }

    void processBulkOut(byte_array data) override;

    void processBulkIn(byte_array data) override;

    [[nodiscard]] const std::map<uint64_t, std::shared_ptr<MuxStream>> &getStreams() const { return this->m_streams; }

    [[nodiscard]] uint32_t getProtocolVersion() const { return this->m_version; }

    [[nodiscard]] uint64_t getResyncCount() const { return this->m_resyncs; }

protected:
    struct Direction {
        bool outbound;
        std::vector<std::byte> buffer{};
    };

    uint64_t beginStream(uint16_t host, uint16_t device);

    uint64_t getStreamId(uint16_t host, uint16_t device);

    std::shared_ptr<MuxStream> streamFor(uint16_t host, uint16_t device);

    void feed(Direction &direction, byte_array data);

    void processPacket(bool outbound, uint32_t protocol, std::byte *payload, size_t length);

    void processTCP(bool outbound, std::byte *payload, size_t length);

    void learnServices(const std::shared_ptr<MuxStream> &stream, size_t firstNewMessage);

private:
    Direction m_in{false};
    Direction m_out{true};
    uint32_t m_version{};
    uint64_t m_resyncs{};
    std::map<uint32_t, uint32_t> m_streamIndex{};
    std::map<uint64_t, std::shared_ptr<MuxStream>> m_streams{};
    std::map<uint16_t, std::string> m_serviceNames{};
};


#endif //demuxusb_USBMUXINTERFACEEXPERT_H
