#ifndef _demuxusb_H
#define _demuxusb_H

// External dependencies for formats
#include <pcapng.h>
#include <usb.h>
#include <usbmux.h>

// STL
#include <set>
#include <map>
#include <array>
#include <vector>
#include <functional>
#include <string>
#include <memory>
#include <cassert>
#include <iostream>
#include <utility>

// Boost
#include <boost/filesystem.hpp>
#include <boost/iostreams/device/mapped_file.hpp>

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
    IRECV_K_RECOVERY_MODE_1   = 0x1280,
    IRECV_K_RECOVERY_MODE_2   = 0x1281,
    IRECV_K_RECOVERY_MODE_3   = 0x1282,
    IRECV_K_RECOVERY_MODE_4   = 0x1283,
    IRECV_K_WTF_MODE          = 0x1222,
    IRECV_K_DFU_MODE          = 0x1227
};

#endif