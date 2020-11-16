//
// Created by Rick Mark on 11/14/20.
//

#ifndef DEUSBMUX_CDEVICE_H
#define DEUSBMUX_CDEVICE_H

#include <pcapng.h>
#include <usb.h>
#include <set>
#include <functional>

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

        void processPacket(pcap_usb_header_t* header, void* data);
    protected:
        uint32_t m_device;

        usb_device_descriptor m_deviceDescriptor;
        std::set<USBConfiguration> m_configurations;

        USBControlState m_state;
    };
}


#endif //DEUSBMUX_CDEVICE_H
