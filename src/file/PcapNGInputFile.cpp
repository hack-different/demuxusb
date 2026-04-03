//
// Created by Rick Mark on 11/15/20.
//

#include "PcapNGInputFile.h"


namespace demuxusb {
    bool PcapNGInputFile::process_dawrin_packet(std::byte *location) {
        auto *usbHeader = reinterpret_cast<darwin_usb_header_1_1_t *>(location + sizeof(block_t) + sizeof(pcapng_epb_t));
        assert(usbHeader->version == 0x101);
        assert(usbHeader->headerLength == sizeof(darwin_usb_header_1_1_t));

        auto deviceAddress = (static_cast<uint64_t>(usbHeader->locationId) << 32U) | usbHeader->deviceId;

        auto *dataAddress = (location + sizeof(block_t) + sizeof(pcapng_epb_t) + usbHeader->headerLength);
        const auto data = byte_array{dataAddress, usbHeader->length};
        const auto endpointId = (usb_endpoint_id) usbHeader->endpointId;

        auto &device = this->m_devices[deviceAddress];

        if (device == nullptr) {
            device = std::make_shared<DeviceHandle>(deviceAddress);
            this->m_devices[deviceAddress] = device;
        }

        switch (usbHeader->endpointType) {
            case USB_EP_CONTROL:
                if (usbHeader->type == DARWIN_IO_SUBMIT) {
                    auto *setup = reinterpret_cast<usb_setup_t *>(data.first);
                    device->beginControl(USB_ENDPOINT_ID(usbHeader->endpointId), usbHeader->uniqueId, setup);
                } else if (usbHeader->type == DARWIN_IO_COMPLETE) {
                    auto setup = device->endControl(USB_ENDPOINT_ID(usbHeader->endpointId),
                                                    usbHeader->uniqueId);

                    if (USB_ENDPOINT_IN(usbHeader->endpointId)) {
                        device->device()->processControlPacketIn(USB_ENDPOINT_ID(usbHeader->endpointId), setup,
                                                                 data);
                    } else {
                        device->device()->processControlPacketOut(USB_ENDPOINT_ID(usbHeader->endpointId), setup,
                                                                  data);
                    }
                }
                break;
            case USB_EP_BULK:
                if (usbHeader->type == DARWIN_IO_SUBMIT) {
                    device->beginBulk(usbHeader->endpointId, usbHeader->uniqueId);
                } else {
                    device->endBulk(usbHeader->endpointId, usbHeader->uniqueId);

                    if (USB_ENDPOINT_IN(usbHeader->endpointId)) {
                        device->device()->processBulkPacketIn(USB_ENDPOINT_ID(usbHeader->endpointId), data);
                    } else {
                        device->device()->processBulkPacketOut(USB_ENDPOINT_ID(usbHeader->endpointId), data);
                    }
                }
                break;
            default: ;
        }
        return true;
    }

    bool PcapNGInputFile::process_usbpcap_packet(std::byte *location) {

        auto *usbHeader = reinterpret_cast<usbpcap_packet_header_t *>(location + sizeof(block_t) + sizeof(pcapng_epb_t));
        if (usbHeader->transfer ==  USBPCAP_TRANSFER_CONTROL) {
            assert(usbHeader->headerLen == sizeof(usbpcap_buffer_control_header_t));
        } else {
            assert(usbHeader->headerLen == sizeof(usbpcap_packet_header_t));
        }

        auto deviceAddress = usbHeader->device;

        auto *dataAddress = (location + sizeof(block_t) + sizeof(pcapng_epb_t) + usbHeader->headerLen);
        const auto data = byte_array{dataAddress, usbHeader->dataLength};
        const auto endpointId = (usb_endpoint_id) usbHeader->endpoint;

        auto &device = this->m_devices[deviceAddress];

        if (device == nullptr) {
            device = std::make_shared<DeviceHandle>(deviceAddress);
            this->m_devices[deviceAddress] = device;
        }
        auto *controlHeader = reinterpret_cast<usbpcap_buffer_control_header_t *>(location + sizeof(block_t) + sizeof(pcapng_epb_t));

        switch (usbHeader->transfer) {
            case USBPCAP_TRANSFER_CONTROL:

                if (controlHeader->stage == USBPCAP_CONTROL_STAGE_SETUP) {
                    auto *setup = reinterpret_cast<usb_setup_t *>(data.first);
                    device->beginControl(USB_ENDPOINT_ID(usbHeader->endpoint), usbHeader->irpId, setup);
                } else if (controlHeader->stage == USBPCAP_CONTROL_STAGE_DATA) {
                    auto setup = device->endControl(USB_ENDPOINT_ID(usbHeader->endpoint),
                                                    usbHeader->irpId);

                    if (USB_ENDPOINT_IN(usbHeader->endpoint)) {
                        device->device()->processControlPacketIn(USB_ENDPOINT_ID(usbHeader->endpoint), setup,
                                                                 data);
                    } else {
                        device->device()->processControlPacketOut(USB_ENDPOINT_ID(usbHeader->endpoint), setup,
                                                                  data);
                    }
                }
                break;
            case USBPCAP_TRANSFER_BULK:
                device->beginBulk(usbHeader->endpoint, usbHeader->irpId);
                device->endBulk(usbHeader->endpoint, usbHeader->irpId);

                auto endpoint_id = USB_ENDPOINT_ID(usbHeader->endpoint);
                if (USB_ENDPOINT_IN(usbHeader->endpoint)) {
                    device->device()->processBulkPacketIn(endpoint_id, data);
                } else {
                    device->device()->processBulkPacketOut(endpoint_id, data);
                }
        }
        return true;

    }

    void PcapNGInputFile::parse() {

        auto *location = (std::byte *) this->m_mappedInputFile.data();
        const auto *endLocation = location + this->m_mappedInputFile.size();

        uint16_t interface_type;

        while (location < endLocation) {
            auto *block = reinterpret_cast<block_t *>(location);
            if (block->type == TIME_INTERFACE_DESCRIPTOR_BLOCK) {
                auto *interfaceDescriptor = reinterpret_cast<pcap_interface_descriptor_t *>(location + sizeof(block_t));

                interface_type = interfaceDescriptor->linktype;
                location += block->length;
                continue;
            }

            if (block->type == TYPE_ENHANCED_PACKET_BLOCK) {
                auto *epb = reinterpret_cast<pcapng_epb_t *>(location + sizeof(block_t));
                assert(epb->interface_id == 0);

                switch (interface_type) {
                    case LINKTYPE_USB_DARWIN:
                        process_dawrin_packet(location);
                        break;

                    case LINKTYPE_USBPCAP:
                        process_usbpcap_packet(location);
                        break;
                    default:
                        continue;
                }

            }

            auto *trailingSize = reinterpret_cast<uint32_t *>(location + block->length - sizeof(uint32_t));
            if (block->length != *trailingSize) {
                std::cerr << "Block length != trailingLength" << std::endl;
                exit(-1);
            }

            location += block->length;
        }
    }

    PcapNGInputFile::~PcapNGInputFile() = default;
}