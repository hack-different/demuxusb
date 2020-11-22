//
// Created by Rick Mark on 11/16/20.
//

#include "USBMUXInterfaceExpert.h"

std::string USBMUXInterfaceExpert::toString() {
    auto format = boost::format("USBMUXInterfaceExpert (config=%1%, int=%2%):\n");
    auto stream_format = boost::format("-\tStream %1%\t: from host %2% to device %3% (%4% bytes)\n");

    std::string result = (format % this->m_configuration % this->m_interface).str();

    for (auto& stream : this->m_streams) {
        result.append((stream_format % stream.first % stream.second->getHostPort() %
        stream.second->getDevicePort() % stream.second->getBytes()).str());
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
