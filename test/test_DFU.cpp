#include <boost/test/unit_test.hpp>
#include "CaptureBuilder.h"
#include "protocol/DFUInterfaceExpert.h"

using namespace fixture;

static const uint16_t TRANSFER_SIZE = 0x800;

static Transcript dfuDevice() {
    Transcript transcript;
    enumerate(transcript, deviceDescriptor(APPLE_VID, IRECV_K_DFU_MODE),
              {configDescriptor(1, {{0, 0, USB_CLASS_APP_SPEC, USB_SUBCLASS_DFU, 0, {},
                                     dfuFunctionalDescriptor(TRANSFER_SIZE)}})},
              {"Apple Inc.", "Apple Mobile Device (DFU Mode)",
               "CPID:8101 CPRV:10 CPFM:03 SCEP:01 BDID:0C ECID:001234567890001E IBFL:3C SRTG:[iBoot-4479.0.0.100.4]"});
    return transcript;
}

static Bytes getStatusResponse(uint8_t status, uint8_t state) {
    return bytes({status, 0, 0, 0, state, 0});
}

static Bytes pattern(size_t size, uint8_t seed) {
    Bytes result(size);
    for (size_t i = 0; i < size; i++) { result[i] = static_cast<std::byte>((i * 7 + seed) & 0xFF); }
    return result;
}

static void download(Transcript &transcript, const Bytes &image) {
    for (size_t offset = 0, block = 0; offset < image.size(); offset += TRANSFER_SIZE, block++) {
        auto end = std::min(image.size(), offset + TRANSFER_SIZE);
        Bytes chunk(image.begin() + offset, image.begin() + end);
        transcript.control(setup(0x21, DFU_DNLOAD, block, 0, chunk.size()), chunk);
        transcript.control(setup(0xA1, DFU_GETSTATUS, 0, 0, 6), getStatusResponse(0, 5));
    }
    transcript.control(setup(0x21, DFU_DNLOAD, 0, 0, 0));
    transcript.control(setup(0xA1, DFU_GETSTATUS, 0, 0, 6), getStatusResponse(0, 6));
    transcript.control(setup(0xA1, DFU_GETSTATUS, 0, 0, 6), getStatusResponse(0, 7));
    transcript.control(setup(0xA1, DFU_GETSTATUS, 0, 0, 6), getStatusResponse(0, 8));
}

BOOST_AUTO_TEST_SUITE(dfu)

    BOOST_AUTO_TEST_CASE(test_images_are_reassembled) {
        auto image = pattern(TRANSFER_SIZE * 3 + 123, 1);
        auto transcript = dfuDevice();
        download(transcript, image);

        auto parsed = parse(capture(Format::Usbmon, transcript));
        auto *device = parsed.devices()[0];
        BOOST_TEST(device->getMode() == "DFU");
        BOOST_TEST(device->getSerial().find("ECID:001234567890001E") != std::string::npos);

        auto dfu = expertOf<DFUInterfaceExpert>(device);
        BOOST_REQUIRE(dfu != nullptr);
        auto images = dfu->getImages();
        BOOST_REQUIRE_EQUAL(images.size(), 1u);
        BOOST_TEST((images[0] == image));
        BOOST_TEST(dfu->getLastState() == 8);
        BOOST_TEST(DFUInterfaceExpert::stateName(dfu->getLastState()) == "dfuMANIFEST-WAIT-RESET");
        BOOST_TEST(dfu->getAnomalies().empty());

        auto artifacts = dfu->getArtifacts();
        BOOST_REQUIRE_EQUAL(artifacts.size(), 1u);
        BOOST_TEST(artifacts[0].name == "dfu-image-0.bin");
    }

    BOOST_AUTO_TEST_CASE(test_partial_download_is_kept) {
        auto transcript = dfuDevice();
        auto chunk = pattern(TRANSFER_SIZE, 2);
        transcript.control(setup(0x21, DFU_DNLOAD, 0, 0, chunk.size()), chunk);

        auto parsed = parse(capture(Format::Darwin, transcript));
        auto dfu = expertOf<DFUInterfaceExpert>(parsed.devices()[0]);
        BOOST_REQUIRE(dfu != nullptr);
        BOOST_TEST(dfu->getImages().size() == 1u);
        BOOST_TEST(dfu->getArtifacts()[0].name == "dfu-image-0-partial.bin");
    }

    BOOST_AUTO_TEST_CASE(test_protocol_anomalies_are_recorded) {
        auto transcript = dfuDevice();
        auto oversized = pattern(TRANSFER_SIZE * 2, 3);
        transcript.control(setup(0x21, DFU_DNLOAD, 0, 0, oversized.size()), oversized);
        transcript.control(setup(0x21, DFU_ABORT, 0, 0, 0));
        transcript.control(setup(0x21, 0x42, 0, 0, 0));
        transcript.control(setup(0xA1, DFU_GETSTATUS, 0, 0, 6), getStatusResponse(0x0E, 10));

        auto parsed = parse(capture(Format::USBPcap, transcript));
        auto dfu = expertOf<DFUInterfaceExpert>(parsed.devices()[0]);
        BOOST_REQUIRE(dfu != nullptr);
        const auto &anomalies = dfu->getAnomalies();
        BOOST_REQUIRE_EQUAL(anomalies.size(), 3u);
        BOOST_TEST(anomalies[0] == "DNLOAD of 4096 bytes exceeds wTransferSize 2048");
        BOOST_TEST(anomalies[1] == "ABORT after a partial download of 4096 bytes");
        BOOST_TEST(anomalies[2] == "Unexpected class OUT request 0x42");
        BOOST_TEST(dfu->getLastStatus() == 0x0E);
        BOOST_TEST(DFUInterfaceExpert::stateName(dfu->getLastState()) == "dfuERROR");
    }

BOOST_AUTO_TEST_SUITE_END()
