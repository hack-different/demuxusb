//
// Created by Rick Mark on 11/14/20.
//

#include "MuxStream.h"

// Messages larger than this are not plausible service messages; treat the stream as not length framed.
static const uint32_t MAX_PLIST_MESSAGE = 64U * 1024U * 1024U;

static const char XML_PLIST_PREFIX[] = "<?xml";
static const char BINARY_PLIST_PREFIX[] = "bplist00";

static void freePlistMemory(char *memory) {
#ifdef HAVE_PLIST_MEM_FREE
    plist_mem_free(memory);
#else
    free(memory);
#endif
}

const char *encodingName(StreamEncoding encoding) {
    switch (encoding) {
        case StreamEncoding::Unknown: return "unknown";
        case StreamEncoding::Plist: return "plist";
        case StreamEncoding::Raw: return "raw";
        case StreamEncoding::TLS: return "tls";
    }
    return "unknown";
}

static bool startsWith(const std::byte *data, size_t length, const char *prefix, size_t prefixLength) {
    return length >= prefixLength && std::memcmp(data, prefix, prefixLength) == 0;
}

static bool looksLikeTLS(const std::vector<std::byte> &buffer) {
    // TLS record: handshake (0x16) or application data (0x17), version 3.x
    return buffer.size() >= 3 && (buffer[0] == std::byte{0x16} || buffer[0] == std::byte{0x17}) &&
           buffer[1] == std::byte{0x03} && static_cast<uint8_t>(buffer[2]) <= 0x04;
}

static uint32_t readBigEndian32(const std::byte *data) {
    uint32_t value;
    std::memcpy(&value, data, sizeof(value));
    return boost::endian::big_to_native(value);
}

std::string MuxMessage::xml() const {
    if (this->plist == nullptr) { return {}; }
    char *xml = nullptr;
    uint32_t length = 0;
    plist_to_xml(this->plist.get(), &xml, &length);
    if (xml == nullptr) { return {}; }
    std::string result(xml, length);
    freePlistMemory(xml);
    return result;
}

std::string MuxMessage::stringValue(const char *key) const {
    if (this->plist == nullptr || plist_get_node_type(this->plist.get()) != PLIST_DICT) { return {}; }
    auto item = plist_dict_get_item(this->plist.get(), key);
    if (item == nullptr || plist_get_node_type(item) != PLIST_STRING) { return {}; }
    uint64_t length = 0;
    const char *value = plist_get_string_ptr(item, &length);
    return value == nullptr ? std::string{} : std::string(value, length);
}

int64_t MuxMessage::uintValue(const char *key) const {
    if (this->plist == nullptr || plist_get_node_type(this->plist.get()) != PLIST_DICT) { return -1; }
    auto item = plist_dict_get_item(this->plist.get(), key);
    if (item == nullptr || plist_get_node_type(item) != PLIST_UINT) { return -1; }
    uint64_t value = 0;
    plist_get_uint_val(item, &value);
    return static_cast<int64_t>(value);
}

std::string MuxMessage::summary() const {
    if (this->plist == nullptr) { return "<undecodable plist>"; }
    if (plist_get_node_type(this->plist.get()) != PLIST_DICT) { return "<non-dictionary plist>"; }

    std::string result;
    for (const char *key: {"Request", "Type", "Service", "Key", "Domain", "Error", "Result", "MessageType",
                           "Command"}) {
        auto value = this->stringValue(key);
        if (!value.empty()) {
            if (!result.empty()) { result += ", "; }
            result += std::string(key) + "=" + value;
        }
    }

    auto port = this->uintValue("Port");
    if (port >= 0) {
        if (!result.empty()) { result += ", "; }
        result += "Port=" + std::to_string(port);
    }

    return result.empty() ? "<dictionary>" : result;
}

void MuxStream::receiveBytes(size_t length, std::byte *start) {
    this->consume(this->m_in, length, start);
}

void MuxStream::sendBytes(size_t length, std::byte *start) {
    this->consume(this->m_out, length, start);
}

void MuxStream::consume(Direction &direction, size_t length, std::byte *start) {
    direction.bytes += length;
    if (length == 0) { return; }

    if (direction.encoding == StreamEncoding::Raw || direction.encoding == StreamEncoding::TLS) { return; }

    direction.buffer.insert(direction.buffer.end(), start, start + length);
    this->drain(direction);
}

void MuxStream::drain(Direction &direction) {
    while (direction.buffer.size() >= sizeof(uint32_t)) {
        if (looksLikeTLS(direction.buffer)) {
            direction.encoding = StreamEncoding::TLS;
            direction.buffer.clear();
            return;
        }

        auto size = readBigEndian32(direction.buffer.data());
        if (size == 0 || size > MAX_PLIST_MESSAGE) {
            direction.encoding = StreamEncoding::Raw;
            direction.buffer.clear();
            return;
        }

        auto *payload = direction.buffer.data() + sizeof(uint32_t);
        auto available = direction.buffer.size() - sizeof(uint32_t);

        if (direction.encoding == StreamEncoding::Unknown) {
            // Decide on framing once enough of the first message is present to see the plist signature
            auto needed = std::min<size_t>(size, sizeof(BINARY_PLIST_PREFIX) - 1);
            if (available < needed) { return; }

            if (startsWith(payload, available, XML_PLIST_PREFIX, sizeof(XML_PLIST_PREFIX) - 1) ||
                startsWith(payload, available, BINARY_PLIST_PREFIX, sizeof(BINARY_PLIST_PREFIX) - 1)) {
                direction.encoding = StreamEncoding::Plist;
            } else {
                direction.encoding = StreamEncoding::Raw;
                direction.buffer.clear();
                return;
            }
        }

        if (available < size) { return; }

        plist_t plist = nullptr;
        if (startsWith(payload, size, BINARY_PLIST_PREFIX, sizeof(BINARY_PLIST_PREFIX) - 1)) {
            plist_from_bin(reinterpret_cast<const char *>(payload), size, &plist);
        } else {
            plist_from_xml(reinterpret_cast<const char *>(payload), size, &plist);
        }

        this->m_messages.push_back(MuxMessage{direction.outbound, size, unique_plist{plist}});
        direction.buffer.erase(direction.buffer.begin(),
                               direction.buffer.begin() + static_cast<std::ptrdiff_t>(sizeof(uint32_t) + size));
    }
}
