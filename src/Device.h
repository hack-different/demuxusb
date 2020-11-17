//
// Created by Rick Mark on 11/14/20.
//

#ifndef DEUSBMUX_DEVICE_H
#define DEUSBMUX_DEVICE_H

#include "deusbmux.hpp"
#include "InterfaceExpert.h"

namespace deusbmux {

    struct bos_descriptor {
        usb_bos_descriptor descriptor;
        std::vector<std::byte> data;
    };

    struct usb_interface {
        usb_interface_descriptor interface;
        std::vector<usb_endpoint_descriptor> endpoints;
        std::shared_ptr<InterfaceExpert> expert;
    };

    struct usb_configuration {
        usb_config_descriptor descriptor;
        usb_dfu_functional_descriptor dfu_descriptor;
        std::vector<usb_interface> interfaces{};

        static void parse(usb_configuration& config, std::byte *data, size_t size);
    };

    // The device class is a state machine that reconstructs the devices properties by composing the control endpoint
    //  packets, then de-multiplexes traffic to the various declared USB endpoints.  Those endpoints combined with the
    //  metadata from the control endpoint allow for endpoint experts to raise the traffic from there.
    class Device {
    public:
        explicit Device(uint64_t id) : m_device{id}, m_vendorId{}, m_productId{} {}

        [[nodiscard]] uint64_t getIdentifier() const { return this->m_device; }

        [[nodiscard]] bool isApple() const { return this->m_deviceDescriptor.idVendor == APPLE_VID; }
        [[nodiscard]] bool isAppleDFU() const { return this->m_deviceDescriptor.idVendor == APPLE_VID && this->m_deviceDescriptor.idProduct == IRECV_K_DFU_MODE; }
        [[nodiscard]] bool isAppleRecovery() const {
            if (this->m_deviceDescriptor.idVendor == APPLE_VID) {
                return ((this->m_deviceDescriptor.idProduct == IRECV_K_RECOVERY_MODE_1) ||
                        (this->m_deviceDescriptor.idProduct == IRECV_K_RECOVERY_MODE_2) ||
                        (this->m_deviceDescriptor.idProduct == IRECV_K_RECOVERY_MODE_3) ||
                        (this->m_deviceDescriptor.idProduct == IRECV_K_RECOVERY_MODE_4));
            }
            return false;
        }

        std::wstring getString(uint8_t index) {
            if (this->m_strings.contains(index)) {
                return this->m_strings[index];
            }

            return L"<unknown>";
        }

        std::wstring getProduct() { return this->getString(this->m_deviceDescriptor.iProduct); }

        std::wstring getManufacturer() { return this->getString(this->m_deviceDescriptor.iManufacturer); }

        std::wstring getSerial() { return this->getString(this->m_deviceDescriptor.iSerialNumber); }

        void processControlPacketIn(uint8_t endpoint, usb_setup_t setup, byte_array data);

        void processControlPacketOut(uint8_t endpoint, usb_setup_t setup, byte_array data);

        void processBulkPacketIn(uint8_t endpoint, byte_array data) {
            this->m_packetCount++;
            this->m_byteCount += data.second;
        }

        void processBulkPacketOut(uint8_t endpoint, byte_array data) {
            this->m_packetCount++;
            this->m_byteCount += data.second;
        }

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

        uint8_t m_currentConfiguration{};
        usb_device_descriptor m_deviceDescriptor{};
        std::vector<usb_configuration> m_configurations{};
        std::map<uint16_t, std::wstring> m_strings;
        std::vector<bos_descriptor> m_bosDescriptors{};

        usb_setup_t m_controlSetup{};

        std::map<uint8_t, std::shared_ptr<InterfaceExpert>> m_interfaces;
    };
}


#endif //DEUSBMUX_DEVICE_H
