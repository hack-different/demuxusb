//
// Created by Rick Mark on 11/16/20.
//

#include "DFUInterfaceExpert.h"

std::string DFUInterfaceExpert::toString() {
    auto format = boost::format("DFUInterfaceExpert (config=%1%, int=%2%) - %3% packets (%4% bytes)");

    return (format % (uint32_t)this->m_configuration % (uint32_t)this->m_interface % this->m_controlPackets % this->m_controlBytes).str();
}
