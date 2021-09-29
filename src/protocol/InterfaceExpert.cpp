//
// Created by Rick Mark on 11/16/20.
//

#include "InterfaceExpert.h"

std::string InterfaceExpert::toString() {
    auto format = boost::format("InterfaceExpert (config=%1%, int=%2%) - %3% control packets (%4% bytes) and %5% packets (%6% bytes)");

    return (format % (uint32_t)this->m_configuration % (uint32_t)this->m_interface % this->m_controlPackets % this->m_controlBytes % this->m_packets % this->m_bytes).str();
}
