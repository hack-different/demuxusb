//
// Created by Rick Mark on 11/16/20.
//

#include <boost/test/unit_test.hpp>
#include "CaptureBuilder.h"
#include "protocol/USBMUXInterfaceExpert.h"

using namespace fixture;

static Transcript lockdownSession() {
    auto transcript = iphone();
    transcript.bulkOut(0x04, muxVersion(2, true));
    transcript.bulkIn(0x85, muxVersion(2, false));
    transcript.bulkOut(0x04, muxPacket(MUX_PROTO_TCP, tcpSegment(49152, 62078, USB_TCP_FLAG_SYN), true));
    transcript.bulkIn(0x85, muxPacket(MUX_PROTO_TCP, tcpSegment(62078, 49152, USB_TCP_FLAG_SYN | USB_TCP_FLAG_ACK), false));
    transcript.bulkOut(0x04, muxPacket(MUX_PROTO_TCP, tcpSegment(49152, 62078, USB_TCP_FLAG_ACK, lengthPrefixed(
            xmlPlist("<key>Label</key><string>test</string><key>Request</key><string>QueryType</string>"))), true));
    transcript.bulkIn(0x85, muxPacket(MUX_PROTO_TCP, tcpSegment(62078, 49152, USB_TCP_FLAG_ACK, lengthPrefixed(
            xmlPlist("<key>Request</key><string>QueryType</string><key>Type</key>"
                     "<string>com.apple.mobile.lockdown</string>"))), false));
    return transcript;
}

BOOST_AUTO_TEST_SUITE(pcapng_input_file)

    BOOST_AUTO_TEST_CASE(test_every_link_type_decodes_the_same_session) {
        for (auto format: {Format::Darwin, Format::Darwin_1_1, Format::USBPcap, Format::Usbmon,
                           Format::UsbmonMmapped}) {
            BOOST_TEST_CONTEXT("format " << static_cast<int>(format)) {
                auto parsed = parse(capture(format, lockdownSession()));
                BOOST_TEST(parsed.input->getWarningCount() == 0u);

                auto devices = parsed.devices();
                BOOST_REQUIRE_EQUAL(devices.size(), 1u);
                auto *device = devices[0];

                BOOST_TEST(device->getVendorId() == APPLE_VID);
                BOOST_TEST(device->getProductId() == 0x12A8);
                BOOST_TEST(device->getMode() == "Normal");
                BOOST_TEST(device->getManufacturer() == "Apple Inc.");
                BOOST_TEST(device->getProduct() == "iPhone");
                BOOST_TEST(device->getSerial() == "00008101-001234567890001E");
                BOOST_TEST(device->getCurrentConfiguration() == 1);

                auto usbmux = expertOf<USBMUXInterfaceExpert>(device);
                BOOST_REQUIRE(usbmux != nullptr);
                BOOST_TEST(usbmux->getProtocolVersion() == 2u);
                BOOST_REQUIRE_EQUAL(usbmux->getStreams().size(), 1u);

                auto stream = usbmux->getStreams().begin()->second;
                BOOST_TEST(stream->getHostPort() == 49152);
                BOOST_TEST(stream->getDevicePort() == 62078);
                BOOST_TEST(stream->getServiceName() == "lockdownd");
                BOOST_REQUIRE_EQUAL(stream->getMessages().size(), 2u);
                BOOST_TEST(stream->getMessages()[0].outbound);
                BOOST_TEST(stream->getMessages()[0].stringValue("Request") == "QueryType");
                BOOST_TEST(!stream->getMessages()[1].outbound);
                BOOST_TEST(stream->getMessages()[1].stringValue("Type") == "com.apple.mobile.lockdown");
            }
        }
    }

    BOOST_AUTO_TEST_CASE(test_unsupported_interface_is_skipped_without_hanging) {
        Bytes file;
        appendBlock(file, TYPE_SECTION_HEADER, sectionHeaderBody());
        appendBlock(file, TIME_INTERFACE_DESCRIPTOR_BLOCK, interfaceBody(1 /* Ethernet */));
        appendBlock(file, TIME_INTERFACE_DESCRIPTOR_BLOCK, interfaceBody(LINKTYPE_USB_DARWIN));
        appendBlock(file, TYPE_ENHANCED_PACKET_BLOCK, enhancedPacketBody(0, bytes({1, 2, 3, 4, 5, 6})));
        for (const auto &packet: encode(Format::Darwin, lockdownSession())) {
            appendBlock(file, TYPE_ENHANCED_PACKET_BLOCK, enhancedPacketBody(1, packet));
        }
        appendBlock(file, TYPE_ENHANCED_PACKET_BLOCK, enhancedPacketBody(0, bytes({7, 8})));

        auto parsed = parse(file);
        BOOST_TEST(parsed.input->getWarningCount() == 1u);
        BOOST_REQUIRE_EQUAL(parsed.devices().size(), 1u);
        BOOST_TEST(parsed.devices()[0]->getProduct() == "iPhone");
    }

    BOOST_AUTO_TEST_CASE(test_multiple_sections_reset_interfaces) {
        auto packets = encode(Format::Darwin, lockdownSession());
        auto half = packets.size() / 2;

        Bytes file;
        appendBlock(file, TYPE_SECTION_HEADER, sectionHeaderBody());
        appendBlock(file, TIME_INTERFACE_DESCRIPTOR_BLOCK, interfaceBody(LINKTYPE_USB_DARWIN));
        for (size_t i = 0; i < half; i++) { appendBlock(file, TYPE_ENHANCED_PACKET_BLOCK, enhancedPacketBody(0, packets[i])); }
        appendBlock(file, TYPE_SECTION_HEADER, sectionHeaderBody());
        appendBlock(file, TIME_INTERFACE_DESCRIPTOR_BLOCK, interfaceBody(LINKTYPE_USB_DARWIN));
        for (size_t i = half; i < packets.size(); i++) {
            appendBlock(file, TYPE_ENHANCED_PACKET_BLOCK, enhancedPacketBody(0, packets[i]));
        }

        auto parsed = parse(file);
        BOOST_TEST(parsed.input->getWarningCount() == 0u);
        auto usbmux = expertOf<USBMUXInterfaceExpert>(parsed.devices()[0]);
        BOOST_REQUIRE(usbmux != nullptr);
        BOOST_TEST(usbmux->getStreams().begin()->second->getMessages().size() == 2u);
    }

    BOOST_AUTO_TEST_CASE(test_simple_packet_blocks) {
        Bytes file;
        appendBlock(file, TYPE_SECTION_HEADER, sectionHeaderBody());
        appendBlock(file, TIME_INTERFACE_DESCRIPTOR_BLOCK, interfaceBody(LINKTYPE_USB_LINUX));
        for (const auto &packet: encode(Format::Usbmon, lockdownSession())) {
            Bytes body;
            appendStruct(body, static_cast<uint32_t>(packet.size()));
            append(body, packet);
            appendBlock(file, TYPE_SIMPLE_PACKET_BLOCK, body);
        }

        auto parsed = parse(file);
        BOOST_TEST(parsed.input->getWarningCount() == 0u);
        BOOST_TEST(parsed.devices()[0]->getProduct() == "iPhone");
    }

    BOOST_AUTO_TEST_CASE(test_packet_for_undeclared_interface_is_reported) {
        Bytes file;
        appendBlock(file, TYPE_SECTION_HEADER, sectionHeaderBody());
        appendBlock(file, TYPE_ENHANCED_PACKET_BLOCK, enhancedPacketBody(3, bytes({1, 2, 3})));
        auto parsed = parse(file);
        BOOST_TEST(parsed.input->getWarningCount() == 1u);
        BOOST_TEST(parsed.devices().empty());
    }

    BOOST_AUTO_TEST_CASE(test_big_endian_section_is_rejected) {
        Bytes file;
        Bytes body;
        appendStruct(body, static_cast<uint32_t>(PCAPNG_BYTE_ORDER_MAGIC_SWAPPED));
        appendStruct(body, static_cast<uint32_t>(0));
        appendStruct(body, static_cast<int64_t>(-1));
        appendBlock(file, TYPE_SECTION_HEADER, body);
        BOOST_CHECK_THROW(parse(file), demuxusb::ParseError);
    }

    BOOST_AUTO_TEST_CASE(test_non_capture_is_rejected) {
        BOOST_CHECK_THROW(parse(bytes("definitely not a capture file")), demuxusb::ParseError);
        BOOST_CHECK_THROW(parse(Bytes{}), demuxusb::ParseError);
    }

    BOOST_AUTO_TEST_CASE(test_truncated_capture_keeps_what_was_parsed) {
        auto file = capture(Format::Darwin, lockdownSession());
        file.resize(file.size() - 10);

        auto parsed = parse(file);
        BOOST_TEST(parsed.input->getWarningCount() == 1u);
        BOOST_REQUIRE_EQUAL(parsed.devices().size(), 1u);
        BOOST_TEST(parsed.devices()[0]->getProduct() == "iPhone");
        auto usbmux = expertOf<USBMUXInterfaceExpert>(parsed.devices()[0]);
        BOOST_REQUIRE(usbmux != nullptr);
        // The last (device -> host) message was in the truncated block
        BOOST_TEST(usbmux->getStreams().begin()->second->getMessages().size() == 1u);
    }

BOOST_AUTO_TEST_SUITE_END()
