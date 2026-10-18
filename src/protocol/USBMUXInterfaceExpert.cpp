//
// Created by Rick Mark on 11/16/20.
//

#include "USBMUXInterfaceExpert.h"

// usbmux v1 header: protocol and length.  v2 adds magic and sequence numbers (struct mux_header).
static const size_t MUX_HEADER_V1_SIZE = 8;
// Larger than any usbmux packet usbmuxd will produce; anything bigger means we lost framing.
static const uint32_t MAX_MUX_PACKET = 1024U * 1024U;

static uint32_t readBigEndian32(const std::byte *data) {
    uint32_t value;
    std::memcpy(&value, data, sizeof(value));
    return boost::endian::big_to_native(value);
}

std::string USBMUXInterfaceExpert::toString() {
    auto format = boost::format("USBMUXInterfaceExpert (configuration=%1%, interface=%2%, version=%3%):\n");
    auto stream_format = boost::format(
            "\t-\t%1$x\t: %2% -> %3%%4% (%5% transactions, in %6% bytes, out %7% bytes, %8%)\n");

    std::string result = (format % (uint32_t) this->m_configuration % (uint32_t) this->m_interface %
                          this->m_version).str();

    for (auto &[id, stream]: this->m_streams) {
        auto service = stream->getServiceName().empty() ? std::string{} : " [" + stream->getServiceName() + "]";
        auto encoding = stream->getEncodingOut() == stream->getEncodingIn()
                        ? std::string(encodingName(stream->getEncodingOut()))
                        : std::string(encodingName(stream->getEncodingOut())) + "/" +
                          encodingName(stream->getEncodingIn());

        result.append((stream_format % id % stream->getHostPort() % stream->getDevicePort() % service %
                       stream->getTransactionCount() % stream->getBytesIn() % stream->getBytesOut() %
                       encoding).str());
    }

    return result;
}

std::string USBMUXInterfaceExpert::details() {
    std::string result;
    for (auto &[id, stream]: this->m_streams) {
        if (stream->getMessages().empty()) { continue; }
        result += (boost::format("      stream %1$x (%2% -> %3%):\n") % id % stream->getHostPort() %
                   stream->getDevicePort()).str();
        for (const auto &message: stream->getMessages()) {
            result += std::string("        ") + (message.outbound ? "> " : "< ") + message.summary() + "\n";
        }
    }
    return result;
}

std::vector<ExtractedArtifact> USBMUXInterfaceExpert::getArtifacts() {
    std::vector<ExtractedArtifact> result;
    for (auto &[id, stream]: this->m_streams) {
        if (stream->getMessages().empty()) { continue; }

        std::string document;
        for (const auto &message: stream->getMessages()) {
            document += message.outbound ? "<!-- host -> device -->\n" : "<!-- device -> host -->\n";
            document += message.xml();
        }

        auto name = (boost::format("usbmux-%1$016x-%2%-%3%.plist.txt") % id % stream->getHostPort() %
                     stream->getDevicePort()).str();
        auto *begin = reinterpret_cast<const std::byte *>(document.data());
        result.push_back({name, std::vector<std::byte>(begin, begin + document.size())});
    }
    return result;
}

uint64_t USBMUXInterfaceExpert::beginStream(uint16_t host, uint16_t device) {
    uint32_t stream_port = STREAM_PORT(host, device);
    auto index = ++this->m_streamIndex[stream_port];
    auto stream_id = STREAM_ID(host, device, index);

    auto stream = std::make_shared<MuxStream>(host, device);
    if (device == LOCKDOWN_PORT) {
        stream->setServiceName("lockdownd");
    } else if (this->m_serviceNames.contains(device)) {
        stream->setServiceName(this->m_serviceNames[device]);
    }
    this->m_streams[stream_id] = stream;

    return stream_id;
}

uint64_t USBMUXInterfaceExpert::getStreamId(uint16_t host, uint16_t device) {
    uint32_t stream_port = STREAM_PORT(host, device);

    auto found = this->m_streamIndex.find(stream_port);
    if (found == this->m_streamIndex.end()) {
        // Traffic for a connection opened before the capture started
        return this->beginStream(host, device);
    }

    return STREAM_ID(host, device, found->second);
}

std::shared_ptr<MuxStream> USBMUXInterfaceExpert::streamFor(uint16_t host, uint16_t device) {
    return this->m_streams[this->getStreamId(host, device)];
}

void USBMUXInterfaceExpert::processBulkOut(byte_array data) {
    InterfaceExpert::processBulkOut(data);
    this->feed(this->m_out, data);
}

void USBMUXInterfaceExpert::processBulkIn(byte_array data) {
    InterfaceExpert::processBulkIn(data);
    this->feed(this->m_in, data);
}

void USBMUXInterfaceExpert::feed(Direction &direction, byte_array data) {
    if (data.second == 0) { return; }
    direction.buffer.insert(direction.buffer.end(), data.first, data.first + data.second);

    auto &buffer = direction.buffer;
    while (buffer.size() >= MUX_HEADER_V1_SIZE) {
        auto protocol = readBigEndian32(buffer.data());
        auto length = readBigEndian32(buffer.data() + sizeof(uint32_t));

        if (length < MUX_HEADER_V1_SIZE || length > MAX_MUX_PACKET || protocol > MUX_PROTO_TCP) {
            // Lost framing (capture started mid packet, or dropped transfers).  Discard and wait for a fresh packet.
            this->m_resyncs++;
            buffer.clear();
            return;
        }

        // A v2 header carries the magic; version packets are always sent with the v1 header.
        auto peek = std::min<size_t>(length, sizeof(mux_header));
        if (buffer.size() < peek) { return; }

        size_t header_length = MUX_HEADER_V1_SIZE;
        if (length >= sizeof(mux_header)) {
            auto magic = readBigEndian32(buffer.data() + offsetof(mux_header, magic));
            if (magic == USBMUX_MAGIC_IN || magic == USBMUX_MAGIC_OUT) {
                header_length = sizeof(mux_header);
            }
        }

        if (buffer.size() < length) { return; }

        this->processPacket(direction.outbound, protocol, buffer.data() + header_length, length - header_length);
        buffer.erase(buffer.begin(), buffer.begin() + length);
    }
}

void USBMUXInterfaceExpert::processPacket(bool outbound, uint32_t protocol, std::byte *payload, size_t length) {
    switch (protocol) {
        case MUX_PROTO_VERSION:
            if (length >= sizeof(uint32_t)) {
                this->m_version = readBigEndian32(payload);
            }
            break;
        case MUX_PROTO_TCP:
            this->processTCP(outbound, payload, length);
            break;
        default:
            break;
    }
}

void USBMUXInterfaceExpert::processTCP(bool outbound, std::byte *payload, size_t length) {
    tcphdr tcp_header{};
    if (!demuxusb::readStruct(payload, length, tcp_header)) { return; }

    auto source = boost::endian::big_to_native(tcp_header.source);
    auto destination = boost::endian::big_to_native(tcp_header.dest);
    auto offset_and_flags = boost::endian::big_to_native(tcp_header.flags);
    auto flags = offset_and_flags & 0x3FU;
    size_t data_offset = (offset_and_flags >> 12U) * 4U;
    if (data_offset < sizeof(tcphdr) || data_offset > length) { data_offset = sizeof(tcphdr); }

    auto *data = payload + data_offset;
    auto data_length = length - data_offset;

    if (outbound) {
        // Host is the source; a bare SYN from the host opens a new connection
        std::shared_ptr<MuxStream> stream;
        if ((flags & USB_TCP_FLAG_SYN) && !(flags & USB_TCP_FLAG_ACK)) {
            stream = this->m_streams[this->beginStream(source, destination)];
        } else {
            stream = this->streamFor(source, destination);
        }

        if (data_length > 0) {
            auto first_new = stream->getMessages().size();
            stream->sendBytes(data_length, data);
            this->learnServices(stream, first_new);
        }
        if (flags & (USB_TCP_FLAG_RST | USB_TCP_FLAG_FIN)) { stream->close(); }
    } else {
        auto stream = this->streamFor(destination, source);

        if (data_length > 0) {
            auto first_new = stream->getMessages().size();
            stream->receiveBytes(data_length, data);
            this->learnServices(stream, first_new);
        }
        if (flags & (USB_TCP_FLAG_RST | USB_TCP_FLAG_FIN)) { stream->close(); }
    }
}

void USBMUXInterfaceExpert::learnServices(const std::shared_ptr<MuxStream> &stream, size_t firstNewMessage) {
    if (stream->getDevicePort() != LOCKDOWN_PORT) { return; }

    // lockdownd answers StartService with the service name and the device port to connect to
    const auto &messages = stream->getMessages();
    for (auto i = firstNewMessage; i < messages.size(); i++) {
        const auto &message = messages[i];
        if (message.outbound) { continue; }

        auto service = message.stringValue("Service");
        auto port = message.uintValue("Port");
        if (!service.empty() && port > 0 && port <= UINT16_MAX) {
            this->m_serviceNames[static_cast<uint16_t>(port)] = service;
        }
    }
}
