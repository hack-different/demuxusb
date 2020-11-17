//
// Created by Rick Mark on 11/14/20.
//

#ifndef DEUSBMUX_DEVICE_H
#define DEUSBMUX_DEVICE_H

#include "deusbmux.hpp"
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

    // The device class is a state machine that reconstructs the devices properties by composing the control endpoint
    //  packets, then de-multiplexes traffic to the various declared USB endpoints.  Those endpoints combined with the
    //  metadata from the control endpoint allow for endpoint experts to raise the traffic from there.
    class Device {
    public:
        explicit Device(uint64_t id, uint16_t vendorId, uint16_t productId) : m_device{id}, m_vendorId{vendorId}, m_productId{productId} {}

        uint64_t getIdentifier() { return this->m_device; }

        void processControlPacketIn(uint8_t endpoint, usb_setup_t setup, byte_array data) { this->m_controlPacketCount++; this->m_controlByteCount += data.second; }
        void processControlPacketOut(uint8_t endpoint, usb_setup_t setup, byte_array data) { this->m_controlPacketCount++; this->m_controlByteCount += data.second; }
        void processBulkPacketIn(uint8_t endpoint, byte_array data) { this->m_packetCount++; this->m_byteCount += data.second; }
        void processBulkPacketOut(uint8_t endpoint, byte_array data)  { this->m_packetCount++; this->m_byteCount += data.second; }

        [[nodiscard]] uint64_t getPacketCount() const { return this->m_packetCount; }
        [[nodiscard]] uint64_t getByteCount() const { return this->m_byteCount; }
        [[nodiscard]] uint64_t getControlPacketCount() const { return this->m_controlPacketCount; }
        [[nodiscard]] uint64_t getControlByteCount() const { return this->m_controlByteCount; }
    protected:
        uint64_t m_device;
        uint16_t m_vendorId;
        uint16_t m_productId;

        uint64_t m_packetCount{};
        uint64_t m_byteCount{};
        uint64_t m_controlPacketCount{};
        uint64_t m_controlByteCount{};

        usb_device_descriptor m_deviceDescriptor{};
        std::set<USBConfiguration> m_configurations;
        std::map<uint16_t, std::u16string> m_strings;

        usb_setup_t m_controlSetup{};

        std::map<uint8_t, std::shared_ptr<InterfaceExpert>> m_interfaces;
    };
}


#endif //DEUSBMUX_DEVICE_H
