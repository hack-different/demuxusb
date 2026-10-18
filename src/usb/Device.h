//
// Created by Rick Mark on 11/14/20.
//

#ifndef demuxusb_DEVICE_H
#define demuxusb_DEVICE_H

#include "../demuxusb.hpp"
#include "usb.h"
#include "pcapng.h"
#include "../protocol/InterfaceExpert.h"


namespace demuxusb {

    struct bos_descriptor {
        usb_bos_descriptor descriptor;
        std::vector<std::byte> data;
    };

    // One alternate setting of an interface, with the endpoints it declares.  All alternate settings of an interface
    //  share a single expert.
    struct usb_interface {
        usb_interface_descriptor interface;
        std::vector<usb_endpoint_descriptor> endpoints;
        std::shared_ptr<InterfaceExpert> expert;
    };

    struct usb_configuration {
        usb_config_descriptor descriptor{};
        usb_dfu_functional_descriptor dfu_descriptor{};
        bool has_dfu_descriptor{false};
        // Every interface alternate setting, in descriptor order
        std::vector<usb_interface> interfaces{};

        // Parses a complete configuration descriptor set.  Returns false (leaving what could be parsed) when the
        //  descriptors are malformed.
        static bool parse(usb_configuration &config, const std::byte *data, size_t size);

        usb_interface *find(uint8_t interface, uint8_t alternate);
    };

    // The device class is a state machine that reconstructs the devices properties by composing the control endpoint
    //  packets, then de-multiplexes traffic to the various declared USB endpoints.  Those endpoints combined with the
    //  metadata from the control endpoint allow for endpoint experts to raise the traffic from there.
    class Device {
    public:
        explicit Device(uint64_t id) : m_device{id} {}

        [[nodiscard]] uint64_t getIdentifier() const { return this->m_device; }

        [[nodiscard]] bool hasDeviceDescriptor() const { return this->m_hasDeviceDescriptor; }

        [[nodiscard]] const usb_device_descriptor &getDeviceDescriptor() const { return this->m_deviceDescriptor; }

        [[nodiscard]] uint16_t getVendorId() const { return this->m_deviceDescriptor.idVendor; }

        [[nodiscard]] uint16_t getProductId() const { return this->m_deviceDescriptor.idProduct; }

        [[nodiscard]] bool isApple() const { return this->m_deviceDescriptor.idVendor == APPLE_VID; }

        [[nodiscard]] bool isAppleDFU() const {
            return this->m_deviceDescriptor.idVendor == APPLE_VID &&
                   (this->m_deviceDescriptor.idProduct == IRECV_K_DFU_MODE ||
                    this->m_deviceDescriptor.idProduct == IRECV_K_WTF_MODE);
        }

        [[nodiscard]] bool isAppleRecovery() const {
            if (this->m_deviceDescriptor.idVendor == APPLE_VID) {
                return ((this->m_deviceDescriptor.idProduct == IRECV_K_RECOVERY_MODE_1) ||
                        (this->m_deviceDescriptor.idProduct == IRECV_K_RECOVERY_MODE_2) ||
                        (this->m_deviceDescriptor.idProduct == IRECV_K_RECOVERY_MODE_3) ||
                        (this->m_deviceDescriptor.idProduct == IRECV_K_RECOVERY_MODE_4));
            }
            return false;
        }

        // "DFU", "WTF", "Recovery", "Normal" (any other Apple device) or "Unknown"
        [[nodiscard]] std::string getMode() const;

        // UTF-8 decoded string descriptor, or "<unknown>"
        std::string getString(uint8_t index) {
            if (this->m_strings.contains(index)) {
                return this->m_strings[index];
            }

            return "<unknown>";
        }

        std::string getProduct() { return this->getString(this->m_deviceDescriptor.iProduct); }

        std::string getManufacturer() { return this->getString(this->m_deviceDescriptor.iManufacturer); }

        std::string getSerial() { return this->getString(this->m_deviceDescriptor.iSerialNumber); }

        void processControlPacketIn(uint8_t endpoint, usb_setup_t setup, byte_array data);

        void processControlPacketOut(uint8_t endpoint, usb_setup_t setup, byte_array data);

        void processBulkPacketIn(uint8_t endpoint, byte_array data);

        void processBulkPacketOut(uint8_t endpoint, byte_array data);

        [[nodiscard]] uint64_t getPacketCount() const { return this->m_packetCount; }

        [[nodiscard]] uint64_t getByteCount() const { return this->m_byteCount; }

        [[nodiscard]] uint64_t getControlPacketCount() const { return this->m_controlPacketCount; }

        [[nodiscard]] uint64_t getControlByteCount() const { return this->m_controlByteCount; }

        [[nodiscard]] uint8_t getCurrentConfiguration() const { return this->m_currentConfiguration; }

        [[nodiscard]] uint8_t getAlternateSetting(uint8_t interface) const {
            auto found = this->m_alternates.find(interface);
            return found == this->m_alternates.end() ? 0 : found->second;
        }

        [[nodiscard]] const std::map<uint8_t, usb_configuration> &getConfigurations() const {
            return this->m_configurations;
        }

        [[nodiscard]] const std::vector<bos_descriptor> &getBOSDescriptors() const { return this->m_bosDescriptors; }

        // Every distinct expert attached to any interface of any configuration
        std::vector<std::shared_ptr<InterfaceExpert>> getExperts();

        // Descriptor or protocol irregularities observed on the default control pipe
        [[nodiscard]] const std::vector<std::string> &getAnomalies() const { return this->m_anomalies; }

    protected:
        void processDescriptor(usb_setup_t setup, byte_array data);

        void attachExperts(usb_configuration &config);

        usb_configuration *activeConfiguration();

        std::shared_ptr<InterfaceExpert> getExpertForEndpoint(uint8_t endpoint);

        std::shared_ptr<InterfaceExpert> getExpertForControl(const usb_setup_t &setup);

        uint64_t m_device;

        uint64_t m_packetCount{};
        uint64_t m_byteCount{};
        uint64_t m_controlPacketCount{};
        uint64_t m_controlByteCount{};

        bool m_hasDeviceDescriptor{false};
        uint8_t m_currentConfiguration{};
        usb_device_descriptor m_deviceDescriptor{};
        std::map<uint8_t, usb_configuration> m_configurations{};
        std::map<uint8_t, uint8_t> m_alternates{};
        std::map<uint8_t, std::string> m_strings;
        std::vector<bos_descriptor> m_bosDescriptors{};
        std::vector<std::string> m_anomalies{};
    };
}


#endif //demuxusb_DEVICE_H
