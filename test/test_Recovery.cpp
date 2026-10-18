#include <boost/test/unit_test.hpp>
#include "CaptureBuilder.h"
#include "protocol/RecoveryInterfaceExpert.h"

using namespace fixture;

// iBoot exposes interface 0 for control and, in alternate setting 1 of interface 1, the bulk pipes libirecovery uses
static Transcript recoveryDevice() {
    Transcript transcript;
    enumerate(transcript, deviceDescriptor(APPLE_VID, IRECV_K_RECOVERY_MODE_2),
              {configDescriptor(1, {{0, 0, USB_CLASS_VENDOR_SPEC, 0x01, 0, {}},
                                    {1, 0, USB_CLASS_VENDOR_SPEC, 0x01, 0, {}},
                                    {1, 1, USB_CLASS_VENDOR_SPEC, 0x01, 0, {{0x04, 2}, {0x81, 2}}}})},
              {"Apple Inc.", "Apple Mobile Device (Recovery Mode)"});
    transcript.setInterface(0, 0);
    transcript.setInterface(1, 1);
    return transcript;
}

static Bytes command(const std::string &value) {
    auto result = bytes(value);
    result.push_back(std::byte{0});
    return result;
}

BOOST_AUTO_TEST_SUITE(recovery)

    BOOST_AUTO_TEST_CASE(test_commands_uploads_and_console) {
        Bytes ibec(70000);
        for (size_t i = 0; i < ibec.size(); i++) { ibec[i] = static_cast<std::byte>(i % 251); }

        auto transcript = recoveryDevice();
        transcript.control(setup(0x40, 0, 0, 0, 22), command("setenv auto-boot true"));
        transcript.control(setup(0x40, 0, 0, 0, 9), command("saveenv"));
        transcript.control(setup(0x40, 0, 0, 0, 19), command("getenv build-style"));
        transcript.control(setup(0xC0, 0, 0, 0, 255), command("RELEASE"));
        transcript.control(setup(0x41, 0, 0, 0, 0));
        for (size_t offset = 0; offset < ibec.size(); offset += 0x8000) {
            auto end = std::min(ibec.size(), offset + 0x8000);
            transcript.bulkOut(0x04, Bytes(ibec.begin() + offset, ibec.begin() + end));
        }
        transcript.control(setup(0x40, 0, 0, 0, 3), command("go"));
        transcript.bulkIn(0x81, bytes("] go\n"));
        transcript.bulkIn(0x81, bytes("jumping into image\n"));

        auto parsed = parse(capture(Format::Darwin, transcript));
        auto *device = parsed.devices()[0];
        BOOST_TEST(device->getMode() == "Recovery");

        // One expert spans both interfaces so commands and uploads stay in order
        BOOST_REQUIRE_EQUAL(device->getExperts().size(), 1u);
        auto recovery = expertOf<RecoveryInterfaceExpert>(device);
        BOOST_REQUIRE(recovery != nullptr);

        auto commands = recovery->getCommands();
        BOOST_REQUIRE_EQUAL(commands.size(), 4u);
        BOOST_TEST(commands[0] == "setenv auto-boot true");
        BOOST_TEST(commands[3] == "go");

        const auto &payloads = recovery->getPayloads();
        BOOST_TEST((payloads[3].kind == RecoveryPayloadKind::Response && payloads[3].text == "RELEASE"));
        BOOST_TEST((payloads[4].kind == RecoveryPayloadKind::Upload && payloads[4].data == ibec));
        BOOST_TEST(recovery->getConsole() == "] go\njumping into image\n");

        auto artifacts = recovery->getArtifacts();
        BOOST_REQUIRE_EQUAL(artifacts.size(), 2u);
        BOOST_TEST(artifacts[0].name == "recovery-upload-0.bin");
        BOOST_TEST(artifacts[1].name == "recovery-console.txt");

        BOOST_TEST(recovery->details().find("> setenv auto-boot true") != std::string::npos);
        BOOST_TEST(recovery->toString().find("4 commands, 1 uploads (70000 bytes)") != std::string::npos);
    }

    BOOST_AUTO_TEST_CASE(test_upload_without_announcement) {
        auto transcript = recoveryDevice();
        transcript.bulkOut(0x04, bytes("IMG4"));

        auto parsed = parse(capture(Format::Usbmon, transcript));
        auto recovery = expertOf<RecoveryInterfaceExpert>(parsed.devices()[0]);
        BOOST_REQUIRE(recovery != nullptr);
        BOOST_REQUIRE_EQUAL(recovery->getPayloads().size(), 1u);
        BOOST_TEST((recovery->getPayloads()[0].data == bytes("IMG4")));
    }

BOOST_AUTO_TEST_SUITE_END()
