//
// Created by Rick Mark on 11/16/20.
//

#ifndef demuxusb_USBMUXINTERFACEEXPERT_H
#define demuxusb_USBMUXINTERFACEEXPERT_H

#include "../demuxusb.hpp"
#include "InterfaceExpert.h"
#include "MuxStream.h"

#define STREAM_PORT(host, device) (((uint32_t)host << 16U) | (uint32_t)device)
#define STREAM_ID(host, device, index) ((((uint64_t)STREAM_PORT(host, device)) << 32U) | (uint64_t)index)

class USBMUXInterfaceExpert : public InterfaceExpert {
public:
    USBMUXInterfaceExpert(uint8_t configuration, uint8_t interface) : InterfaceExpert(configuration, interface) {};
    std::string toString() override;
    bool isEmpty() override { return this->m_streams.empty(); }
protected:
    uint64_t beginStream(uint16_t host, uint16_t device);
private:
    std::map<uint32_t, uint32_t> m_streamIndex{};
    std::map<uint64_t, std::shared_ptr<MuxStream>> m_streams{};
};


#endif //demuxusb_USBMUXINTERFACEEXPERT_H
