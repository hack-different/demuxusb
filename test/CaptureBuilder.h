// Builders for synthetic USB captures used by the test suite.  A Transcript records USB transfers independently of
//  the capture format; encoders turn it into the per-packet layout of each supported link type and the container
//  helpers wrap those packets in pcapng or pcap files.

#ifndef demuxusb_TEST_CAPTUREBUILDER_H
#define demuxusb_TEST_CAPTUREBUILDER_H

#include "demuxusb.hpp"
#include "pcapng.h"
#include "usb.h"
#include "usbmux.h"
#include "file/InputFile.h"

#include <fstream>
#include <random>

namespace fixture {
    typedef std::vector<std::byte> Bytes;

    inline Bytes bytes(std::initializer_list<int> values) {
        Bytes result;
        for (auto value: values) { result.push_back(static_cast<std::byte>(value)); }
        return result;
    }

    inline Bytes bytes(const std::string &value) {
        auto *begin = reinterpret_cast<const std::byte *>(value.data());
        return {begin, begin + value.size()};
    }

    inline void append(Bytes &target, const Bytes &source) {
        target.insert(target.end(), source.begin(), source.end());
    }

    template<typename T>
    void appendStruct(Bytes &target, const T &value) {
        auto *begin = reinterpret_cast<const std::byte *>(&value);
        target.insert(target.end(), begin, begin + sizeof(T));
    }

    inline void appendBE32(Bytes &target, uint32_t value) {
        appendStruct(target, boost::endian::native_to_big(value));
    }

    inline void appendBE16(Bytes &target, uint16_t value) {
        appendStruct(target, boost::endian::native_to_big(value));
    }

    inline void appendLE16(Bytes &target, uint16_t value) {
        appendStruct(target, boost::endian::native_to_little(value));
    }

    inline usb_setup_t setup(uint8_t requestType, uint8_t request, uint16_t value, uint16_t index, uint16_t length) {
        return usb_setup_t{requestType, request, value, index, length};
    }

    struct Transfer {
        bool control;
        uint8_t endpoint;
        usb_setup_t setup;
        Bytes data; // OUT payload or IN response
    };

    class Transcript {
    public:
        // A control transfer on endpoint 0.  data is the OUT payload, or the IN response.
        Transcript &control(const usb_setup_t &setup, const Bytes &data = {}) {
            this->transfers.push_back({true, static_cast<uint8_t>(setup.bmRequestType & USB_DIR_IN), setup, data});
            return *this;
        }

        Transcript &bulkOut(uint8_t endpoint, const Bytes &data) {
            this->transfers.push_back({false, static_cast<uint8_t>(endpoint & 0x7FU), {}, data});
            return *this;
        }

        Transcript &bulkIn(uint8_t endpoint, const Bytes &data) {
            this->transfers.push_back({false, static_cast<uint8_t>(endpoint | 0x80U), {}, data});
            return *this;
        }

        Transcript &getDescriptor(uint8_t type, uint8_t index, const Bytes &descriptor, uint16_t length = 0xFFFF) {
            auto response = descriptor;
            if (response.size() > length) { response.resize(length); }
            return this->control(setup(0x80, USB_REQ_GET_DESCRIPTOR, (type << 8U) | index, type == USB_DT_STRING ? 0x0409 : 0,
                                       length), response);
        }

        Transcript &setConfiguration(uint8_t value) {
            return this->control(setup(0x00, USB_REQ_SET_CONFIGURATION, value, 0, 0));
        }

        Transcript &setInterface(uint8_t interface, uint8_t alternate) {
            return this->control(setup(0x01, USB_REQ_SET_INTERFACE, alternate, interface, 0));
        }

        std::vector<Transfer> transfers;
    };

    // ---- Descriptors ----

    inline Bytes deviceDescriptor(uint16_t vendor, uint16_t product, uint16_t bcdDevice = 0x0100,
                                  uint8_t configurations = 1) {
        usb_device_descriptor descriptor{USB_DT_DEVICE_SIZE, USB_DT_DEVICE, 0x0200, 0, 0, 0, 64, vendor, product,
                                         bcdDevice, 1, 2, 3, configurations};
        Bytes result;
        appendStruct(result, descriptor);
        return result;
    }

    inline Bytes stringDescriptor(const std::string &value) {
        Bytes result{static_cast<std::byte>(2 + value.size() * 2), static_cast<std::byte>(USB_DT_STRING)};
        for (auto c: value) { appendLE16(result, static_cast<uint8_t>(c)); }
        return result;
    }

    struct Endpoint {
        uint8_t address;
        uint8_t attributes; // 2 = bulk, 3 = interrupt
    };

    struct Interface {
        uint8_t number;
        uint8_t alternate;
        uint8_t cls;
        uint8_t subclass;
        uint8_t protocol;
        std::vector<Endpoint> endpoints;
        Bytes extra{}; // class specific descriptors following the interface descriptor
    };

    inline Bytes configDescriptor(uint8_t value, const std::vector<Interface> &interfaces) {
        Bytes body;
        std::set<uint8_t> numbers;
        for (const auto &interface: interfaces) {
            numbers.insert(interface.number);
            usb_interface_descriptor descriptor{USB_DT_INTERFACE_SIZE, USB_DT_INTERFACE, interface.number,
                                                interface.alternate,
                                                static_cast<uint8_t>(interface.endpoints.size()), interface.cls,
                                                interface.subclass, interface.protocol, 0};
            appendStruct(body, descriptor);
            append(body, interface.extra);
            for (const auto &endpoint: interface.endpoints) {
                body.push_back(std::byte{USB_DT_ENDPOINT_SIZE});
                body.push_back(std::byte{USB_DT_ENDPOINT});
                body.push_back(static_cast<std::byte>(endpoint.address));
                body.push_back(static_cast<std::byte>(endpoint.attributes));
                appendLE16(body, 512);
                body.push_back(std::byte{0});
            }
        }

        usb_config_descriptor config{USB_DT_CONFIG_SIZE, USB_DT_CONFIG,
                                     static_cast<uint16_t>(USB_DT_CONFIG_SIZE + body.size()),
                                     static_cast<uint8_t>(numbers.size()), value, 0, 0x80, 250};
        Bytes result;
        appendStruct(result, config);
        append(result, body);
        return result;
    }

    // Enumerates a device the way a host does: partial device descriptor, address, full descriptors, strings.
    inline Transcript &enumerate(Transcript &transcript, const Bytes &device, const std::vector<Bytes> &configurations,
                                 const std::vector<std::string> &strings, uint8_t configuration = 1) {
        transcript.getDescriptor(USB_DT_DEVICE, 0, device, 8);
        transcript.getDescriptor(USB_DT_DEVICE, 0, device, USB_DT_DEVICE_SIZE);
        for (size_t i = 0; i < configurations.size(); i++) {
            transcript.getDescriptor(USB_DT_CONFIG, i, configurations[i], USB_DT_CONFIG_SIZE);
            transcript.getDescriptor(USB_DT_CONFIG, i, configurations[i], configurations[i].size());
        }
        transcript.getDescriptor(USB_DT_STRING, 0, bytes({4, USB_DT_STRING, 0x09, 0x04}), 255);
        for (size_t i = 0; i < strings.size(); i++) {
            transcript.getDescriptor(USB_DT_STRING, i + 1, stringDescriptor(strings[i]), 255);
        }
        if (configuration != 0) { transcript.setConfiguration(configuration); }
        return transcript;
    }

    // An iPhone in normal mode: configuration 1 with PTP, configuration 4 with USBMUX on interface 2 whose bulk
    //  endpoints are only present in alternate setting 1 when withAlternate is set.
    inline Bytes iphoneUsbmuxConfiguration(uint8_t value, bool withAlternate = false) {
        std::vector<Interface> interfaces{
                {0, 0, 0x06, 0x01, 0x01, {{0x81, 2}, {0x02, 2}, {0x83, 3}}},
        };
        if (withAlternate) {
            interfaces.push_back({1, 0, USB_CLASS_APPLICATION_SPECIFIC, APPLE_SUBCLASS_USBMUX, APPLE_PROTOCOL_USBMUX2, {}});
            interfaces.push_back({1, 1, USB_CLASS_APPLICATION_SPECIFIC, APPLE_SUBCLASS_USBMUX, APPLE_PROTOCOL_USBMUX2,
                                  {{0x85, 2}, {0x04, 2}}});
        } else {
            interfaces.push_back({1, 0, USB_CLASS_APPLICATION_SPECIFIC, APPLE_SUBCLASS_USBMUX, APPLE_PROTOCOL_USBMUX2,
                                  {{0x85, 2}, {0x04, 2}}});
        }
        return configDescriptor(value, interfaces);
    }

    inline Bytes dfuFunctionalDescriptor(uint16_t transferSize) {
        Bytes result{std::byte{9}, std::byte{USB_DT_DFU_FUNCTIONAL}, std::byte{0x0B}};
        appendLE16(result, 0x0A);
        appendLE16(result, transferSize);
        appendLE16(result, 0x0110);
        return result;
    }

    // ---- usbmux ----

    inline Bytes muxPacket(uint32_t protocol, const Bytes &payload, bool outbound, bool v2 = true,
                           uint16_t txSeq = 0, uint16_t rxSeq = 0) {
        Bytes result;
        auto headerLength = v2 ? sizeof(mux_header) : 8;
        appendBE32(result, protocol);
        appendBE32(result, static_cast<uint32_t>(headerLength + payload.size()));
        if (v2) {
            appendBE32(result, outbound ? USBMUX_MAGIC_OUT : USBMUX_MAGIC_IN);
            appendBE16(result, txSeq);
            appendBE16(result, rxSeq);
        }
        append(result, payload);
        return result;
    }

    inline Bytes muxVersion(uint32_t major, bool outbound) {
        Bytes payload;
        appendBE32(payload, major);
        appendBE32(payload, 0);
        appendBE32(payload, 0);
        return muxPacket(MUX_PROTO_VERSION, payload, outbound, false);
    }

    inline Bytes tcpSegment(uint16_t source, uint16_t destination, uint16_t flags, const Bytes &data = {}) {
        Bytes result;
        appendBE16(result, source);
        appendBE16(result, destination);
        appendBE32(result, 0);
        appendBE32(result, 0);
        appendBE16(result, static_cast<uint16_t>((5U << 12U) | flags));
        appendBE16(result, 0x0200);
        appendBE16(result, 0);
        appendBE16(result, 0);
        append(result, data);
        return result;
    }

    inline Bytes xmlPlist(const std::string &dictionaryBody) {
        return bytes("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                     "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
                     "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
                     "<plist version=\"1.0\"><dict>" + dictionaryBody + "</dict></plist>\n");
    }

    inline Bytes lengthPrefixed(const Bytes &message) {
        Bytes result;
        appendBE32(result, static_cast<uint32_t>(message.size()));
        append(result, message);
        return result;
    }

    // ---- Packet encoders ----

    enum class Format {
        Darwin,
        Darwin_1_1,
        USBPcap,
        Usbmon,
        UsbmonMmapped
    };

    inline uint32_t linktype(Format format) {
        switch (format) {
            case Format::Darwin:
            case Format::Darwin_1_1: return LINKTYPE_USB_DARWIN;
            case Format::USBPcap: return LINKTYPE_USBPCAP;
            case Format::Usbmon: return LINKTYPE_USB_LINUX;
            case Format::UsbmonMmapped: return LINKTYPE_USB_LINUX_MMAPPED;
        }
        return 0;
    }

    inline Bytes darwinPacket(bool v11, uint8_t type, uint64_t id, uint32_t location, uint8_t address,
                              uint8_t endpoint, uint8_t endpointType, const Bytes &payload) {
        Bytes result;
        darwin_usb_header_1_1_t header{};
        header.version = v11 ? DARWIN_USB_HEADER_VERSION_1_1 : DARWIN_USB_HEADER_VERSION_1_0;
        header.headerLength = v11 ? sizeof(darwin_usb_header_1_1_t) : sizeof(darwin_usb_header_1_0_t);
        header.type = type;
        header.length = payload.size();
        header.uniqueId = id;
        header.locationId = location;
        header.speed = DARWIN_SPEED_HIGH;
        header.deviceId = address;
        header.endpointId = endpoint;
        header.endpointType = endpointType;
        auto *begin = reinterpret_cast<const std::byte *>(&header);
        result.insert(result.end(), begin, begin + header.headerLength);
        append(result, payload);
        return result;
    }

    inline Bytes usbpcapPacket(uint64_t irp, uint8_t info, uint16_t device, uint8_t endpoint, uint8_t transfer,
                               int stage, const Bytes &payload) {
        usbpcap_buffer_control_header_t header{};
        header.header.headerLen = stage >= 0 ? sizeof(usbpcap_buffer_control_header_t) : sizeof(usbpcap_packet_header_t);
        header.header.irpId = irp;
        header.header.info = info;
        header.header.bus = 1;
        header.header.device = device;
        header.header.endpoint = endpoint;
        header.header.transfer = transfer;
        header.header.dataLength = payload.size();
        header.stage = stage >= 0 ? stage : 0;
        Bytes result;
        auto *begin = reinterpret_cast<const std::byte *>(&header);
        result.insert(result.end(), begin, begin + header.header.headerLen);
        append(result, payload);
        return result;
    }

    inline Bytes usbmonPacket(bool mmapped, uint8_t event, uint64_t id, uint8_t transfer, uint8_t endpoint,
                              uint8_t address, const usb_setup_t *setup, const Bytes &payload) {
        pcap_usb_header_mmapped_t header{};
        header.header.id = id;
        header.header.event_type = event;
        header.header.transfer_type = transfer;
        header.header.endpoint_number = endpoint;
        header.header.device_address = address;
        header.header.bus_id = 1;
        header.header.setup_flag = setup != nullptr ? 0 : '-';
        header.header.data_flag = payload.empty() ? '<' : 0;
        header.header.urb_len = payload.size();
        header.header.data_len = payload.size();
        if (setup != nullptr) { header.header.setup = *setup; }
        Bytes result;
        auto *begin = reinterpret_cast<const std::byte *>(&header);
        result.insert(result.end(), begin, begin + (mmapped ? sizeof(header) : sizeof(header.header)));
        append(result, payload);
        return result;
    }

    // Encodes a transcript for one device.  address is the device address (Darwin also uses location).
    inline std::vector<Bytes> encode(Format format, const Transcript &transcript, uint8_t address = 5,
                                     uint32_t location = 0x14100000, uint64_t firstId = 0x1000) {
        std::vector<Bytes> packets;
        auto id = firstId;
        for (const auto &transfer: transcript.transfers) {
            id++;
            bool in = transfer.endpoint & USB_DIR_IN;
            Bytes outData = in ? Bytes{} : transfer.data;
            Bytes inData = in ? transfer.data : Bytes{};

            switch (format) {
                case Format::Darwin:
                case Format::Darwin_1_1: {
                    bool v11 = format == Format::Darwin_1_1;
                    if (transfer.control) {
                        Bytes submit;
                        appendStruct(submit, transfer.setup);
                        append(submit, outData);
                        packets.push_back(darwinPacket(v11, DARWIN_IO_SUBMIT, id, location, address, transfer.endpoint,
                                                       USB_EP_CONTROL, submit));
                        packets.push_back(darwinPacket(v11, DARWIN_IO_COMPLETE, id, location, address,
                                                       transfer.endpoint, USB_EP_CONTROL, inData));
                    } else {
                        packets.push_back(darwinPacket(v11, DARWIN_IO_SUBMIT, id, location, address, transfer.endpoint,
                                                       USB_EP_BULK, outData));
                        packets.push_back(darwinPacket(v11, DARWIN_IO_COMPLETE, id, location, address,
                                                       transfer.endpoint, USB_EP_BULK, inData));
                    }
                    break;
                }
                case Format::USBPcap:
                    if (transfer.control) {
                        Bytes submit;
                        appendStruct(submit, transfer.setup);
                        append(submit, outData);
                        packets.push_back(usbpcapPacket(id, 0, address, transfer.endpoint, USBPCAP_TRANSFER_CONTROL,
                                                        USBPCAP_CONTROL_STAGE_SETUP, submit));
                        packets.push_back(usbpcapPacket(id, USBPCAP_INFO_PDO_TO_FDO, address, transfer.endpoint,
                                                        USBPCAP_TRANSFER_CONTROL, USBPCAP_CONTROL_STAGE_COMPLETE,
                                                        inData));
                    } else {
                        packets.push_back(usbpcapPacket(id, 0, address, transfer.endpoint, USBPCAP_TRANSFER_BULK, -1,
                                                        outData));
                        packets.push_back(usbpcapPacket(id, USBPCAP_INFO_PDO_TO_FDO, address, transfer.endpoint,
                                                        USBPCAP_TRANSFER_BULK, -1, inData));
                    }
                    break;
                case Format::Usbmon:
                case Format::UsbmonMmapped: {
                    bool mmapped = format == Format::UsbmonMmapped;
                    auto type = transfer.control ? USBMON_TRANSFER_CONTROL : USBMON_TRANSFER_BULK;
                    packets.push_back(usbmonPacket(mmapped, USBMON_EVENT_SUBMIT, id, type, transfer.endpoint, address,
                                                   transfer.control ? &transfer.setup : nullptr, outData));
                    packets.push_back(usbmonPacket(mmapped, USBMON_EVENT_COMPLETE, id, type, transfer.endpoint,
                                                   address, nullptr, inData));
                    break;
                }
            }
        }
        return packets;
    }

    // ---- Containers ----

    inline void appendBlock(Bytes &file, uint32_t type, const Bytes &body) {
        auto padded = body;
        while (padded.size() % 4 != 0) { padded.push_back(std::byte{0}); }
        uint32_t length = static_cast<uint32_t>(padded.size() + 12);
        appendStruct(file, type);
        appendStruct(file, length);
        append(file, padded);
        appendStruct(file, length);
    }

    inline Bytes sectionHeaderBody() {
        Bytes body;
        appendStruct(body, static_cast<uint32_t>(PCAPNG_BYTE_ORDER_MAGIC));
        appendStruct(body, static_cast<uint16_t>(1));
        appendStruct(body, static_cast<uint16_t>(0));
        appendStruct(body, static_cast<int64_t>(-1));
        return body;
    }

    inline Bytes interfaceBody(uint32_t linktype) {
        Bytes body;
        appendStruct(body, static_cast<uint16_t>(linktype));
        appendStruct(body, static_cast<uint16_t>(0));
        appendStruct(body, static_cast<uint32_t>(0x40000));
        return body;
    }

    inline Bytes enhancedPacketBody(uint32_t interface, const Bytes &packet) {
        Bytes body;
        pcapng_epb_t epb{interface, 0, 0, static_cast<uint32_t>(packet.size()), static_cast<uint32_t>(packet.size())};
        appendStruct(body, epb);
        append(body, packet);
        return body;
    }

    inline Bytes pcapng(uint32_t linktype, const std::vector<Bytes> &packets) {
        Bytes file;
        appendBlock(file, TYPE_SECTION_HEADER, sectionHeaderBody());
        appendBlock(file, TIME_INTERFACE_DESCRIPTOR_BLOCK, interfaceBody(linktype));
        for (const auto &packet: packets) {
            appendBlock(file, TYPE_ENHANCED_PACKET_BLOCK, enhancedPacketBody(0, packet));
        }
        return file;
    }

    inline Bytes pcap(uint32_t linktype, const std::vector<Bytes> &packets, bool swapped = false,
                      bool nanoseconds = false) {
        auto value = [swapped](uint32_t field) { return swapped ? boost::endian::endian_reverse(field) : field; };
        auto value16 = [swapped](uint16_t field) { return swapped ? boost::endian::endian_reverse(field) : field; };

        Bytes file;
        pcap_file_header_t header{value(nanoseconds ? PCAP_MAGIC_NANOSECONDS : PCAP_MAGIC_MICROSECONDS), value16(2),
                                  value16(4), 0, 0, value(0x40000), value(linktype)};
        appendStruct(file, header);
        for (const auto &packet: packets) {
            pcap_record_header_t record{0, 0, value(packet.size()), value(packet.size())};
            appendStruct(file, record);
            append(file, packet);
        }
        return file;
    }

    inline Bytes capture(Format format, const Transcript &transcript) {
        return pcapng(linktype(format), encode(format, transcript));
    }

    // A file in the system temp directory, removed when the object is destroyed.
    class TempCapture {
    public:
        explicit TempCapture(const Bytes &contents, const std::string &extension = ".pcapng") {
            this->path = fs::temp_directory_path() / fs::unique_path("demuxusb-test-%%%%-%%%%-%%%%" + extension);
            std::ofstream output(this->path.string(), std::ios::binary);
            output.write(reinterpret_cast<const char *>(contents.data()), static_cast<std::streamsize>(contents.size()));
        }

        ~TempCapture() {
            boost::system::error_code error;
            fs::remove(this->path, error);
        }

        fs::path path;
    };

    // ---- Parsing helpers ----

    struct Parsed {
        std::unique_ptr<TempCapture> file;
        std::unique_ptr<demuxusb::InputFile> input;

        std::vector<demuxusb::Device *> devices() { return this->input->getDevices(); }
    };

    inline Parsed parse(const Bytes &contents, const std::string &extension = ".pcapng") {
        Parsed result;
        result.file = std::make_unique<TempCapture>(contents, extension);
        result.input = demuxusb::InputFile::open(result.file->path);
        result.input->parse();
        return result;
    }

    template<typename T>
    std::shared_ptr<T> expertOf(demuxusb::Device *device) {
        for (const auto &expert: device->getExperts()) {
            if (auto typed = std::dynamic_pointer_cast<T>(expert)) { return typed; }
        }
        return nullptr;
    }

    // Enumerates an iPhone exposing USBMUX on configuration 1, interface 1 (endpoints 0x85 IN / 0x04 OUT).
    inline Transcript iphone(bool withAlternate = false) {
        Transcript transcript;
        enumerate(transcript, deviceDescriptor(APPLE_VID, 0x12A8), {iphoneUsbmuxConfiguration(1, withAlternate)},
                  {"Apple Inc.", "iPhone", "00008101-001234567890001E"});
        if (withAlternate) { transcript.setInterface(1, 1); }
        return transcript;
    }
}

#endif //demuxusb_TEST_CAPTUREBUILDER_H
