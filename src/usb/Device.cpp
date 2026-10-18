//
// Created by Rick Mark on 11/14/20.
//


#include "Device.h"
#include "../protocol/DFUInterfaceExpert.h"
#include "../protocol/RecoveryInterfaceExpert.h"
#include "../protocol/USBMUXInterfaceExpert.h"

#define USB_CLASS_DFU USB_CLASS_APP_SPEC

namespace demuxusb {
    std::string Device::getMode() const {
        if (!this->m_hasDeviceDescriptor) { return "Unknown"; }
        if (this->isApple()) {
            if (this->m_deviceDescriptor.idProduct == IRECV_K_DFU_MODE) { return "DFU"; }
            if (this->m_deviceDescriptor.idProduct == IRECV_K_WTF_MODE) { return "WTF"; }
            if (this->isAppleRecovery()) { return "Recovery"; }
            return "Normal";
        }
        return "Unknown";
    }

    void Device::processBulkPacketIn(uint8_t endpoint, byte_array data) {
        endpoint |= 0x80U;
        this->m_packetCount++;
        this->m_byteCount += data.second;

        auto expert = this->getExpertForEndpoint(endpoint);

        if (expert != nullptr) {
            expert->processBulkIn(data);
        }
    }

    void Device::processBulkPacketOut(uint8_t endpoint, byte_array data) {
        endpoint = USB_ENDPOINT_ID(endpoint);
        this->m_packetCount++;
        this->m_byteCount += data.second;

        auto expert = this->getExpertForEndpoint(endpoint);

        if (expert != nullptr) {
            expert->processBulkOut(data);
        }
    }

    void Device::processControlPacketIn(uint8_t endpoint, usb_setup_t setup, byte_array data) {
        this->m_controlPacketCount++;
        this->m_controlByteCount += data.second;

        if (USB_ENDPOINT_ID(endpoint) != 0) { return; }

        if ((setup.bmRequestType & USB_TYPE_MASK) == USB_TYPE_STANDARD) {
            if (setup.bRequest == USB_REQ_GET_DESCRIPTOR && (setup.bmRequestType & USB_RECIP_MASK) == USB_RECIP_DEVICE) {
                this->processDescriptor(setup, data);
            }
            return;
        }

        auto expert = this->getExpertForControl(setup);
        if (expert != nullptr) {
            expert->processControlIn(setup, data);
        }
    }

    void Device::processControlPacketOut(uint8_t endpoint, usb_setup_t setup, byte_array data) {
        this->m_controlPacketCount++;
        this->m_controlByteCount += data.second;

        if (USB_ENDPOINT_ID(endpoint) != 0) { return; }

        if ((setup.bmRequestType & USB_TYPE_MASK) == USB_TYPE_STANDARD) {
            auto recipient = setup.bmRequestType & USB_RECIP_MASK;
            if (recipient == USB_RECIP_DEVICE && setup.bRequest == USB_REQ_SET_CONFIGURATION) {
                this->m_currentConfiguration = setup.wValue & 0xFFU;
                this->m_alternates.clear();
            } else if (recipient == USB_RECIP_INTERFACE && setup.bRequest == USB_REQ_SET_INTERFACE) {
                this->m_alternates[setup.wIndex & 0xFFU] = setup.wValue & 0xFFU;
            }
            return;
        }

        auto expert = this->getExpertForControl(setup);
        if (expert != nullptr) {
            expert->processControlOut(setup, data);
        }
    }

    void Device::processDescriptor(usb_setup_t setup, byte_array data) {
        auto descriptorType = setup.wValue >> 8U;
        auto descriptorIndex = setup.wValue & 0xFFU;

        usb_descriptor_header header{};
        if (!readStruct(data.first, data.second, header)) { return; }

        if (header.bDescriptorType != descriptorType) {
            this->m_anomalies.push_back((boost::format("GET_DESCRIPTOR(0x%1$02x) returned descriptor type 0x%2$02x") %
                                         descriptorType % (uint32_t) header.bDescriptorType).str());
            return;
        }

        switch (descriptorType) {
            case USB_DT_DEVICE:
                // Hosts commonly read the first 8 bytes for bMaxPacketSize0 before the full descriptor
                if (data.second < USB_DT_DEVICE_SIZE) { break; }
                readStruct(data.first, data.second, this->m_deviceDescriptor);
                this->m_hasDeviceDescriptor = true;
                break;

            case USB_DT_CONFIG: {
                usb_config_descriptor configDescriptor{};
                if (!readStruct(data.first, data.second, configDescriptor)) { break; }
                // Only handle the full descriptor set, not the initial 9 byte read for wTotalLength
                if (data.second < configDescriptor.wTotalLength || configDescriptor.wTotalLength <= USB_DT_CONFIG_SIZE) {
                    break;
                }

                auto config = usb_configuration{};
                if (!usb_configuration::parse(config, data.first, configDescriptor.wTotalLength)) {
                    this->m_anomalies.push_back((boost::format("Malformed configuration descriptor %1%") %
                                                 (uint32_t) configDescriptor.bConfigurationValue).str());
                }

                auto value = config.descriptor.bConfigurationValue;
                if (this->m_configurations.contains(value)) {
                    // Re-reading a configuration keeps the experts (and what they have learned) already attached
                    break;
                }

                this->attachExperts(config);
                this->m_configurations[value] = std::move(config);
            }
                break;

            case USB_DT_STRING: {
                // String zero is the supported LANGID list, not a string
                if (descriptorIndex == 0) { break; }
                if (header.bLength < 2 || data.second < header.bLength) { break; }

                this->m_strings[descriptorIndex] = utf16leToUtf8(data.first + 2, header.bLength - 2);
            }
                break;

            case USB_DT_BOS: {
                usb_bos_descriptor bos_descriptor{};
                if (!readStruct(data.first, data.second, bos_descriptor)) { break; }
                // As with configurations the host first reads the header alone
                if (bos_descriptor.wTotalLength < USB_DT_BOS_SIZE || data.second < bos_descriptor.wTotalLength) {
                    break;
                }
                if (bos_descriptor.wTotalLength == USB_DT_BOS_SIZE && bos_descriptor.bNumDeviceCaps != 0) { break; }

                struct bos_descriptor bos{bos_descriptor, {}};
                bos.data.assign(data.first + USB_DT_BOS_SIZE, data.first + bos_descriptor.wTotalLength);
                this->m_bosDescriptors.push_back(std::move(bos));
                break;
            }

            default:
                break;
        }
    }

    void Device::attachExperts(usb_configuration &config) {
        auto value = config.descriptor.bConfigurationValue;

        // DFU and recovery devices speak a single protocol across all interfaces; share one expert
        std::shared_ptr<InterfaceExpert> deviceExpert;
        if (this->isAppleDFU()) {
            auto dfu = std::make_shared<DFUInterfaceExpert>(value, 0);
            if (config.has_dfu_descriptor) { dfu->setTransferSize(config.dfu_descriptor.wTransferSize); }
            deviceExpert = dfu;
        } else if (this->isAppleRecovery()) {
            deviceExpert = std::make_shared<RecoveryInterfaceExpert>(value, 0);
        }

        std::map<uint8_t, std::shared_ptr<InterfaceExpert>> byInterface;

        for (auto &interface: config.interfaces) {
            const auto &descriptor = interface.interface;
            auto number = descriptor.bInterfaceNumber;

            if (deviceExpert != nullptr) {
                interface.expert = deviceExpert;
                continue;
            }

            if (byInterface.contains(number)) {
                interface.expert = byInterface[number];
                continue;
            }

            std::shared_ptr<InterfaceExpert> expert;
            if (this->isApple() &&
                descriptor.bInterfaceClass == USB_CLASS_APPLICATION_SPECIFIC &&
                descriptor.bInterfaceSubClass == APPLE_SUBCLASS_USBMUX &&
                descriptor.bInterfaceProtocol == APPLE_PROTOCOL_USBMUX2) {
                expert = std::make_shared<USBMUXInterfaceExpert>(value, number);
            } else if (descriptor.bInterfaceClass == USB_CLASS_DFU && descriptor.bInterfaceSubClass == USB_SUBCLASS_DFU) {
                auto dfu = std::make_shared<DFUInterfaceExpert>(value, number);
                if (config.has_dfu_descriptor) { dfu->setTransferSize(config.dfu_descriptor.wTransferSize); }
                expert = dfu;
            }

            if (expert != nullptr) {
                byInterface[number] = expert;
                // Earlier alternate settings of this interface share the expert too
                for (auto &other: config.interfaces) {
                    if (other.interface.bInterfaceNumber == number) { other.expert = expert; }
                }
            }
        }
    }

    std::vector<std::shared_ptr<InterfaceExpert>> Device::getExperts() {
        auto result = std::vector<std::shared_ptr<InterfaceExpert>>();
        std::set<InterfaceExpert *> seen;

        for (const auto &[value, configuration]: this->m_configurations) {
            for (const auto &interface: configuration.interfaces) {
                if (interface.expert != nullptr && !seen.contains(interface.expert.get())) {
                    seen.insert(interface.expert.get());
                    result.push_back(interface.expert);
                }
            }
        }

        return result;
    }

    usb_configuration *Device::activeConfiguration() {
        if (this->m_configurations.empty()) { return nullptr; }

        auto found = this->m_configurations.find(this->m_currentConfiguration);
        if (found != this->m_configurations.end()) { return &found->second; }

        // SET_CONFIGURATION may predate the capture.  With a single configuration there is no ambiguity.
        if (this->m_currentConfiguration == 0 && this->m_configurations.size() == 1) {
            return &this->m_configurations.begin()->second;
        }

        return nullptr;
    }

    std::shared_ptr<InterfaceExpert> Device::getExpertForEndpoint(uint8_t endpoint) {
        auto *config = this->activeConfiguration();
        if (config == nullptr) { return nullptr; }

        std::shared_ptr<InterfaceExpert> fallback;
        for (const auto &interface: config->interfaces) {
            for (const auto &interface_endpoint: interface.endpoints) {
                if (interface_endpoint.bEndpointAddress != endpoint) { continue; }

                if (interface.interface.bAlternateSetting ==
                    this->getAlternateSetting(interface.interface.bInterfaceNumber)) {
                    return interface.expert;
                }
                // SET_INTERFACE may predate the capture; remember the first alternate declaring the endpoint
                if (fallback == nullptr) { fallback = interface.expert; }
            }
        }

        return fallback;
    }

    std::shared_ptr<InterfaceExpert> Device::getExpertForControl(const usb_setup_t &setup) {
        auto *config = this->activeConfiguration();
        if (config == nullptr) {
            // Class and vendor requests can precede SET_CONFIGURATION (DFU / recovery devices in particular)
            if (this->m_configurations.empty()) { return nullptr; }
            config = &this->m_configurations.begin()->second;
        }

        if ((setup.bmRequestType & USB_RECIP_MASK) == USB_RECIP_INTERFACE) {
            auto number = setup.wIndex & 0xFFU;
            for (const auto &interface: config->interfaces) {
                if (interface.interface.bInterfaceNumber == number) { return interface.expert; }
            }
            return nullptr;
        }

        // Device recipient vendor / class requests belong to the first interface with an expert
        for (const auto &interface: config->interfaces) {
            if (interface.expert != nullptr) { return interface.expert; }
        }
        return nullptr;
    }

    usb_interface *usb_configuration::find(uint8_t interface, uint8_t alternate) {
        for (auto &candidate: this->interfaces) {
            if (candidate.interface.bInterfaceNumber == interface && candidate.interface.bAlternateSetting == alternate) {
                return &candidate;
            }
        }
        return nullptr;
    }

    bool usb_configuration::parse(usb_configuration &config, const std::byte *data, size_t size) {
        const auto *end = data + size;
        usb_interface *current = nullptr;

        while (data < end) {
            usb_descriptor_header header{};
            auto remaining = static_cast<size_t>(end - data);
            if (!readStruct(data, remaining, header) || header.bLength < sizeof(usb_descriptor_header) ||
                header.bLength > remaining) {
                return false;
            }

            switch (header.bDescriptorType) {
                case USB_DT_CONFIG:
                    if (header.bLength < USB_DT_CONFIG_SIZE) { return false; }
                    readStruct(data, remaining, config.descriptor);
                    break;

                case USB_DT_INTERFACE: {
                    if (header.bLength < USB_DT_INTERFACE_SIZE) { return false; }
                    usb_interface interface{};
                    readStruct(data, remaining, interface.interface);
                    config.interfaces.push_back(std::move(interface));
                    current = &config.interfaces.back();
                    break;
                }

                case USB_DT_ENDPOINT: {
                    if (header.bLength < USB_DT_ENDPOINT_SIZE) { return false; }
                    if (current == nullptr) { break; }
                    usb_endpoint_descriptor endpoint{};
                    std::memcpy(&endpoint, data, std::min<size_t>(header.bLength, sizeof(endpoint)));
                    current->endpoints.push_back(endpoint);
                    break;
                }

                case USB_DT_DFU_FUNCTIONAL:
                    // 0x21 is also the HID descriptor type; only treat it as DFU inside a DFU interface
                    if (current != nullptr && current->interface.bInterfaceClass == USB_CLASS_DFU &&
                        current->interface.bInterfaceSubClass == USB_SUBCLASS_DFU) {
                        std::memcpy(&config.dfu_descriptor, data,
                                    std::min<size_t>(header.bLength, sizeof(config.dfu_descriptor)));
                        config.has_dfu_descriptor = true;
                    }
                    break;

                default:
                    // Interface association, class specific and vendor descriptors are not needed for routing
                    break;
            }

            data += header.bLength;
        }

        return true;
    }
}
