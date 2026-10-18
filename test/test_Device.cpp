#include <boost/test/unit_test.hpp>
#include "CaptureBuilder.h"
#include "protocol/USBMUXInterfaceExpert.h"
#include "protocol/DFUInterfaceExpert.h"
#include "protocol/RecoveryInterfaceExpert.h"

using namespace fixture;

static Bytes lockdownSyn(uint16_t host) {
    return muxPacket(MUX_PROTO_TCP, tcpSegment(host, 62078, USB_TCP_FLAG_SYN), true);
}

BOOST_AUTO_TEST_SUITE(device)

    BOOST_AUTO_TEST_CASE(test_alternate_settings_route_endpoints) {
        auto transcript = iphone(true);
        transcript.bulkOut(0x04, lockdownSyn(49152));

        auto parsed = parse(capture(Format::Darwin, transcript));
        auto *device = parsed.devices()[0];
        BOOST_TEST(device->getAlternateSetting(1) == 1);

        const auto &config = device->getConfigurations().at(1);
        BOOST_TEST(config.interfaces.size() == 3u);

        // Both alternate settings of interface 1 share one expert
        BOOST_TEST(device->getExperts().size() == 1u);
        auto usbmux = expertOf<USBMUXInterfaceExpert>(device);
        BOOST_REQUIRE(usbmux != nullptr);
        BOOST_TEST(usbmux->getInterface() == 1);
        BOOST_TEST(usbmux->getStreams().size() == 1u);
    }

    BOOST_AUTO_TEST_CASE(test_traffic_before_configuration_is_known) {
        // Capture started after enumeration of a single configuration device: no SET_CONFIGURATION seen
        Transcript transcript;
        enumerate(transcript, deviceDescriptor(APPLE_VID, 0x12A8), {iphoneUsbmuxConfiguration(1)}, {"Apple Inc."}, 0);
        transcript.bulkOut(0x04, lockdownSyn(49152));

        auto parsed = parse(capture(Format::USBPcap, transcript));
        auto usbmux = expertOf<USBMUXInterfaceExpert>(parsed.devices()[0]);
        BOOST_REQUIRE(usbmux != nullptr);
        BOOST_TEST(usbmux->getStreams().size() == 1u);
    }

    BOOST_AUTO_TEST_CASE(test_strings_decode_utf16) {
        Transcript transcript;
        auto product = bytes({12, USB_DT_STRING, 'i', 0, 0xe9, 0x00 /* é */, 0x3c, 0xd8, 0x4e, 0xdf /* 🍎 */, '!', 0});
        transcript.getDescriptor(USB_DT_DEVICE, 0, deviceDescriptor(APPLE_VID, 0x12A8), USB_DT_DEVICE_SIZE);
        transcript.getDescriptor(USB_DT_STRING, 2, product, 255);
        // A partial read of the string header alone must not clobber the full value
        transcript.getDescriptor(USB_DT_STRING, 2, product, 2);

        auto parsed = parse(capture(Format::Usbmon, transcript));
        BOOST_TEST(parsed.devices()[0]->getProduct() == "i\xc3\xa9\xf0\x9f\x8d\x8e!");
        BOOST_TEST(parsed.devices()[0]->getManufacturer() == "<unknown>");
    }

    BOOST_AUTO_TEST_CASE(test_reenumeration_starts_a_new_device_session) {
        Transcript transcript;
        enumerate(transcript, deviceDescriptor(APPLE_VID, IRECV_K_DFU_MODE),
                  {configDescriptor(1, {{0, 0, USB_CLASS_APP_SPEC, USB_SUBCLASS_DFU, 0, {}}})}, {"Apple Inc.", "Apple Mobile Device (DFU Mode)"});
        // Same address re-reads its own descriptor: still the same session
        transcript.getDescriptor(USB_DT_DEVICE, 0, deviceDescriptor(APPLE_VID, IRECV_K_DFU_MODE), USB_DT_DEVICE_SIZE);
        enumerate(transcript, deviceDescriptor(APPLE_VID, IRECV_K_RECOVERY_MODE_2),
                  {configDescriptor(1, {{0, 0, USB_CLASS_VENDOR_SPEC, 0x01, 0, {}}})},
                  {"Apple Inc.", "Apple Mobile Device (Recovery Mode)"});

        auto parsed = parse(capture(Format::Darwin, transcript));
        auto devices = parsed.devices();
        BOOST_REQUIRE_EQUAL(devices.size(), 2u);
        BOOST_TEST(devices[0]->getIdentifier() == devices[1]->getIdentifier());
        BOOST_TEST(devices[0]->getMode() == "DFU");
        BOOST_TEST(devices[0]->getProduct() == "Apple Mobile Device (DFU Mode)");
        BOOST_TEST(expertOf<DFUInterfaceExpert>(devices[0]) != nullptr);
        BOOST_TEST(devices[1]->getMode() == "Recovery");
        BOOST_TEST(devices[1]->getProduct() == "Apple Mobile Device (Recovery Mode)");
        BOOST_TEST(expertOf<RecoveryInterfaceExpert>(devices[1]) != nullptr);
    }

    BOOST_AUTO_TEST_CASE(test_bos_descriptor_is_kept) {
        auto bos = bytes({5, USB_DT_BOS, 12, 0, 1, 7, USB_DT_DEVICE_CAPABILITY, 2, 0x06, 0, 0, 0});
        Transcript transcript;
        transcript.getDescriptor(USB_DT_DEVICE, 0, deviceDescriptor(APPLE_VID, 0x12A8), USB_DT_DEVICE_SIZE);
        transcript.getDescriptor(USB_DT_BOS, 0, bos, 5);
        transcript.getDescriptor(USB_DT_BOS, 0, bos, 12);

        auto parsed = parse(capture(Format::Darwin, transcript));
        const auto &descriptors = parsed.devices()[0]->getBOSDescriptors();
        BOOST_REQUIRE_EQUAL(descriptors.size(), 1u);
        BOOST_TEST(descriptors[0].descriptor.bNumDeviceCaps == 1);
        BOOST_TEST(descriptors[0].data.size() == 7u);
    }

    BOOST_AUTO_TEST_CASE(test_malformed_configuration_is_flagged) {
        auto config = iphoneUsbmuxConfiguration(1);
        // Corrupt the second interface descriptor's length so it runs past the end
        config[USB_DT_CONFIG_SIZE + USB_DT_INTERFACE_SIZE + 3 * USB_DT_ENDPOINT_SIZE] = std::byte{200};

        Transcript transcript;
        enumerate(transcript, deviceDescriptor(APPLE_VID, 0x12A8), {config}, {});

        auto parsed = parse(capture(Format::Darwin, transcript));
        auto *device = parsed.devices()[0];
        BOOST_REQUIRE_EQUAL(device->getAnomalies().size(), 1u);
        BOOST_TEST(device->getConfigurations().at(1).interfaces.size() == 1u);
    }

    BOOST_AUTO_TEST_CASE(test_mismatched_descriptor_type_is_flagged) {
        Transcript transcript;
        transcript.control(setup(0x80, USB_REQ_GET_DESCRIPTOR, USB_DT_DEVICE << 8U, 0, 18),
                           stringDescriptor("not a device descriptor"));

        auto parsed = parse(capture(Format::Darwin, transcript));
        BOOST_TEST(parsed.devices()[0]->getAnomalies().size() == 1u);
        BOOST_TEST(!parsed.devices()[0]->hasDeviceDescriptor());
    }

BOOST_AUTO_TEST_SUITE_END()
