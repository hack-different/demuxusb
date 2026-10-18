//
// Created by Rick Mark on 11/16/20.
//

#include "DFUInterfaceExpert.h"

static const char *requestName(uint8_t request) {
    switch (request) {
        case DFU_DETACH: return "DETACH";
        case DFU_DNLOAD: return "DNLOAD";
        case DFU_UPLOAD: return "UPLOAD";
        case DFU_GETSTATUS: return "GETSTATUS";
        case DFU_CLRSTATUS: return "CLRSTATUS";
        case DFU_GETSTATE: return "GETSTATE";
        case DFU_ABORT: return "ABORT";
        default: return "UNKNOWN";
    }
}

std::string DFUInterfaceExpert::stateName(int state) {
    switch (state) {
        case 0: return "appIDLE";
        case 1: return "appDETACH";
        case 2: return "dfuIDLE";
        case 3: return "dfuDNLOAD-SYNC";
        case 4: return "dfuDNBUSY";
        case 5: return "dfuDNLOAD-IDLE";
        case 6: return "dfuMANIFEST-SYNC";
        case 7: return "dfuMANIFEST";
        case 8: return "dfuMANIFEST-WAIT-RESET";
        case 9: return "dfuUPLOAD-IDLE";
        case 10: return "dfuERROR";
        case -1: return "unknown";
        default: return (boost::format("state(%1%)") % state).str();
    }
}

void DFUInterfaceExpert::anomaly(const std::string &description) {
    this->m_anomalies.push_back(description);
}

void DFUInterfaceExpert::processControlOut(const usb_setup_t &setup, byte_array data) {
    InterfaceExpert::processControlOut(setup, data);

    if ((setup.bmRequestType & USB_TYPE_MASK) != USB_TYPE_CLASS) { return; }
    this->m_requestCounts[setup.bRequest]++;

    if (data.second != setup.wLength) {
        this->anomaly((boost::format("%1% carried %2% bytes but wLength was %3%") % requestName(setup.bRequest) %
                       data.second % setup.wLength).str());
    }

    switch (setup.bRequest) {
        case DFU_DNLOAD:
            if (setup.wLength == 0) {
                // A zero length download moves the device to manifestation, completing the image
                if (!this->m_currentImage.empty()) {
                    this->m_images.push_back(std::move(this->m_currentImage));
                    this->m_currentImage = {};
                }
                break;
            }

            if (this->m_transferSize != 0 && setup.wLength > this->m_transferSize) {
                this->anomaly((boost::format("DNLOAD of %1% bytes exceeds wTransferSize %2%") % setup.wLength %
                               this->m_transferSize).str());
            }

            this->m_currentImage.insert(this->m_currentImage.end(), data.first, data.first + data.second);
            break;

        case DFU_ABORT:
            if (!this->m_currentImage.empty()) {
                this->anomaly((boost::format("ABORT after a partial download of %1% bytes") %
                               this->m_currentImage.size()).str());
                this->m_images.push_back(std::move(this->m_currentImage));
                this->m_currentImage = {};
            }
            break;

        case DFU_DETACH:
        case DFU_CLRSTATUS:
            break;

        default:
            this->anomaly((boost::format("Unexpected class OUT request 0x%1$02x") % (uint32_t) setup.bRequest).str());
    }
}

void DFUInterfaceExpert::processControlIn(const usb_setup_t &setup, byte_array data) {
    InterfaceExpert::processControlIn(setup, data);

    if ((setup.bmRequestType & USB_TYPE_MASK) != USB_TYPE_CLASS) { return; }
    this->m_requestCounts[setup.bRequest]++;

    switch (setup.bRequest) {
        case DFU_GETSTATUS:
            // bStatus, bwPollTimeout[3], bState, iString
            if (data.second >= 5) {
                this->m_lastStatus = static_cast<uint8_t>(data.first[0]);
                this->m_lastState = static_cast<uint8_t>(data.first[4]);
            }
            break;
        case DFU_GETSTATE:
            if (data.second >= 1) {
                this->m_lastState = static_cast<uint8_t>(data.first[0]);
            }
            break;
        case DFU_UPLOAD:
            this->m_uploadBytes += data.second;
            break;
        default:
            this->anomaly((boost::format("Unexpected class IN request 0x%1$02x") % (uint32_t) setup.bRequest).str());
    }
}

std::vector<std::vector<std::byte>> DFUInterfaceExpert::getImages() {
    auto result = this->m_images;
    if (!this->m_currentImage.empty()) {
        result.push_back(this->m_currentImage);
    }
    return result;
}

std::vector<ExtractedArtifact> DFUInterfaceExpert::getArtifacts() {
    std::vector<ExtractedArtifact> result;
    auto images = this->getImages();
    for (size_t i = 0; i < images.size(); i++) {
        auto partial = (i == this->m_images.size()) ? "-partial" : "";
        result.push_back({(boost::format("dfu-image-%1%%2%.bin") % i % partial).str(), std::move(images[i])});
    }
    return result;
}

std::string DFUInterfaceExpert::toString() {
    uint64_t imageBytes = 0;
    auto images = this->getImages();
    for (const auto &image: images) { imageBytes += image.size(); }

    auto format = boost::format(
            "DFUInterfaceExpert (config=%1%, int=%2%) - %3% control packets (%4% bytes), %5% images downloaded "
            "(%6% bytes), %7% bytes uploaded, last state %8%, %9% anomalies");

    return (format % (uint32_t) this->m_configuration % (uint32_t) this->m_interface % this->m_controlPackets %
            this->m_controlBytes % images.size() % imageBytes % this->m_uploadBytes % stateName(this->m_lastState) %
            this->m_anomalies.size()).str();
}

std::string DFUInterfaceExpert::details() {
    std::string result;
    for (const auto &[request, count]: this->m_requestCounts) {
        result += (boost::format("      %1%: %2%\n") % requestName(request) % count).str();
    }
    for (const auto &anomaly: this->m_anomalies) {
        result += "      ! " + anomaly + "\n";
    }
    return result;
}
