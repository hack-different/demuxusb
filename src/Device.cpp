//
// Created by Rick Mark on 11/14/20.
//


#include "Device.h"

namespace deusbmux {


    void Device::processControlPacketIn(uint8_t endpoint, usb_setup_t setup, byte_array data) {
        this->m_controlPacketCount++;
        this->m_controlByteCount += data.second;

        if (USB_ENDPOINT_ID(endpoint) == 0) {
            if (setup.bmRequestType == 0x80 && setup.bRequest == 0x06) {
                // We are a descriptor read
                auto descriptorType = setup.wValue >> 8U;
                auto descriptorIndex = setup.wValue & 0xFFU;

                switch (descriptorType) {
                    case USB_DT_DEVICE:
                        break;
                    case USB_DT_STRING:
                        auto length = *data.first;
                        auto type = *(data.first + 1);
                        assert(static_cast<int>(type) == USB_DT_STRING);
                        if (static_cast<unsigned long>(length) != data.second) { break; }

                        this->m_strings[descriptorIndex] = std::u16string((char16_t*)(data.first+2), data.second/2);

                        break;
                }
            }
        }
    }

    void Device::processControlPacketOut(uint8_t endpoint, usb_setup_t setup, byte_array data) {
        this->m_controlPacketCount++;
        this->m_controlByteCount += data.second;

    }
}