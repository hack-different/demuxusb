//
// Created by Rick Mark on 11/14/20.
//

#ifndef DEUSBMUX_DEVICE_H
#define DEUSBMUX_DEVICE_H

#include <pcapng.h>
#include <usb.h>
#include <set>
#include <map>
#include <functional>
#include <string>

#include "InterfaceExpert.h"

namespace deusbmux {

    struct usb_interface_descriptor_comparator : std::binary_function<usb_interface_descriptor, usb_interface_descriptor, bool> {
        bool operator() (const usb_interface_descriptor& x, const usb_interface_descriptor& y) const { return x.bInterfaceNumber == y.bInterfaceNumber; }
    };

    struct USBConfiguration  {
        usb_config_descriptor descriptor;
        std::set<usb_interface_descriptor, usb_interface_descriptor_comparator> interfaces;

        bool operator==(const USBConfiguration& other) const { return this->descriptor.bConfigurationValue == other.descriptor.bConfigurationValue; }
    };

    struct USBControlState {
        uint8_t requestType;
        uint8_t index;
    };

    // The device class is a state machine that reconstructs the devices properties by composing the control endpoint
    //  packets, then de-multiplexes traffic to the various declared USB endpoints.  Those endpoints combined with the
    //  metadata from the control endpoint allow for endpoint experts to raise the traffic from there.
    class Device {
    public:
        explicit Device(uint32_t id);

        void processPacket(void* data);
        [[nodiscard]] uint64_t getPacketCount() const;
    protected:
        uint32_t m_device;
        uint64_t m_packetCount;

        usb_device_descriptor m_deviceDescriptor;
        std::set<USBConfiguration> m_configurations;
        std::map<uint16_t, std::u16string> m_strings;

        USBControlState m_state;

        std::map<uint8_t, std::shared_ptr<InterfaceExpert>> m_interfaces;
    };
}


#endif //DEUSBMUX_DEVICE_H
