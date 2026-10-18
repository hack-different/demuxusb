//
// Created by Rick Mark on 11/14/20.
//

#ifndef demuxusb_CMUXSTREAM_H
#define demuxusb_CMUXSTREAM_H

#include "../demuxusb.hpp"

#define LOCKDOWN_PORT 62078

struct PlistDeleter {
    void operator()(void *plist) const { plist_free(static_cast<plist_t>(plist)); }
};

typedef std::unique_ptr<void, PlistDeleter> unique_plist;

// How the payload of one direction of a stream is framed.  Lockdown and most lockdown services exchange
//  property lists prefixed with a big endian 32 bit length; once a session is upgraded the bytes become TLS.
enum class StreamEncoding {
    Unknown,
    Plist,
    Raw,
    TLS
};

const char *encodingName(StreamEncoding encoding);

inline std::ostream &operator<<(std::ostream &stream, StreamEncoding encoding) {
    return stream << encodingName(encoding);
}

// One deserialized property list message.  outbound is host -> device.
struct MuxMessage {
    bool outbound;
    uint32_t size;
    unique_plist plist;

    [[nodiscard]] std::string xml() const;

    // Returns the string value of a top level dictionary key, or an empty string.
    [[nodiscard]] std::string stringValue(const char *key) const;

    // Returns the unsigned integer value of a top level dictionary key, or -1.
    [[nodiscard]] int64_t uintValue(const char *key) const;

    // A short human readable description (Request / Type / Service / Error keys).
    [[nodiscard]] std::string summary() const;
};

// A TCP connection tunnelled over USBMUX between a host port and a device port.
class MuxStream {
public:
    MuxStream(uint16_t host, uint16_t device) : m_host{host}, m_device{device} {};

    uint16_t getHostPort() { return this->m_host; }

    uint16_t getDevicePort() { return this->m_device; }

    uint64_t getBytesIn() { return this->m_in.bytes; }

    uint64_t getBytesOut() { return this->m_out.bytes; }

    size_t getTransactionCount() { return this->m_messages.size(); }

    bool shouldMaterializePlist() {
        return this->m_out.encoding == StreamEncoding::Unknown || this->m_out.encoding == StreamEncoding::Plist;
    }

    StreamEncoding getEncodingIn() { return this->m_in.encoding; }

    StreamEncoding getEncodingOut() { return this->m_out.encoding; }

    // Device -> host bytes
    void receiveBytes(size_t length, std::byte *start);

    // Host -> device bytes
    void sendBytes(size_t length, std::byte *start);

    [[nodiscard]] const std::vector<MuxMessage> &getMessages() const { return this->m_messages; }

    const std::string &getServiceName() { return this->m_serviceName; }

    void setServiceName(std::string name) { this->m_serviceName = std::move(name); }

    bool isClosed() { return this->m_closed; }

    void close() { this->m_closed = true; }

protected:
    struct Direction {
        bool outbound;
        StreamEncoding encoding{StreamEncoding::Unknown};
        uint64_t bytes{};
        std::vector<std::byte> buffer{};
    };

    void consume(Direction &direction, size_t length, std::byte *start);

    void drain(Direction &direction);

    uint16_t m_host;
    uint16_t m_device;
    bool m_closed{false};
    std::string m_serviceName{};
    Direction m_in{false};
    Direction m_out{true};
    std::vector<MuxMessage> m_messages{};
};


#endif //demuxusb_CMUXSTREAM_H
