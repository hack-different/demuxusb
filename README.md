# DeMuxUSB - Analyze iDevice USB Protocols

DeMuxUSB (a play on `usbmuxd`) is a C++20 program and library suite designed to capture, demultiplex, and analyze USB sessions with Apple iDevices.

DeMuxUSB was created because every major iOS version and new Apple device architecture (e.g., Apple Silicon / M-series chips) requires reverse engineering of the recovery and restore processes used by tools such as Apple Configurator and `idevicerestore`. This toolset enables taking USB packet captures of restore workflows and rapidly identifying protocol behavior, changes between firmware versions, and hardware-specific variations.

Beyond reverse engineering, DeMuxUSB provides digital forensics capabilities to analyze (particularly out-of-band) device restores against known baselines to detect failures, deviations, or unauthorized modifications—such as attempts by a technician workstation or untrusted host to execute exploits or install jailbreaks (e.g., checkra1n).

---

## Table of Contents

- [Features](#features)
- [Supported Protocols and Modes](#supported-protocols-and-modes)
- [Input Capture Formats](#input-capture-formats)
- [Prerequisites & Dependencies](#prerequisites--dependencies)
- [Building & Testing](#building--testing)
- [Command Line Usage](#command-line-usage)
- [C++ API Reference](#c-api-reference)
  - [Namespace & Core Definitions](#namespace--core-definitions)
  - [Input Layer](#input-layer)
  - [USB Device Model](#usb-device-model)
  - [Interface Experts & Protocol Parsers](#interface-experts--protocol-parsers)
  - [MuxStream & Plist Services](#muxstream--plist-services)
- [C++ API Example](#c-api-example)
- [License](#license)

---

## Features

- **PcapNG Capture Processing:** Memory-mapped parsing of PCAPNG captures from macOS Darwin USB (`LINKTYPE_USB_DARWIN`) and Windows/Linux USBPcap (`LINKTYPE_USBPCAP`).
- **USB Device State Reconstruction:** Tracks device descriptors, configuration trees, interfaces, endpoints, string descriptors, and Binary Device Object Store (BOS) descriptors.
- **In-Flight Transaction Assembly:** Correlates USB control setup packets (`usb_setup_t`) with completion stages across all endpoints.
- **Protocol Demultiplexing:** Automatically routes endpoint traffic to specialized **Interface Experts** for DFU, Recovery, and USBMUX protocols.
- **USBMUX & Plist Stream Extraction:** Tracks bidirectional TCP streams multiplexed over USB, decodes packet streams, and deserializes Apple property lists (`plist_t` via `libplist`).
- **Forensic & Traffic Statistics:** Comprehensive accounting of packets, bytes, control transfers, and payload transactions per device and interface.

---

## Supported Protocols and Modes

| Mode / Protocol | USB Vendor / Product ID | Description |
| :--- | :--- | :--- |
| **DFU Mode** | `0x05ac` / `0x1227` (DFU), `0x1222` (WTF) | Device Firmware Upgrade mode interface; parses DFU functional descriptors and control transfer commands. |
| **Recovery Mode** | `0x05ac` / `0x1280` - `0x1283` | iBoot recovery mode handling bulk and control communications during system restore. |
| **USBMUX (Normal Mode)** | Class `0xFF`, SubClass `0xFE`, Protocol `0x02` | Apple USB Multiplexing protocol encapsulating TCP connections and plist-based service transactions. |

---

## Input Capture Formats

Currently supported front-end input handlers:

- **macOS Darwin PcapNG (`LINKTYPE_USB_DARWIN` / 266):** Captured via tools like Wireshark on macOS capturing on the `XHC20` USB monitor interface. Supports both Darwin 1.0 and 1.1 USB headers.
- **USBPcap PcapNG (`LINKTYPE_USBPCAP` / 249):** Captured via USBPcap on Windows / Linux systems.

---

## Prerequisites & Dependencies

To build DeMuxUSB, ensure you have:

- **C++ Compiler:** C++20 compliant compiler (AppleClang 12+, GCC 10+, or Clang 11+)
- **CMake:** Version 3.17 or higher
- **PkgConfig:** For dependency discovery
- **libplist:** Version 2.0+ (`libplist-2.0` and C++ bindings `libplist++`)
- **Boost Libraries:**
  - `Boost.Container`
  - `Boost.Filesystem`
  - `Boost.Iostreams`
  - `Boost.UnitTestFramework` (for test suite)

### Installing Dependencies

#### macOS (Homebrew)
```bash
brew install cmake pkg-config boost libplist
```

#### Ubuntu / Debian
```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake pkg-config \
    libboost-container-dev libboost-filesystem-dev \
    libboost-iostreams-dev libboost-test-dev libplist-dev
```

---

## Building & Testing

### Build

```bash
# Configure the build directory
cmake -B build -S .

# Compile the executable and test suite
cmake --build build
```

### Run Tests

```bash
ctest --test-dir build --output-on-failure
```

---

## Command Line Usage

### Syntax

```bash
./build/demuxusb <capture_file.pcapng|capture_file.pcap>
```

### Options & Arguments

- `<capture_file.pcapng|capture_file.pcap>`: Path to the input packet capture file in PCAP or PCAPNG format.

### Example

```bash
./build/demuxusb captures/restore_session.pcapng
```

### Sample Output

```text
Device ID: 100000000000001
  Manufacturer: Apple Inc.
  Product: iPhone
  Statistics: 24890 packets and 18392010 bytes. (control 142 packets, 9088 bytes).
  Interface Experts:
    USBMUXInterfaceExpert (configuration=1, interface=1):
	-	100010001	: 49152 -> 62078 (34 transactions, in 4102 bytes, out 8204 bytes)
	-	100010002	: 49153 -> 62078 (120 transactions, in 102400 bytes, out 51200 bytes)
```

---

## C++ API Reference

DeMuxUSB is designed with a modular architecture split into three layers: the **File/Input layer**, the **USB Device state machine**, and the **Protocol Interface Experts**.

All classes and helper functions reside in the `demuxusb` namespace (defined across headers in `src/`).

### Namespace & Core Definitions

Include header: `#include "demuxusb.hpp"`

- **`byte_array`**: `std::pair<std::byte *, std::size_t>` representing memory buffers and payload slices.
- **`usb_endpoint_id`**: `uint8_t` representing endpoint addresses.
- **Endpoint Macros:**
  - `USB_ENDPOINT_IN(ep)`: Evaluates whether endpoint direction is IN (`ep & 0x80U`).
  - `USB_ENDPOINT_OUT(ep)`: Evaluates whether endpoint direction is OUT (`!(ep & 0x80U)`).
  - `USB_ENDPOINT_ID(ep)`: Extracts the 7-bit endpoint index (`ep & 0x7FU`).
- **Device Constants:**
  - `APPLE_VID`: `0x05ac`
  - `IRECV_K_DFU_MODE` (`0x1227`), `IRECV_K_WTF_MODE` (`0x1222`)
  - `IRECV_K_RECOVERY_MODE_1` through `IRECV_K_RECOVERY_MODE_4` (`0x1280` - `0x1283`)

---

### Input Layer

Header: `#include "file/InputFile.h"` and `#include "file/PcapNGInputFile.h"`

#### `demuxusb::InputFile`
Abstract base class representing an input capture stream mapped into memory.

- `explicit InputFile(fs::path path)`: Opens and memory-maps the input file using `boost::iostreams::mapped_file_source`.
- `virtual void parse() = 0`: Pure virtual method implemented by subclasses to parse packet streams.
- `std::vector<Device *> getDevices()`: Returns pointers to all reconstructed `Device` objects discovered in the capture.

#### `demuxusb::PcapNGInputFile`
Inherits from `InputFile`. Parses PCAPNG blocks:

- `explicit PcapNGInputFile(fs::path input)`: Constructs a PCAPNG file parser.
- `void parse() override`: Iterates through PCAPNG Section Headers, Interface Description Blocks (`TIME_INTERFACE_DESCRIPTOR_BLOCK`), and Enhanced Packet Blocks (`TYPE_ENHANCED_PACKET_BLOCK`).
- `bool process_dawrin_packet(std::byte *location)`: Parses Darwin USB 1.0 / 1.1 headers (`darwin_usb_header_1_1_t`), tracks control/bulk I/O submit and completion events, and forwards transfers to `DeviceHandle`.
- `bool process_usbpcap_packet(std::byte *location)`: Parses USBPcap transfer headers and control buffer stages.

#### `demuxusb::DeviceHandle`
Internal state helper that tracks active, in-flight USB control setups (`usb_setup_t`) and bulk transactions matching submissions with completions.

- `void beginControl(uint8_t endpoint, uint64_t id, usb_setup_t *setup)`
- `usb_setup_t endControl(uint8_t endpoint, uint64_t id)`
- `void beginBulk(uint8_t endpoint, uint64_t id)`
- `void endBulk(uint8_t endpoint, uint64_t id)`
- `Device *device()`: Returns the underlying `Device` instance.

---

### USB Device Model

Header: `#include "usb/Device.h"`

#### `demuxusb::Device`
Represents an individual physical or virtual USB device. Tracks enumeration state, descriptor data, transfer statistics, and dispatches transfers to interface experts.

- `explicit Device(uint64_t id)`: Constructs a device with a 64-bit unique bus/location identifier.
- `uint64_t getIdentifier() const`: Returns the device identifier.
- `bool isApple() const`: Returns `true` if `idVendor == APPLE_VID`.
- `bool isAppleDFU() const`: Returns `true` if device matches Apple DFU mode PID (`0x1227`).
- `bool isAppleRecovery() const`: Returns `true` if device matches any Apple Recovery mode PID (`0x1280`–`0x1283`).
- `std::wstring getProduct()`: Decodes the product string descriptor.
- `std::wstring getManufacturer()`: Decodes the manufacturer string descriptor.
- `std::wstring getSerial()`: Decodes the device serial number string descriptor.
- `std::wstring getString(uint8_t index)`: Retrieves a decoded UTF-16 string descriptor by index.
- `uint64_t getPacketCount() const`: Total packet count processed for the device.
- `uint64_t getByteCount() const`: Total byte count processed for the device.
- `uint64_t getControlPacketCount() const`: Total control packet count.
- `uint64_t getControlByteCount() const`: Total control byte count.
- `std::vector<std::shared_ptr<InterfaceExpert>> getExperts()`: Returns all active interface experts attached to configured interfaces.
- `void processControlPacketIn(uint8_t endpoint, usb_setup_t setup, byte_array data)`
- `void processControlPacketOut(uint8_t endpoint, usb_setup_t setup, byte_array data)`
- `void processBulkPacketIn(uint8_t endpoint, byte_array data)`
- `void processBulkPacketOut(uint8_t endpoint, byte_array data)`

#### `demuxusb::usb_configuration`
Descriptor structure holding parsed USB configurations:
- `static void parse(usb_configuration &config, std::byte *data, size_t size)`: Parses raw descriptor blobs into interface and endpoint structures, attaching experts automatically.

---

### Interface Experts & Protocol Parsers

Header: `#include "protocol/InterfaceExpert.h"`

#### `InterfaceExpert`
Base class for endpoint traffic inspectors.

- `InterfaceExpert(uint8_t configuration, uint8_t interface)`
- `virtual void processControlIn(byte_array data)`
- `virtual void processControlOut(byte_array data)`
- `virtual void processBulkIn(byte_array data)`
- `virtual void processBulkOut(byte_array data)`
- `virtual bool isEmpty()`: Returns `true` if no traffic has been recorded on this interface.
- `virtual std::string toString()`: Formats interface configuration, packet counts, and byte counters into a human-readable summary.

#### `DFUInterfaceExpert`
Header: `#include "protocol/DFUInterfaceExpert.h"`
Specialized expert for Apple DFU mode interfaces. Records control transfers and DFU functional state changes.

#### `RecoveryInterfaceExpert`
Header: `#include "protocol/RecoveryInterfaceExpert.h"`
Specialized expert for Apple Recovery mode interfaces. Captures recovery mode payloads and command sequences.

#### `USBMUXInterfaceExpert`
Header: `#include "protocol/USBMUXInterfaceExpert.h"`
Specialized expert for Apple USBMUX2 (`bInterfaceClass = 0xFF`, `bInterfaceSubClass = 0xFE`, `bInterfaceProtocol = 0x02`).

- Parses `mux_header` (`USBMUX_MAGIC_IN = 0xFACEFACE`, `USBMUX_MAGIC_OUT = 0xFEEDFACE`).
- Inspects TCP headers (`tcphdr`) encapsulated inside USBMUX frames.
- Detects `SYN` packets to establish stream sessions (`MuxStream`).
- Demultiplexes incoming and outgoing data bytes into active `MuxStream` instances.

---

### MuxStream & Plist Services

Header: `#include "protocol/MuxStream.h"`

#### `MuxStream`
Represents an individual TCP stream multiplexed over USBMUX between a host port and a device port.

- `MuxStream(uint16_t host, uint16_t device)`
- `uint16_t getHostPort()`: Returns the client host port number.
- `uint16_t getDevicePort()`: Returns the destination device port number.
- `uint64_t getBytesIn()`: Total inbound bytes received on this stream.
- `uint64_t getBytesOut()`: Total outbound bytes sent on this stream.
- `size_t getTransactionCount()`: Returns total count of inbound and outbound transactions.
- `bool shouldMaterializePlist()`: Checks if port traffic should be deserialized as Apple XML/Binary Property Lists.
- `void receiveBytes(size_t length, std::byte *start)`: Feeds inbound data stream bytes for reassembly and parsing.
- `void sendBytes(size_t length, std::byte *start)`: Feeds outbound data stream bytes for reassembly and parsing.

---

## C++ API Example

The following example demonstrates how to use the DeMuxUSB C++ library programmatically in an analysis application:

```cpp
#include <iostream>
#include <memory>
#include <boost/filesystem.hpp>
#include "file/PcapNGInputFile.h"
#include "usb/Device.h"
#include "protocol/InterfaceExpert.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: analyze <capture.pcapng>" << std::endl;
        return 1;
    }

    boost::filesystem::path capture_path(argv[1]);

    // Create and parse the PCAPNG file
    auto input_file = std::make_unique<demuxusb::PcapNGInputFile>(capture_path);
    input_file->parse();

    // Iterate over all discovered USB devices
    for (demuxusb::Device *device : input_file->getDevices()) {
        std::wcout << L"Found Device ID: 0x" << std::hex << device->getIdentifier() << std::endl;
        std::wcout << L"  Manufacturer: " << device->getManufacturer() << std::endl;
        std::wcout << L"  Product:      " << device->getProduct() << std::endl;
        std::wcout << L"  Serial:       " << device->getSerial() << std::endl;

        if (device->isAppleDFU()) {
            std::cout << "  -> Device is in Apple DFU Mode" << std::endl;
        } else if (device->isAppleRecovery()) {
            std::cout << "  -> Device is in Apple Recovery Mode" << std::endl;
        }

        std::cout << "  Total Packets: " << std::dec << device->getPacketCount()
                  << " (" << device->getByteCount() << " bytes)" << std::endl;

        // Inspect interface experts
        for (const auto &expert : device->getExperts()) {
            if (!expert->isEmpty()) {
                std::cout << "  " << expert->toString() << std::endl;
            }
        }
    }

    return 0;
}
```

---

## License

This project is licensed under the **MIT License**. See the [LICENSE](LICENSE) file for complete details.