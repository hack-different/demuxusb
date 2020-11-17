#ifndef _DEUSBMUX_H
#define _DEUSBMUX_H

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

typedef std::pair<std::byte*, std::size_t> byte_array;
typedef uint8_t usb_endpoint_id;


#define USB_ENDPOINT_IN(endpoint) (endpoint & 0x80U)
#define USB_ENDPOINT_OUT(endpoint) (!(endpoint & 0x80U))
#define USB_ENDPOINT_ID(endpoint) (endpoint & 0x7FU)


#endif