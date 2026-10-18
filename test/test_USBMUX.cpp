#include <boost/test/unit_test.hpp>
#include "CaptureBuilder.h"
#include "protocol/USBMUXInterfaceExpert.h"

using namespace fixture;

static const uint16_t HOST_PORT = 49152;

static Bytes out(uint16_t host, uint16_t device, uint16_t flags, const Bytes &data = {}) {
    return muxPacket(MUX_PROTO_TCP, tcpSegment(host, device, flags, data), true);
}

static Bytes in(uint16_t host, uint16_t device, uint16_t flags, const Bytes &data = {}) {
    return muxPacket(MUX_PROTO_TCP, tcpSegment(device, host, flags, data), false);
}

static std::shared_ptr<USBMUXInterfaceExpert> usbmuxOf(Parsed &parsed) {
    BOOST_REQUIRE_EQUAL(parsed.devices().size(), 1u);
    auto usbmux = expertOf<USBMUXInterfaceExpert>(parsed.devices()[0]);
    BOOST_REQUIRE(usbmux != nullptr);
    return usbmux;
}

static std::shared_ptr<MuxStream> onlyStream(const std::shared_ptr<USBMUXInterfaceExpert> &usbmux) {
    BOOST_REQUIRE_EQUAL(usbmux->getStreams().size(), 1u);
    return usbmux->getStreams().begin()->second;
}

static Bytes request(const std::string &name) {
    return lengthPrefixed(xmlPlist("<key>Request</key><string>" + name + "</string>"));
}

BOOST_AUTO_TEST_SUITE(usbmux)

    BOOST_AUTO_TEST_CASE(test_mux_packet_split_across_transfers) {
        auto packet = out(HOST_PORT, 62078, USB_TCP_FLAG_ACK, request("GetValue"));
        auto transcript = iphone();
        transcript.bulkOut(0x04, out(HOST_PORT, 62078, USB_TCP_FLAG_SYN));
        transcript.bulkOut(0x04, Bytes(packet.begin(), packet.begin() + 5));
        transcript.bulkOut(0x04, Bytes(packet.begin() + 5, packet.begin() + 40));
        transcript.bulkOut(0x04, Bytes(packet.begin() + 40, packet.end()));

        auto parsed = parse(capture(Format::Darwin, transcript));
        auto stream = onlyStream(usbmuxOf(parsed));
        BOOST_REQUIRE_EQUAL(stream->getMessages().size(), 1u);
        BOOST_TEST(stream->getMessages()[0].stringValue("Request") == "GetValue");
        BOOST_TEST(stream->getBytesOut() == request("GetValue").size());
    }

    BOOST_AUTO_TEST_CASE(test_several_mux_packets_in_one_transfer_and_split_plist) {
        auto message = request("StartSession");
        Bytes coalesced;
        append(coalesced, in(HOST_PORT, 62078, USB_TCP_FLAG_SYN | USB_TCP_FLAG_ACK));
        // Length prefix in its own segment, then the plist in two more
        append(coalesced, in(HOST_PORT, 62078, USB_TCP_FLAG_ACK, Bytes(message.begin(), message.begin() + 4)));
        append(coalesced, in(HOST_PORT, 62078, USB_TCP_FLAG_ACK, Bytes(message.begin() + 4, message.begin() + 30)));
        append(coalesced, in(HOST_PORT, 62078, USB_TCP_FLAG_ACK, Bytes(message.begin() + 30, message.end())));

        auto transcript = iphone();
        transcript.bulkOut(0x04, out(HOST_PORT, 62078, USB_TCP_FLAG_SYN));
        transcript.bulkIn(0x85, coalesced);

        auto parsed = parse(capture(Format::USBPcap, transcript));
        auto stream = onlyStream(usbmuxOf(parsed));
        BOOST_REQUIRE_EQUAL(stream->getMessages().size(), 1u);
        BOOST_TEST(!stream->getMessages()[0].outbound);
        BOOST_TEST(stream->getMessages()[0].summary() == "Request=StartSession");
        BOOST_TEST(stream->getEncodingIn() == StreamEncoding::Plist);
    }

    BOOST_AUTO_TEST_CASE(test_binary_plist) {
        plist_t dict = plist_new_dict();
        plist_dict_set_item(dict, "Request", plist_new_string("GetValue"));
        plist_dict_set_item(dict, "Key", plist_new_string("ProductVersion"));
        char *binary = nullptr;
        uint32_t length = 0;
        plist_to_bin(dict, &binary, &length);
        Bytes message(reinterpret_cast<std::byte *>(binary), reinterpret_cast<std::byte *>(binary) + length);
        free(binary);
        plist_free(dict);

        auto transcript = iphone();
        transcript.bulkOut(0x04, out(HOST_PORT, 62078, USB_TCP_FLAG_SYN));
        transcript.bulkOut(0x04, out(HOST_PORT, 62078, USB_TCP_FLAG_ACK, lengthPrefixed(message)));

        auto parsed = parse(capture(Format::Usbmon, transcript));
        auto stream = onlyStream(usbmuxOf(parsed));
        BOOST_REQUIRE_EQUAL(stream->getMessages().size(), 1u);
        BOOST_TEST(stream->getMessages()[0].summary() == "Request=GetValue, Key=ProductVersion");
        BOOST_TEST(stream->getMessages()[0].xml().find("<string>ProductVersion</string>") != std::string::npos);
    }

    BOOST_AUTO_TEST_CASE(test_start_service_names_later_streams) {
        auto transcript = iphone();
        transcript.bulkOut(0x04, out(HOST_PORT, 62078, USB_TCP_FLAG_SYN));
        transcript.bulkOut(0x04, out(HOST_PORT, 62078, USB_TCP_FLAG_ACK, lengthPrefixed(xmlPlist(
                "<key>Request</key><string>StartService</string>"
                "<key>Service</key><string>com.apple.afc</string>"))));
        transcript.bulkIn(0x85, in(HOST_PORT, 62078, USB_TCP_FLAG_ACK, lengthPrefixed(xmlPlist(
                "<key>Port</key><integer>50123</integer><key>Request</key><string>StartService</string>"
                "<key>Service</key><string>com.apple.afc</string>"))));
        transcript.bulkOut(0x04, out(HOST_PORT, 62078, USB_TCP_FLAG_FIN | USB_TCP_FLAG_ACK));
        transcript.bulkOut(0x04, out(HOST_PORT + 1, 50123, USB_TCP_FLAG_SYN));
        transcript.bulkOut(0x04, out(HOST_PORT + 1, 50123, USB_TCP_FLAG_ACK, bytes("CFA6LPAA(")));

        auto parsed = parse(capture(Format::Darwin, transcript));
        auto usbmux = usbmuxOf(parsed);
        BOOST_REQUIRE_EQUAL(usbmux->getStreams().size(), 2u);

        auto lockdown = usbmux->getStreams().begin()->second;
        auto afc = usbmux->getStreams().rbegin()->second;
        if (lockdown->getDevicePort() != 62078) { std::swap(lockdown, afc); }

        BOOST_TEST(lockdown->isClosed());
        BOOST_TEST(lockdown->getMessages()[1].summary() == "Request=StartService, Service=com.apple.afc, Port=50123");
        BOOST_TEST(afc->getServiceName() == "com.apple.afc");
        BOOST_TEST(afc->getEncodingOut() == StreamEncoding::Raw);
        BOOST_TEST(afc->getTransactionCount() == 0u);
        BOOST_TEST(usbmux->toString().find("[com.apple.afc]") != std::string::npos);
        BOOST_TEST(usbmux->details().find("> Request=StartService, Service=com.apple.afc") != std::string::npos);
    }

    BOOST_AUTO_TEST_CASE(test_tls_upgrade_is_detected) {
        auto transcript = iphone();
        transcript.bulkOut(0x04, out(HOST_PORT, 62078, USB_TCP_FLAG_SYN));
        transcript.bulkOut(0x04, out(HOST_PORT, 62078, USB_TCP_FLAG_ACK, request("StartSession")));
        transcript.bulkOut(0x04, out(HOST_PORT, 62078, USB_TCP_FLAG_ACK, bytes({0x16, 0x03, 0x01, 0x00, 0x05, 1, 2, 3, 4, 5})));
        transcript.bulkOut(0x04, out(HOST_PORT, 62078, USB_TCP_FLAG_ACK, bytes({0x17, 0x03, 0x03, 0x00, 0x02, 9, 9})));

        auto parsed = parse(capture(Format::Darwin, transcript));
        auto stream = onlyStream(usbmuxOf(parsed));
        BOOST_TEST(stream->getMessages().size() == 1u);
        BOOST_TEST(stream->getEncodingOut() == StreamEncoding::TLS);
        BOOST_TEST(stream->getBytesOut() == request("StartSession").size() + 17);
    }

    BOOST_AUTO_TEST_CASE(test_connection_opened_before_capture) {
        auto transcript = iphone();
        transcript.bulkIn(0x85, in(HOST_PORT, 62078, USB_TCP_FLAG_ACK, request("Goodbye")));

        auto parsed = parse(capture(Format::Darwin, transcript));
        auto stream = onlyStream(usbmuxOf(parsed));
        BOOST_TEST(stream->getHostPort() == HOST_PORT);
        BOOST_TEST(stream->getMessages().size() == 1u);
    }

    BOOST_AUTO_TEST_CASE(test_reconnects_create_new_streams) {
        auto transcript = iphone();
        for (int i = 0; i < 3; i++) {
            transcript.bulkOut(0x04, out(HOST_PORT, 62078, USB_TCP_FLAG_SYN));
            transcript.bulkOut(0x04, out(HOST_PORT, 62078, USB_TCP_FLAG_ACK, request("QueryType")));
            transcript.bulkOut(0x04, out(HOST_PORT, 62078, USB_TCP_FLAG_RST));
        }

        auto parsed = parse(capture(Format::Darwin, transcript));
        auto usbmux = usbmuxOf(parsed);
        BOOST_REQUIRE_EQUAL(usbmux->getStreams().size(), 3u);
        for (const auto &[id, stream]: usbmux->getStreams()) {
            BOOST_TEST(stream->getMessages().size() == 1u);
            BOOST_TEST(stream->isClosed());
        }
    }

    BOOST_AUTO_TEST_CASE(test_lost_framing_resynchronises) {
        auto transcript = iphone();
        transcript.bulkOut(0x04, bytes({0xde, 0xad, 0xbe, 0xef, 0xff, 0xff, 0xff, 0xff, 1, 2, 3}));
        transcript.bulkOut(0x04, out(HOST_PORT, 62078, USB_TCP_FLAG_SYN));

        auto parsed = parse(capture(Format::Darwin, transcript));
        auto usbmux = usbmuxOf(parsed);
        BOOST_TEST(usbmux->getResyncCount() == 1u);
        BOOST_TEST(usbmux->getStreams().size() == 1u);
    }

    BOOST_AUTO_TEST_CASE(test_artifacts_hold_plist_transcripts) {
        auto transcript = iphone();
        transcript.bulkOut(0x04, out(HOST_PORT, 62078, USB_TCP_FLAG_SYN));
        transcript.bulkOut(0x04, out(HOST_PORT, 62078, USB_TCP_FLAG_ACK, request("QueryType")));

        auto parsed = parse(capture(Format::Darwin, transcript));
        auto artifacts = usbmuxOf(parsed)->getArtifacts();
        BOOST_REQUIRE_EQUAL(artifacts.size(), 1u);
        std::string document(reinterpret_cast<const char *>(artifacts[0].data.data()), artifacts[0].data.size());
        BOOST_TEST(document.find("<!-- host -> device -->") != std::string::npos);
        BOOST_TEST(document.find("<string>QueryType</string>") != std::string::npos);
    }

BOOST_AUTO_TEST_SUITE_END()
