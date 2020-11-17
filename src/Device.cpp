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
                auto* header = (usb_descriptor_header*)data.first;
                assert(header->bDescriptorType == descriptorType);

                switch (descriptorType) {
                    case USB_DT_DEVICE:
                        assert(data.second == sizeof(usb_device_descriptor));
                        memcpy(&this->m_deviceDescriptor, data.first, data.second);
                        assert(this->m_deviceDescriptor.bDescriptorType == USB_DT_DEVICE);
                        this->m_vendorId = this->m_deviceDescriptor.idVendor;
                        this->m_productId = this->m_deviceDescriptor.idProduct;
                        this->m_configurations = std::vector<usb_configuration>(
                                this->m_deviceDescriptor.bNumConfigurations);
                        break;
                    case USB_DT_CONFIG: {
                            // We only want to handle the full descriptor when returned
                            auto *configDescriptor = (usb_config_descriptor *) data.first;
                            auto configurationIndex = configDescriptor->bConfigurationValue - 1;
                            if (configDescriptor->wTotalLength != data.second) { break; }
                            auto config = usb_configuration{};
                            usb_configuration::parse(config, data.first, data.second);
                            this->m_configurations[configurationIndex] = config;
                        }
                        break;
                    case USB_DT_STRING: {
                            auto length = *data.first;
                            auto type = *(data.first + 1);
                            assert(static_cast<int>(type) == USB_DT_STRING);
                            if (static_cast<unsigned long>(length) != data.second) { break; }

                            auto *stringAddress = (char16_t *) (data.first + 2);
                            auto stringLength = (data.second - 2) / 2;

                            auto stringValue = std::u16string(stringAddress, stringLength);

                            this->m_strings[descriptorIndex] = std::wstring{std::begin(stringValue), std::end(stringValue)};
                        }
                        break;
                    default:
                        std::cerr << "Unknown descriptor type " << std::hex << descriptorType << std::endl;
                }
            }
        }
    }

    void Device::processControlPacketOut(uint8_t endpoint, usb_setup_t setup, byte_array data) {
        this->m_controlPacketCount++;
        this->m_controlByteCount += data.second;

    }

    void usb_configuration::parse(usb_configuration &config, std::byte *data, size_t size) {
        usb_interface interface{};
        auto *end = data + size;

        while (data < end) {
            auto *header = (usb_descriptor_header *) data;

            switch (header->bDescriptorType) {
                case USB_DT_CONFIG:
                    assert(header->bLength == sizeof(usb_config_descriptor));
                    memcpy(&config.descriptor, data, sizeof(usb_config_descriptor));
                    config.interfaces.resize(config.descriptor.bNumInterfaces);
                    break;
                case USB_DT_DFU_FUNCTIONAL:
                    assert(header->bLength == sizeof(usb_dfu_functional_descriptor));
                    memcpy(&config.dfu_descriptor, data, sizeof(usb_dfu_functional_descriptor));
                    break;
                    // TODO: Alternates
                case USB_DT_INTERFACE:
                    assert(header->bLength == sizeof(usb_interface_descriptor));
                    interface = usb_interface{};
                    memcpy(&interface.interface, data, sizeof(usb_interface_descriptor));
                    config.interfaces[interface.interface.bInterfaceNumber] = interface;
                    break;
                case USB_DT_BOS:

                default:
                    std::cerr << "Unknown configuration descriptor type " << std::hex << header->bDescriptorType << std::endl;
            }

            data += header->bLength;
        }
    }
}