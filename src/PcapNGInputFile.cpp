//
// Created by Rick Mark on 11/15/20.
//

#include "PcapNGInputFile.h"


namespace deusbmux {
    void PcapNGInputFile::parse() {

        auto *location = (std::byte *) this->m_mappedInputFile.data();
        const auto *endLocation = location + this->m_mappedInputFile.size();

        while (location < endLocation) {
            auto *block = (block_t *) location;

            if (block->type == TYPE_EPB) {
                auto *epb = (pcapng_epb_t *) (location + sizeof(block_t));
                assert(epb->interface_id == 0);

                auto *usbHeader = (darwin_usb_header_1_1_t *) (location + sizeof(block_t) + sizeof(pcapng_epb_t));
                assert(usbHeader->version == 0x101);
                assert(usbHeader->headerLength == sizeof(darwin_usb_header_1_1_t));

                auto deviceAddress = (((uint64_t) usbHeader->locationId) << 32U) | usbHeader->deviceId;

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
                            auto *setup = (usb_setup_t *) data.first;
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
                }
            }

            auto *trailingSize = (uint32_t *) (location + block->length - sizeof(uint32_t));
            if (block->length != *trailingSize) {
                std::cerr << "Block length != trailingLength" << std::endl;
                exit(-1);
            }

            location += block->length;
        }
    }

    PcapNGInputFile::~PcapNGInputFile() = default;
}