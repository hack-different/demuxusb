//
// Created by Rick Mark on 11/14/20.
//

#include "InputFile.h"
#include "PcapInputFile.h"
#include "PcapNGInputFile.h"

#include <fstream>


namespace demuxusb {
    static byte_array mutableBytes(const std::byte *data, size_t length) {
        // Experts only read packet data; the mapping itself is read only.
        return byte_array{const_cast<std::byte *>(data), length};
    }

    static bool isControlDescriptorRead(const usb_setup_t &setup) {
        return setup.bmRequestType == (USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_DEVICE) &&
               setup.bRequest == USB_REQ_GET_DESCRIPTOR && (setup.wValue >> 8U) == USB_DT_DEVICE;
    }

    void DeviceHandle::beginControl(uint8_t endpoint, uint64_t id, const usb_setup_t &setup, byte_array data) {
        auto &pending = this->m_inflight[{USB_ENDPOINT_ID(endpoint), id}];
        pending = Pending{true, setup, {}};
        if (data.first != nullptr && data.second > 0) {
            pending.data.assign(data.first, data.first + data.second);
        }
    }

    void DeviceHandle::appendControl(uint8_t endpoint, uint64_t id, byte_array data) {
        auto found = this->m_inflight.find({USB_ENDPOINT_ID(endpoint), id});
        if (found == this->m_inflight.end() || data.second == 0) { return; }
        found->second.data.insert(found->second.data.end(), data.first, data.first + data.second);
    }

    void DeviceHandle::endControl(uint8_t endpoint, uint64_t id, byte_array data) {
        auto found = this->m_inflight.find({USB_ENDPOINT_ID(endpoint), id});
        if (found == this->m_inflight.end() || !found->second.control) {
            // Completion of a transfer submitted before the capture started; without the setup it can't be decoded
            this->m_unmatched++;
            return;
        }

        auto pending = std::move(found->second);
        this->m_inflight.erase(found);

        byte_array payload = data;
        if (pending.setup.bmRequestType & USB_DIR_IN) {
            if (payload.second == 0 && !pending.data.empty()) {
                payload = {pending.data.data(), pending.data.size()};
            }
        } else if (!pending.data.empty()) {
            payload = {pending.data.data(), pending.data.size()};
        }

        // Never hand on more than was requested
        payload.second = std::min<size_t>(payload.second, pending.setup.wLength);

        this->dispatchControl(USB_ENDPOINT_ID(endpoint), pending.setup, payload);
    }

    void DeviceHandle::dispatchControl(uint8_t endpoint, const usb_setup_t &setup, byte_array data) {
        if (isControlDescriptorRead(setup) && data.second >= USB_DT_DEVICE_SIZE && this->device()->hasDeviceDescriptor()) {
            const auto &current = this->device()->getDeviceDescriptor();
            if (std::memcmp(&current, data.first, USB_DT_DEVICE_SIZE) != 0) {
                this->m_sessions.push_back(std::make_unique<Device>(this->m_id));
            }
        }

        if (setup.bmRequestType & USB_DIR_IN) {
            this->device()->processControlPacketIn(endpoint, setup, data);
        } else {
            this->device()->processControlPacketOut(endpoint, setup, data);
        }
    }

    void DeviceHandle::beginBulk(uint8_t endpoint, uint64_t id, byte_array data) {
        auto &pending = this->m_inflight[{endpoint, id}];
        pending = Pending{false, {}, {}};
        if (data.first != nullptr && data.second > 0 && USB_ENDPOINT_OUT(endpoint)) {
            pending.data.assign(data.first, data.first + data.second);
        }
    }

    void DeviceHandle::endBulk(uint8_t endpoint, uint64_t id, byte_array data) {
        auto found = this->m_inflight.find({endpoint, id});
        Pending pending{};
        if (found != this->m_inflight.end()) {
            pending = std::move(found->second);
            this->m_inflight.erase(found);
        } else {
            this->m_unmatched++;
        }

        if (USB_ENDPOINT_IN(endpoint)) {
            this->device()->processBulkPacketIn(USB_ENDPOINT_ID(endpoint), data);
        } else {
            auto payload = (data.second == 0 && !pending.data.empty())
                           ? byte_array{pending.data.data(), pending.data.size()} : data;
            this->device()->processBulkPacketOut(USB_ENDPOINT_ID(endpoint), payload);
        }
    }

    void DeviceHandle::abort(uint8_t endpoint, uint64_t id) {
        this->m_inflight.erase({endpoint, id});
        this->m_inflight.erase({USB_ENDPOINT_ID(endpoint), id});
    }

    std::vector<Device *> DeviceHandle::devices() {
        std::vector<Device *> result;
        for (const auto &session: this->m_sessions) {
            result.push_back(session.get());
        }
        return result;
    }

    InputFile::InputFile(fs::path path) : m_path{std::move(path)} {
        boost::system::error_code error;
        auto size = fs::file_size(this->m_path, error);
        if (error) {
            throw ParseError("Unable to open " + this->m_path.string() + ": " + error.message());
        }

        if (size > 0) {
            try {
                this->m_mappedInputFile.open(this->m_path);
            } catch (const std::exception &exception) {
                throw ParseError("Unable to map " + this->m_path.string() + ": " + exception.what());
            }
            this->m_data = reinterpret_cast<const std::byte *>(this->m_mappedInputFile.data());
            this->m_size = this->m_mappedInputFile.size();
        }
    }

    std::unique_ptr<InputFile> InputFile::open(const fs::path &path) {
        uint32_t magic = 0;
        {
            std::ifstream stream(path.string(), std::ios::binary);
            if (!stream) { throw ParseError("Unable to open " + path.string()); }
            stream.read(reinterpret_cast<char *>(&magic), sizeof(magic));
            if (stream.gcount() != sizeof(magic)) { throw ParseError(path.string() + " is too short to be a capture"); }
        }

        switch (magic) {
            case TYPE_SECTION_HEADER:
                return std::make_unique<PcapNGInputFile>(path);
            case PCAP_MAGIC_MICROSECONDS:
            case PCAP_MAGIC_NANOSECONDS:
            case PCAP_MAGIC_MICROSECONDS_SWAPPED:
            case PCAP_MAGIC_NANOSECONDS_SWAPPED:
                return std::make_unique<PcapInputFile>(path);
            default:
                throw ParseError(path.string() + " is not a pcap or pcapng capture");
        }
    }

    std::vector<Device *> InputFile::getDevices() {
        auto result = std::vector<Device *>{};

        for (const auto &id: this->m_deviceOrder) {
            for (auto *device: this->m_devices[id]->devices()) {
                result.push_back(device);
            }
        }

        return result;
    }

    DeviceHandle &InputFile::handleFor(uint64_t id) {
        auto &device = this->m_devices[id];

        if (device == nullptr) {
            device = std::make_shared<DeviceHandle>(id);
            this->m_deviceOrder.push_back(id);
        }

        return *device;
    }

    void InputFile::warn(const std::string &warning) {
        this->m_warningCount++;
        if (this->m_warnings.size() < MAX_WARNINGS) {
            this->m_warnings.push_back(warning);
        }
    }

    bool InputFile::processPacket(uint32_t linktype, const std::byte *data, size_t length) {
        this->m_packetCount++;

        switch (linktype) {
            case LINKTYPE_USB_DARWIN:
                return this->process_darwin_packet(data, length);
            case LINKTYPE_USBPCAP:
                return this->process_usbpcap_packet(data, length);
            case LINKTYPE_USB_LINUX:
                return this->process_usbmon_packet(data, length, false);
            case LINKTYPE_USB_LINUX_MMAPPED:
                return this->process_usbmon_packet(data, length, true);
            default:
                if (!this->m_unsupportedLinktypes.contains(linktype)) {
                    this->m_unsupportedLinktypes.insert(linktype);
                    this->warn((boost::format("Skipping packets with unsupported link type %1%") % linktype).str());
                }
                return false;
        }
    }

    bool InputFile::process_darwin_packet(const std::byte *data, size_t length) {
        darwin_usb_header_1_0_t usbHeader{};
        if (!readStruct(data, length, usbHeader)) {
            this->warn("Darwin packet shorter than its header");
            return false;
        }

        if (usbHeader.version != DARWIN_USB_HEADER_VERSION_1_0 && usbHeader.version != DARWIN_USB_HEADER_VERSION_1_1) {
            this->warn((boost::format("Unsupported Darwin USB header version 0x%1$04x") % usbHeader.version).str());
            return false;
        }
        if (usbHeader.headerLength < sizeof(darwin_usb_header_1_0_t) || usbHeader.headerLength > length) {
            this->warn("Darwin packet header length is out of bounds");
            return false;
        }

        auto deviceAddress = (static_cast<uint64_t>(usbHeader.locationId) << 32U) | usbHeader.deviceId;
        auto *payload = data + usbHeader.headerLength;
        auto payloadLength = std::min<size_t>(usbHeader.length, length - usbHeader.headerLength);
        auto &device = this->handleFor(deviceAddress);
        const auto endpoint = (usb_endpoint_id) usbHeader.endpointId;

        switch (usbHeader.endpointType) {
            case USB_EP_CONTROL:
                if (usbHeader.type == DARWIN_IO_SUBMIT) {
                    usb_setup_t setup{};
                    if (!readStruct(payload, payloadLength, setup)) {
                        this->warn("Darwin control submission without a setup packet");
                        return false;
                    }
                    device.beginControl(endpoint, usbHeader.uniqueId, setup,
                                        mutableBytes(payload + sizeof(setup), payloadLength - sizeof(setup)));
                } else if (usbHeader.type == DARWIN_IO_COMPLETE) {
                    device.endControl(endpoint, usbHeader.uniqueId, mutableBytes(payload, payloadLength));
                }
                break;
            case USB_EP_BULK:
            case USB_EP_INTERRUPT:
                if (usbHeader.type == DARWIN_IO_SUBMIT) {
                    device.beginBulk(endpoint, usbHeader.uniqueId, mutableBytes(payload, payloadLength));
                } else if (usbHeader.type == DARWIN_IO_COMPLETE) {
                    device.endBulk(endpoint, usbHeader.uniqueId, mutableBytes(payload, payloadLength));
                }
                break;
            default:
                break;
        }
        return true;
    }

    bool InputFile::process_usbpcap_packet(const std::byte *data, size_t length) {
        usbpcap_packet_header_t usbHeader{};
        if (!readStruct(data, length, usbHeader)) {
            this->warn("USBPcap packet shorter than its header");
            return false;
        }
        if (usbHeader.headerLen < sizeof(usbpcap_packet_header_t) || usbHeader.headerLen > length) {
            this->warn("USBPcap packet header length is out of bounds");
            return false;
        }

        auto deviceAddress = (static_cast<uint64_t>(usbHeader.bus) << 16U) | usbHeader.device;
        auto *payload = data + usbHeader.headerLen;
        auto payloadLength = std::min<size_t>(usbHeader.dataLength, length - usbHeader.headerLen);
        auto &device = this->handleFor(deviceAddress);
        const auto endpoint = (usb_endpoint_id) usbHeader.endpoint;

        switch (usbHeader.transfer) {
            case USBPCAP_TRANSFER_CONTROL: {
                usbpcap_buffer_control_header_t controlHeader{};
                if (usbHeader.headerLen < sizeof(controlHeader) || !readStruct(data, length, controlHeader)) {
                    this->warn("USBPcap control packet without a stage");
                    return false;
                }

                switch (controlHeader.stage) {
                    case USBPCAP_CONTROL_STAGE_SETUP: {
                        usb_setup_t setup{};
                        if (!readStruct(payload, payloadLength, setup)) {
                            this->warn("USBPcap setup stage without a setup packet");
                            return false;
                        }
                        device.beginControl(endpoint, usbHeader.irpId, setup,
                                            mutableBytes(payload + sizeof(setup), payloadLength - sizeof(setup)));
                    }
                        break;
                    case USBPCAP_CONTROL_STAGE_DATA:
                        device.appendControl(endpoint, usbHeader.irpId, mutableBytes(payload, payloadLength));
                        break;
                    case USBPCAP_CONTROL_STAGE_COMPLETE:
                        device.endControl(endpoint, usbHeader.irpId, mutableBytes(payload, payloadLength));
                        break;
                    default:
                        break;
                }
            }
                break;
            case USBPCAP_TRANSFER_BULK:
            case USBPCAP_TRANSFER_INTERRUPT:
                if (usbHeader.info & USBPCAP_INFO_PDO_TO_FDO) {
                    device.endBulk(endpoint, usbHeader.irpId, mutableBytes(payload, payloadLength));
                } else {
                    device.beginBulk(endpoint, usbHeader.irpId, mutableBytes(payload, payloadLength));
                }
                break;
            default:
                break;
        }
        return true;
    }

    bool InputFile::process_usbmon_packet(const std::byte *data, size_t length, bool mmapped) {
        pcap_usb_header_t usbHeader{};
        size_t headerLength = mmapped ? sizeof(pcap_usb_header_mmapped_t) : sizeof(pcap_usb_header_t);
        if (length < headerLength || !readStruct(data, length, usbHeader)) {
            this->warn("usbmon packet shorter than its header");
            return false;
        }

        auto deviceAddress = (static_cast<uint64_t>(usbHeader.bus_id) << 8U) | usbHeader.device_address;
        auto *payload = data + headerLength;
        auto payloadLength = std::min<size_t>(usbHeader.data_len, length - headerLength);
        auto &device = this->handleFor(deviceAddress);
        const auto endpoint = (usb_endpoint_id) usbHeader.endpoint_number;

        if (usbHeader.event_type == USBMON_EVENT_ERROR) {
            device.abort(endpoint, usbHeader.id);
            return true;
        }

        switch (usbHeader.transfer_type) {
            case USBMON_TRANSFER_CONTROL:
                if (usbHeader.event_type == USBMON_EVENT_SUBMIT) {
                    if (usbHeader.setup_flag != 0) {
                        this->warn("usbmon control submission without a setup packet");
                        return false;
                    }
                    device.beginControl(endpoint, usbHeader.id, usbHeader.setup, mutableBytes(payload, payloadLength));
                } else if (usbHeader.event_type == USBMON_EVENT_COMPLETE) {
                    device.endControl(endpoint, usbHeader.id, mutableBytes(payload, payloadLength));
                }
                break;
            case USBMON_TRANSFER_BULK:
            case USBMON_TRANSFER_INTERRUPT:
                if (usbHeader.event_type == USBMON_EVENT_SUBMIT) {
                    device.beginBulk(endpoint, usbHeader.id, mutableBytes(payload, payloadLength));
                } else if (usbHeader.event_type == USBMON_EVENT_COMPLETE) {
                    device.endBulk(endpoint, usbHeader.id, mutableBytes(payload, payloadLength));
                }
                break;
            default:
                break;
        }
        return true;
    }
}
