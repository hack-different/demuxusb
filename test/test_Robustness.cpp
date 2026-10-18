// Captures are untrusted input: a corrupt or hostile file must never crash the analyser.  These tests mutate valid
//  captures of every supported format and only require that parsing terminates (with or without a ParseError).
//  Build with -DDEMUXUSB_SANITIZE=ON to have out of bounds accesses fail the test.

#include <boost/test/unit_test.hpp>
#include "CaptureBuilder.h"
#include "protocol/USBMUXInterfaceExpert.h"

using namespace fixture;

static Transcript busySession() {
    auto transcript = iphone(true);
    transcript.bulkOut(0x04, muxVersion(2, true));
    transcript.bulkOut(0x04, muxPacket(MUX_PROTO_TCP, tcpSegment(49152, 62078, USB_TCP_FLAG_SYN), true));
    transcript.bulkOut(0x04, muxPacket(MUX_PROTO_TCP, tcpSegment(49152, 62078, USB_TCP_FLAG_ACK, lengthPrefixed(
            xmlPlist("<key>Request</key><string>QueryType</string>"))), true));
    transcript.bulkIn(0x85, muxPacket(MUX_PROTO_TCP, tcpSegment(62078, 49152, USB_TCP_FLAG_ACK, lengthPrefixed(
            xmlPlist("<key>Port</key><integer>50000</integer><key>Service</key><string>x</string>"))), false));
    transcript.control(setup(0x21, 1, 0, 1, 4), bytes({1, 2, 3, 4}));
    transcript.control(setup(0x40, 0, 0, 0, 3), bytes({'g', 'o', 0}));
    return transcript;
}

static void parseQuietly(const Bytes &contents, const std::string &extension) {
    try {
        auto parsed = parse(contents, extension);
        for (auto *device: parsed.devices()) {
            for (const auto &expert: device->getExperts()) {
                (void) expert->toString();
                (void) expert->details();
                (void) expert->getArtifacts();
            }
        }
    } catch (const demuxusb::ParseError &) {
        // Rejecting a file is fine; crashing is not
    }
}

BOOST_AUTO_TEST_SUITE(robustness)

    BOOST_AUTO_TEST_CASE(test_mutated_captures_do_not_crash) {
        std::mt19937 random(0x5eed);

        std::vector<std::pair<Bytes, std::string>> seeds;
        for (auto format: {Format::Darwin, Format::Darwin_1_1, Format::USBPcap, Format::Usbmon}) {
            seeds.emplace_back(capture(format, busySession()), ".pcapng");
        }
        seeds.emplace_back(pcap(LINKTYPE_USB_LINUX_MMAPPED, encode(Format::UsbmonMmapped, busySession())), ".pcap");

        for (const auto &[seed, extension]: seeds) {
            for (int iteration = 0; iteration < 300; iteration++) {
                auto mutated = seed;
                auto mutations = 1 + random() % 16;
                for (unsigned m = 0; m < mutations; m++) {
                    auto position = random() % mutated.size();
                    switch (random() % 4) {
                        case 0:
                            mutated[position] = static_cast<std::byte>(random());
                            break;
                        case 1:
                            mutated[position] = std::byte{0xFF};
                            break;
                        case 2:
                            mutated[position] = std::byte{0x00};
                            break;
                        default:
                            mutated[position] ^= static_cast<std::byte>(1U << (random() % 8));
                    }
                }
                if (random() % 4 == 0) { mutated.resize(random() % mutated.size() + 1); }

                parseQuietly(mutated, extension);
            }
        }
        BOOST_TEST(true);
    }

    BOOST_AUTO_TEST_CASE(test_random_files_do_not_crash) {
        std::mt19937 random(0xbad);
        for (int iteration = 0; iteration < 200; iteration++) {
            Bytes file(random() % 512);
            for (auto &value: file) { value = static_cast<std::byte>(random()); }
            // Give the junk a valid magic so it reaches the block parsers
            if (file.size() >= 4) {
                uint32_t magic = iteration % 2 ? TYPE_SECTION_HEADER : PCAP_MAGIC_MICROSECONDS;
                std::memcpy(file.data(), &magic, sizeof(magic));
            }
            parseQuietly(file, ".pcapng");
        }
        BOOST_TEST(true);
    }

BOOST_AUTO_TEST_SUITE_END()
