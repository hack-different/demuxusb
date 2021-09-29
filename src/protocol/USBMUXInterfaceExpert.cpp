//
// Created by Rick Mark on 11/16/20.
//

#include "USBMUXInterfaceExpert.h"

std::string USBMUXInterfaceExpert::toString() {
    auto format = boost::format("USBMUXInterfaceExpert (configuration=%1%, interface=%2%):\n");
    auto stream_format = boost::format("\t-\t%1$x\t: %2% -> %3% (%4% transactions, in %5%, out %6% bytes)\n");

    std::string result = (format % (uint32_t)this->m_configuration % (uint32_t)this->m_interface).str();

    for (auto& stream : this->m_streams) {
        result.append((stream_format % stream.first % stream.second->getHostPort() %
        stream.second->getDevicePort() % stream.second->getTransactionCount() % stream.second->getBytesIn() %
        stream.second->getBytesOut()).str());
    }

    return result;
}

uint64_t USBMUXInterfaceExpert::beginStream(uint16_t host, uint16_t device) {
    uint32_t stream_port = STREAM_PORT(host, device);
    if (!this->m_streamIndex.contains(stream_port)) {
        this->m_streamIndex[stream_port] = 0;
    }

    auto index = this->m_streamIndex[stream_port];
    index++;
    this->m_streamIndex[stream_port] = index;

    return STREAM_ID(host, device, index);
}

void USBMUXInterfaceExpert::processBulkOut(byte_array data) {
    // Check that we at least have the basic header
    if (data.second >= sizeof(mux_header)) {
        auto* header = (mux_header*)data.first;


        switch (ntohl(header->protocol)) {
            case MUX_PROTO_TCP: {
                assert(ntohl(header->magic) == USBMUX_MAGIC_OUT);
                assert(data.second >= (sizeof(mux_header) + sizeof(tcphdr)));
                auto* tcp_header = (tcphdr*)(data.first + sizeof(mux_header));

                auto source = ntohs(tcp_header->source);
                auto destination = ntohs(tcp_header->dest);
                auto flags = ntohs(tcp_header->flags);

                // Magic evaluation for SYN on out (therefore connect)
                if (flags & USB_TCP_FLAG_SYN) {
                    auto stream_id = this->beginStream(source, destination);
                    this->m_streams[stream_id] = std::make_shared<MuxStream>(source, destination);
                }
                else {
                    auto stream_id = this->getStreamId(source, destination);
                    auto stream = this->m_streams[stream_id];

                    assert(stream != nullptr);
                    auto byte_length = data.second - (sizeof(mux_header) + sizeof(tcphdr));

                    stream->sendBytes(byte_length, data.first + byte_length);
                }
            }
                break;
            default:
                break;
        }


    }
}

void USBMUXInterfaceExpert::processBulkIn(byte_array data) {
    // Check that we at least have the basic header
    if (data.second >= sizeof(mux_header)) {
        auto* header = (mux_header*)data.first;

        switch (ntohl(header->protocol)) {
            case MUX_PROTO_TCP: {
                assert(ntohl(header->magic) == USBMUX_MAGIC_IN);
                assert(data.second >= (sizeof(mux_header) + sizeof(tcphdr)));
                auto *tcp_header = (tcphdr *) (data.first + sizeof(mux_header));

                auto source = ntohs(tcp_header->source);
                auto destination = ntohs(tcp_header->dest);
                auto flags = ntohs(tcp_header->flags);

                auto stream_id = this->getStreamId(destination, source);
                auto stream = this->m_streams[stream_id];

                assert(stream != nullptr);
                auto byte_length = data.second - (sizeof(mux_header) + sizeof(tcphdr));

                stream->receiveBytes(byte_length, data.first + byte_length);
            }
                break;
        }

    }
}

uint64_t USBMUXInterfaceExpert::getStreamId(uint16_t host, uint16_t device) {
    uint32_t stream_port = STREAM_PORT(host, device);

    auto index = this->m_streamIndex[stream_port];

    return STREAM_ID(host, device, index);
}
