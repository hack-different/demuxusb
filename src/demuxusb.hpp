#ifndef _demuxusb_H
#define _demuxusb_H

// STL
#include <set>
#include <map>
#include <array>
#include <vector>
#include <functional>
#include <string>
#include <memory>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <type_traits>
#include <utility>

// Boost
#include <boost/filesystem.hpp>
#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/endian/conversion.hpp>
#include <boost/format.hpp>


#include <plist/plist.h>

namespace fs = boost::filesystem;
namespace io = boost::iostreams;

typedef std::pair<std::byte *, std::size_t> byte_array;
typedef uint8_t usb_endpoint_id;


#define USB_ENDPOINT_IN(endpoint) (endpoint & 0x80U)
#define USB_ENDPOINT_OUT(endpoint) (!(endpoint & 0x80U))
#define USB_ENDPOINT_ID(endpoint) (endpoint & 0x7FU)

#define APPLE_VID 0x05ac
#define USB_CLASS_APPLICATION_SPECIFIC 0xFF
#define USB_SUBCLASS_DFU 0x01
#define APPLE_SUBCLASS_USBMUX 0xFE
#define APPLE_PROTOCOL_USBMUX2 0x02

enum apple_usb_device_id {
    IRECV_K_RECOVERY_MODE_1 = 0x1280,
    IRECV_K_RECOVERY_MODE_2 = 0x1281,
    IRECV_K_RECOVERY_MODE_3 = 0x1282,
    IRECV_K_RECOVERY_MODE_4 = 0x1283,
    IRECV_K_WTF_MODE = 0x1222,
    IRECV_K_DFU_MODE = 0x1227
};

namespace demuxusb {
    // Raised when a capture is structurally unreadable (bad magic, unsupported byte order, corrupt block framing).
    //  Individual malformed packets are reported and skipped instead.
    class ParseError : public std::runtime_error {
    public:
        using std::runtime_error::runtime_error;
    };

    // Copies a trivially copyable structure out of capture memory.  Capture data is untrusted and unaligned, so all
    //  structure access goes through here rather than reinterpret_cast.
    template<typename T>
    bool readStruct(const std::byte *data, size_t available, T &out) {
        static_assert(std::is_trivially_copyable_v<T>);
        if (data == nullptr || available < sizeof(T)) { return false; }
        std::memcpy(&out, data, sizeof(T));
        return true;
    }

    // Decodes UTF-16LE (as used by USB string descriptors) into UTF-8.
    inline std::string utf16leToUtf8(const std::byte *data, size_t length) {
        std::string result;
        for (size_t i = 0; i + 1 < length; i += 2) {
            uint32_t unit = static_cast<uint8_t>(data[i]) | (static_cast<uint8_t>(data[i + 1]) << 8U);
            uint32_t codepoint = unit;

            if (unit >= 0xD800 && unit <= 0xDBFF && i + 3 < length) {
                uint32_t low = static_cast<uint8_t>(data[i + 2]) | (static_cast<uint8_t>(data[i + 3]) << 8U);
                if (low >= 0xDC00 && low <= 0xDFFF) {
                    codepoint = 0x10000 + ((unit - 0xD800) << 10U) + (low - 0xDC00);
                    i += 2;
                } else {
                    codepoint = 0xFFFD;
                }
            } else if (unit >= 0xD800 && unit <= 0xDFFF) {
                codepoint = 0xFFFD;
            }

            if (codepoint < 0x80) {
                result.push_back(static_cast<char>(codepoint));
            } else if (codepoint < 0x800) {
                result.push_back(static_cast<char>(0xC0 | (codepoint >> 6U)));
                result.push_back(static_cast<char>(0x80 | (codepoint & 0x3FU)));
            } else if (codepoint < 0x10000) {
                result.push_back(static_cast<char>(0xE0 | (codepoint >> 12U)));
                result.push_back(static_cast<char>(0x80 | ((codepoint >> 6U) & 0x3FU)));
                result.push_back(static_cast<char>(0x80 | (codepoint & 0x3FU)));
            } else {
                result.push_back(static_cast<char>(0xF0 | (codepoint >> 18U)));
                result.push_back(static_cast<char>(0x80 | ((codepoint >> 12U) & 0x3FU)));
                result.push_back(static_cast<char>(0x80 | ((codepoint >> 6U) & 0x3FU)));
                result.push_back(static_cast<char>(0x80 | (codepoint & 0x3FU)));
            }
        }
        return result;
    }
}

#endif
