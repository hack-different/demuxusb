//
// Created by Rick Mark on 11/14/20.
//

#ifndef demuxusb_CMUXSTREAM_H
#define demuxusb_CMUXSTREAM_H

#include "../demuxusb.hpp"

struct plist_service_item {
    uint32_t size{};
    uint32_t received{};
    std::byte *data{};
    plist_t plist{};
};

class MuxStream {
public:
    MuxStream(uint16_t host, uint16_t device) : m_host{host}, m_device{device} {};

    ~MuxStream();

    uint16_t getHostPort() { return this->m_host; }

    uint16_t getDevicePort() { return this->m_device; }

    uint64_t getBytesIn() { return this->m_bytesIn; }

    uint64_t getBytesOut() { return this->m_bytesOut; }

    size_t getTransactionCount() { return this->m_inItems.size() + this->m_outItems.size(); }

    bool shouldMaterializePlist() { return this->getDevicePort() != 12345; }

    void receiveBytes(size_t length, std::byte *start);

    void sendBytes(size_t length, std::byte *start);

protected:
    void materialize(plist_service_item &item);

    size_t handleBytes(plist_service_item &item, size_t length, std::byte *data);

    uint16_t m_host;
    uint16_t m_device;
    uint64_t m_bytesIn{};
    uint64_t m_bytesOut{};
    bool m_isPlistService{true};

    std::vector<std::unique_ptr<plist_service_item>> m_inItems{};
    std::vector<std::unique_ptr<plist_service_item>> m_outItems{};
};


#endif //demuxusb_CMUXSTREAM_H
