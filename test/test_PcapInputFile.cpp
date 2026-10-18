#include <boost/test/unit_test.hpp>
#include "CaptureBuilder.h"

using namespace fixture;

BOOST_AUTO_TEST_SUITE(pcap_input_file)

    BOOST_AUTO_TEST_CASE(test_classic_pcap_variants) {
        auto transcript = iphone();
        for (bool swapped: {false, true}) {
            for (bool nanoseconds: {false, true}) {
                BOOST_TEST_CONTEXT("swapped " << swapped << " nanoseconds " << nanoseconds) {
                    auto parsed = parse(pcap(LINKTYPE_USB_LINUX_MMAPPED, encode(Format::UsbmonMmapped, transcript),
                                             swapped, nanoseconds), ".pcap");
                    BOOST_TEST(parsed.input->getWarningCount() == 0u);
                    BOOST_REQUIRE_EQUAL(parsed.devices().size(), 1u);
                    BOOST_TEST(parsed.devices()[0]->getProduct() == "iPhone");
                    BOOST_TEST(parsed.devices()[0]->getIdentifier() == 0x105u);
                }
            }
        }
    }

    BOOST_AUTO_TEST_CASE(test_format_is_detected_from_content_not_extension) {
        auto parsed = parse(pcap(LINKTYPE_USB_LINUX, encode(Format::Usbmon, iphone())), ".pcapng");
        BOOST_TEST(parsed.devices()[0]->getManufacturer() == "Apple Inc.");
    }

    BOOST_AUTO_TEST_CASE(test_truncated_record) {
        auto file = pcap(LINKTYPE_USB_LINUX, encode(Format::Usbmon, iphone()));
        file.resize(file.size() - 3);
        auto parsed = parse(file, ".pcap");
        BOOST_TEST(parsed.input->getWarningCount() == 1u);
        BOOST_TEST(parsed.devices()[0]->getProduct() == "iPhone");
    }

BOOST_AUTO_TEST_SUITE_END()
