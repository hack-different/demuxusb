[![ci.yaml](https://github.com/hack-different/demuxusb/actions/workflows/ci.yml/badge.svg)](https://github.com/hack-different/demuxusb/actions/workflows/ci.yml)

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

- **Capture Processing:** Memory-mapped parsing of PCAPNG (multiple sections and interfaces, Enhanced and Simple Packet Blocks) and classic libpcap files, with the format detected from the file contents.
- **Hostile Input Tolerance:** Captures are treated as untrusted. Every structure is bounds checked; corrupt packets are skipped and reported as warnings, and the analysis keeps whatever was parsed before structural damage. The test suite fuzzes every supported format under AddressSanitizer and UndefinedBehaviorSanitizer.
- **USB Device State Reconstruction:** Tracks device descriptors, configuration trees, interfaces and their alternate settings, endpoints, string descriptors (UTF-16 decoded to UTF-8), and Binary Device Object Store (BOS) descriptors, following `SET_CONFIGURATION` and `SET_INTERFACE`.
- **Re-enumeration Tracking:** When an address re-enumerates as a different device (DFU → Recovery → Normal mode during a restore) a new device session is started, so each mode is reported separately.
- **Transaction Assembly:** Correlates submissions with completions for control, bulk and interrupt transfers, taking OUT data from the submission and IN data from the completion as each capture format requires.
- **Protocol Demultiplexing:** Routes endpoint traffic to specialized **Interface Experts** for DFU, Recovery, and USBMUX protocols.
- **USBMUX & Plist Stream Extraction:** Reassembles usbmux packets split across (or packed into) USB transfers, tracks the TCP connections they tunnel, deserializes length-prefixed XML and binary property lists (`libplist`), detects TLS upgrades, and names service connections from lockdownd `StartService` replies.
- **DFU Analysis:** Reassembles `DFU_DNLOAD` images, follows the DFU state machine from `GETSTATUS`, and flags protocol anomalies (oversized downloads, aborted partial downloads, unexpected requests).
- **Recovery Mode Analysis:** Decodes iBoot commands and responses, file uploads, and console output as spoken by `libirecovery`.
- **Artifact Extraction:** Writes DFU images, recovery mode uploads, the iBoot console and plist transcripts to disk.
- **Forensic & Traffic Statistics:** Accounting of packets, bytes, control transfers, and payload transactions per device and interface.

---

## Supported Protocols and Modes

| Mode / Protocol | USB Vendor / Product ID | Description |
| :--- | :--- | :--- |
| **DFU Mode** | `0x05ac` / `0x1227` (DFU), `0x1222` (WTF) | Device Firmware Upgrade mode; parses DFU functional descriptors and DFU class requests, reassembles downloaded images. Any vendor's DFU class interface (`0xFE`/`0x01`) is also decoded. |
| **Recovery Mode** | `0x05ac` / `0x1280` - `0x1283` | iBoot recovery mode: vendor control commands, `0x41` announced bulk uploads, bulk IN console. |
| **USBMUX (Normal Mode)** | Class `0xFF`, SubClass `0xFE`, Protocol `0x02` | Apple USB Multiplexing protocol (v1 and v2 headers) encapsulating TCP connections and plist-based service transactions. |

---

## Input Capture Formats

Captures may be PCAPNG or classic pcap (either byte order, micro or nanosecond timestamps). Supported link types:

- **macOS Darwin (`LINKTYPE_USB_DARWIN` / 266):** Captured with Wireshark on macOS on the `XHC20` (or similar) USB monitor interface. Darwin 1.0 and 1.1 USB headers.
- **USBPcap (`LINKTYPE_USBPCAP` / 249):** Captured with USBPcap on Windows.
- **Linux usbmon (`LINKTYPE_USB_LINUX` / 189 and `LINKTYPE_USB_LINUX_MMAPPED` / 220):** Captured with Wireshark or `tcpdump -i usbmonN` on Linux.

Big-endian PCAPNG sections are rejected with an error. Packets of any other link type are skipped with a warning.

---

## Prerequisites & Dependencies

To build DeMuxUSB, ensure you have:

- **C++ Compiler:** C++20 compliant compiler (AppleClang 14+, GCC 10+, or Clang 13+)
- **CMake:** Version 3.17 or higher
- **PkgConfig:** For dependency discovery
- **libplist:** Version 2.0+ (`libplist-2.0`)
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

# Compile the library, executable and test suite
cmake --build build
```

The build produces `libdemuxusb.a` (target `demuxusb_lib`), the `demuxusb` command line tool and the `demuxusb_test_suite`.

### Run Tests

```bash
ctest --test-dir build --output-on-failure
```

The tests build synthetic captures in every supported format (see `test/CaptureBuilder.h`), so no capture files are needed. To run them under AddressSanitizer and UndefinedBehaviorSanitizer:

```bash
cmake -B build-asan -DCMAKE_BUILD_TYPE=Debug -DDEMUXUSB_SANITIZE=ON
cmake --build build-asan && ctest --test-dir build-asan --output-on-failure
```

---

## Command Line Usage

### Syntax

```bash
./build/demuxusb [options] <capture_file.pcapng|capture_file.pcap>
```

### Options & Arguments

- `<capture_file>`: Path to the input packet capture in PCAP or PCAPNG format. The format is detected from the file contents.
- `-v`, `--verbose`: Show decoded protocol details: recovery commands and responses, a summary of every plist message, DFU request counts and anomalies.
- `-p`, `--plists`: Print every decoded USBMUX property list as XML.
- `-x`, `--extract <dir>`: Write DFU images, recovery mode uploads, the iBoot console and per-stream plist transcripts to `<dir>/device-<n>-<id>/`.
- `-w`, `--warnings`: List the packets that could not be decoded (by default only their count is printed).
- `-h`, `--help`: Show usage.

The exit status is non-zero when the file cannot be opened or is not a capture. If a capture is damaged part way through, what was parsed before the damage is still reported.

### Example

```bash
./build/demuxusb -v -x extracted captures/restore_session.pcapng
```

### Sample Output

```text
Device ID: 1410000000000005
  VID/PID: 05ac:12a8 (Normal mode)
  Manufacturer: Apple Inc.
  Product: iPhone
  Serial: 00008101-001234567890001E
  Statistics: 7 packets and 793 bytes. (control 10 packets, 198 bytes).
  Interface Experts:
    USBMUXInterfaceExpert (configuration=1, interface=1, version=2):
	-	c000f27e00000001	: 49152 -> 62078 [lockdownd] (2 transactions, in 276 bytes, out 284 bytes, plist)
	-	c001c3cb00000001	: 49153 -> 50123 [com.apple.afc] (0 transactions, in 0 bytes, out 13 bytes, raw/unknown)

      stream c000f27e00000001 (49152 -> 62078):
        > Request=StartService, Service=com.apple.afc
        < Service=com.apple.afc, Port=50123
  Extracted extracted/device-0-1410000000000005/usbmux-c000f27e00000001-49152-62078.plist.txt (622 bytes)


Device ID: 1420000000000007
  VID/PID: 05ac:1281 (Recovery mode)
  Manufacturer: Apple Inc.
  Product: Apple Mobile Device (Recovery Mode)
  Serial: CPID:8101 ECID:0012
  Statistics: 2 packets and 22 bytes. (control 11 packets, 222 bytes).
  Interface Experts:
    RecoveryInterfaceExpert (config=1, int=0) - 1 commands, 1 uploads (12 bytes), 10 console bytes; 2 control packets (8 bytes) and 2 bulk packets (22 bytes)
      > saveenv
      ^ upload of 12 bytes
  Extracted extracted/device-1-1420000000000007/recovery-upload-0.bin (12 bytes)
  Extracted extracted/device-1-1420000000000007/recovery-console.txt (10 bytes)
```

Stream encodings are reported per direction as `host/device` when they differ: `plist` (length-prefixed property lists), `tls` (the session was upgraded to TLS), `raw` (some other framing, e.g. AFC) or `unknown` (no payload seen yet).

---

## C++ API Reference

DeMuxUSB is designed with a modular architecture split into three layers: the **File/Input layer**, the **USB Device state machine**, and the **Protocol Interface Experts**. Link against the `demuxusb_lib` CMake target.

The input and device layers reside in the `demuxusb` namespace (defined across headers in `src/`); the protocol experts are in the global namespace.

### Namespace & Core Definitions

Include header: `#include "demuxusb.hpp"`

- **`byte_array`**: `std::pair<std::byte *, std::size_t>` representing memory buffers and payload slices.
- **`usb_endpoint_id`**: `uint8_t` representing endpoint addresses.
- **`demuxusb::ParseError`**: `std::runtime_error` thrown when a capture is structurally unreadable (not a capture, bad magic, big-endian PCAPNG, corrupt first block).
- **`demuxusb::readStruct(data, available, out)`**: Bounds-checked, alignment-safe copy of a structure out of capture memory.
- **`demuxusb::utf16leToUtf8(data, length)`**: Decodes USB string descriptor text.
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

Header: `#include "file/InputFile.h"` (and `file/PcapNGInputFile.h`, `file/PcapInputFile.h`)

#### `demuxusb::InputFile`
Abstract base class representing an input capture mapped into memory.

- `static std::unique_ptr<InputFile> open(const fs::path &path)`: Opens a capture, choosing `PcapNGInputFile` or `PcapInputFile` from the file's magic number. Throws `ParseError` for anything else.
- `explicit InputFile(fs::path path)`: Opens and memory-maps the input file using `boost::iostreams::mapped_file_source`. Throws `ParseError` if it cannot be opened.
- `virtual void parse() = 0`: Parses the packet stream.
- `std::vector<Device *> getDevices()`: Returns every reconstructed `Device` session, in the order devices first appeared.
- `bool processPacket(uint32_t linktype, const std::byte *data, size_t length)`: Decodes one captured USB packet of the given link type (Darwin, USBPcap, usbmon or mmapped usbmon). Usable directly to feed packets from another source.
- `const std::vector<std::string> &getWarnings()` / `uint64_t getWarningCount()`: Packets that could not be decoded (the first 100 are kept).
- `uint64_t getPacketCount()`: Packets processed.

#### `demuxusb::PcapNGInputFile`
Reads little endian PCAPNG: any number of sections and interfaces (each interface with its own link type), Enhanced and Simple Packet Blocks. Other block types are skipped.

#### `demuxusb::PcapInputFile`
Reads classic libpcap files in either byte order with micro or nanosecond timestamps.

#### `demuxusb::DeviceHandle`
Tracks in-flight transfers on one device address, pairing submissions with completions, and holds the device sessions seen at that address.

- `void beginControl(uint8_t endpoint, uint64_t id, const usb_setup_t &setup, byte_array data = {})`
- `void appendControl(uint8_t endpoint, uint64_t id, byte_array data)`
- `void endControl(uint8_t endpoint, uint64_t id, byte_array data)`
- `void beginBulk(uint8_t endpoint, uint64_t id, byte_array data = {})`
- `void endBulk(uint8_t endpoint, uint64_t id, byte_array data)`
- `void abort(uint8_t endpoint, uint64_t id)`
- `Device *device()`: The current device session. `std::vector<Device *> devices()` returns all of them.

---

### USB Device Model

Header: `#include "usb/Device.h"`

#### `demuxusb::Device`
Represents one enumeration of a USB device. Tracks descriptor data, the active configuration and alternate settings, transfer statistics, and dispatches transfers to interface experts.

- `explicit Device(uint64_t id)`: Constructs a device with a 64-bit bus/location identifier.
- `uint64_t getIdentifier() const`: Returns the device identifier.
- `bool hasDeviceDescriptor() const`, `const usb_device_descriptor &getDeviceDescriptor() const`, `uint16_t getVendorId() const`, `uint16_t getProductId() const`
- `bool isApple() const`: Returns `true` if `idVendor == APPLE_VID`.
- `bool isAppleDFU() const`: Returns `true` for the Apple DFU (`0x1227`) and WTF (`0x1222`) PIDs.
- `bool isAppleRecovery() const`: Returns `true` if device matches any Apple Recovery mode PID (`0x1280`–`0x1283`).
- `std::string getMode() const`: `"DFU"`, `"WTF"`, `"Recovery"`, `"Normal"` or `"Unknown"`.
- `std::string getProduct()`, `getManufacturer()`, `getSerial()`, `getString(uint8_t index)`: UTF-8 decoded string descriptors, or `"<unknown>"`.
- `uint64_t getPacketCount() const`, `getByteCount() const`, `getControlPacketCount() const`, `getControlByteCount() const`
- `uint8_t getCurrentConfiguration() const`, `uint8_t getAlternateSetting(uint8_t interface) const`
- `const std::map<uint8_t, usb_configuration> &getConfigurations() const`: Configurations keyed by `bConfigurationValue`.
- `const std::vector<bos_descriptor> &getBOSDescriptors() const`
- `std::vector<std::shared_ptr<InterfaceExpert>> getExperts()`: Every distinct expert attached to any interface.
- `const std::vector<std::string> &getAnomalies() const`: Malformed or mismatched descriptors.
- `void processControlPacketIn/Out(uint8_t endpoint, usb_setup_t setup, byte_array data)`
- `void processBulkPacketIn/Out(uint8_t endpoint, byte_array data)`

#### `demuxusb::usb_configuration`
- `usb_config_descriptor descriptor`, `usb_dfu_functional_descriptor dfu_descriptor` (with `has_dfu_descriptor`)
- `std::vector<usb_interface> interfaces`: One entry per interface alternate setting, each with its `usb_interface_descriptor`, endpoints and expert (shared by all alternates of an interface).
- `static bool parse(usb_configuration &config, const std::byte *data, size_t size)`: Parses a configuration descriptor set; returns `false` if it is malformed.
- `usb_interface *find(uint8_t interface, uint8_t alternate)`

Bulk traffic is routed by endpoint address within the active configuration and alternate setting. When `SET_CONFIGURATION` or `SET_INTERFACE` happened before the capture started, the single configuration and the first alternate declaring the endpoint are used.

---

### Interface Experts & Protocol Parsers

Header: `#include "protocol/InterfaceExpert.h"`

#### `InterfaceExpert`
Base class for interface traffic inspectors.

- `InterfaceExpert(uint8_t configuration, uint8_t interface)`
- `virtual void processControlIn(const usb_setup_t &setup, byte_array data)` / `processControlOut(...)`: Class and vendor requests addressed to the interface (or to the device, for the first interface with an expert).
- `virtual void processBulkIn(byte_array data)` / `processBulkOut(byte_array data)`
- `virtual bool isEmpty()`: Returns `true` if no traffic has been recorded on this interface.
- `virtual std::string toString()`: One line summary.
- `virtual std::string details()`: Verbose decode (used by `--verbose`).
- `virtual std::vector<ExtractedArtifact> getArtifacts()`: Named files (`name`, `data`) the expert can extract (used by `--extract`).

#### `DFUInterfaceExpert`
Header: `#include "protocol/DFUInterfaceExpert.h"`

- `std::vector<std::vector<std::byte>> getImages()`: Images terminated by a zero length `DFU_DNLOAD`, then any partial image.
- `int getLastState() const`, `int getLastStatus() const`, `static std::string stateName(int state)`
- `const std::vector<std::string> &getAnomalies() const`: Downloads larger than `wTransferSize`, `DFU_ABORT` after a partial download, unexpected requests, data length / `wLength` mismatches.

#### `RecoveryInterfaceExpert`
Header: `#include "protocol/RecoveryInterfaceExpert.h"`

One expert spans all interfaces of a recovery mode device so commands and uploads stay in order.

- `const std::vector<RecoveryPayload> &getPayloads() const`: `Command`, `Response` and `Upload` payloads in capture order.
- `std::vector<std::string> getCommands()`
- `const std::string &getConsole() const`: iBoot console output read over bulk IN.

#### `USBMUXInterfaceExpert`
Header: `#include "protocol/USBMUXInterfaceExpert.h"`
Specialized expert for Apple USBMUX2 (`bInterfaceClass = 0xFF`, `bInterfaceSubClass = 0xFE`, `bInterfaceProtocol = 0x02`).

- Reassembles usbmux packets per direction by their header length, accepting v1 (8 byte) and v2 (`mux_header`, magic `0xFEEDFACE` / `0xFACEFACE`) headers, and resynchronises after lost framing.
- Records the negotiated protocol version (`getProtocolVersion()`).
- Opens a `MuxStream` for each host `SYN`, closes it on `FIN` / `RST`, and creates streams implicitly for connections opened before the capture.
- `const std::map<uint64_t, std::shared_ptr<MuxStream>> &getStreams() const`
- `uint64_t getResyncCount() const`

---

### MuxStream & Plist Services

Header: `#include "protocol/MuxStream.h"`

#### `MuxStream`
Represents an individual TCP stream multiplexed over USBMUX between a host port and a device port.

- `MuxStream(uint16_t host, uint16_t device)`
- `uint16_t getHostPort()`, `uint16_t getDevicePort()`
- `uint64_t getBytesIn()`, `uint64_t getBytesOut()`: Payload bytes device → host and host → device.
- `size_t getTransactionCount()`: Number of decoded plist messages.
- `StreamEncoding getEncodingIn()`, `getEncodingOut()`: `Unknown`, `Plist`, `Raw` or `TLS`.
- `bool shouldMaterializePlist()`: `true` while the host → device direction may still carry plists.
- `const std::vector<MuxMessage> &getMessages() const`: Decoded messages in order.
- `const std::string &getServiceName()`: `lockdownd`, or the service lockdownd reported for the device port.
- `bool isClosed()`
- `void receiveBytes(size_t length, std::byte *start)` / `void sendBytes(size_t length, std::byte *start)`: Feed device → host and host → device bytes.

#### `MuxMessage`
- `bool outbound`, `uint32_t size`, `unique_plist plist`
- `std::string xml() const`, `std::string summary() const`
- `std::string stringValue(const char *key) const`, `int64_t uintValue(const char *key) const`

---

## C++ API Example

The following example demonstrates how to use the DeMuxUSB C++ library programmatically in an analysis application:

```cpp
#include <iostream>
#include <boost/filesystem.hpp>
#include "file/InputFile.h"
#include "protocol/USBMUXInterfaceExpert.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: analyze <capture.pcapng>" << std::endl;
        return 1;
    }

    std::unique_ptr<demuxusb::InputFile> input;
    try {
        input = demuxusb::InputFile::open(boost::filesystem::path(argv[1]));
        input->parse();
    } catch (const demuxusb::ParseError &error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }

    for (demuxusb::Device *device : input->getDevices()) {
        std::cout << "Found Device ID: 0x" << std::hex << device->getIdentifier() << std::dec
                  << " (" << device->getMode() << " mode)" << std::endl;
        std::cout << "  Product: " << device->getProduct() << std::endl;
        std::cout << "  Serial:  " << device->getSerial() << std::endl;

        for (const auto &expert : device->getExperts()) {
            if (expert->isEmpty()) { continue; }
            std::cout << "  " << expert->toString() << std::endl;

            if (auto usbmux = std::dynamic_pointer_cast<USBMUXInterfaceExpert>(expert)) {
                for (const auto &[id, stream] : usbmux->getStreams()) {
                    for (const auto &message : stream->getMessages()) {
                        std::cout << "    " << (message.outbound ? "> " : "< ") << message.summary() << std::endl;
                    }
                }
            }
        }
    }

    return 0;
}
```

---

## License

This project is licensed under the **MIT License**. See the [LICENSE](LICENSE) file for complete details.
