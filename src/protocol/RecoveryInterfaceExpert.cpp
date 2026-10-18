//
// Created by Rick Mark on 11/16/20.
//

#include "RecoveryInterfaceExpert.h"

static std::string cString(byte_array data) {
    auto *start = reinterpret_cast<const char *>(data.first);
    auto length = strnlen(start, data.second);
    return {start, length};
}

static std::string printable(const std::string &value) {
    std::string result;
    for (auto c: value) {
        if (c == '\n') {
            result += "\\n";
        } else if (static_cast<unsigned char>(c) < 0x20 || c == 0x7F) {
            result += (boost::format("\\x%1$02x") % (uint32_t) static_cast<unsigned char>(c)).str();
        } else {
            result.push_back(c);
        }
    }
    return result;
}

void RecoveryInterfaceExpert::processControlOut(const usb_setup_t &setup, byte_array data) {
    InterfaceExpert::processControlOut(setup, data);

    if (setup.bmRequestType == 0x40 && data.second > 0) {
        this->m_payloads.push_back(RecoveryPayload{RecoveryPayloadKind::Command, setup.bRequest, cString(data)});
    } else if (setup.bmRequestType == 0x41 && setup.bRequest == 0) {
        // Start of a file transfer; the image follows on bulk OUT
        this->m_payloads.push_back(RecoveryPayload{RecoveryPayloadKind::Upload, setup.bRequest});
        this->m_uploadOpen = true;
    }
}

void RecoveryInterfaceExpert::processControlIn(const usb_setup_t &setup, byte_array data) {
    InterfaceExpert::processControlIn(setup, data);

    if (setup.bmRequestType == 0xC0 && data.second > 0) {
        this->m_payloads.push_back(RecoveryPayload{RecoveryPayloadKind::Response, setup.bRequest, cString(data)});
    }
}

RecoveryPayload &RecoveryInterfaceExpert::currentUpload() {
    if (!this->m_uploadOpen) {
        // Capture began mid transfer or the client skipped the announcement
        this->m_payloads.push_back(RecoveryPayload{RecoveryPayloadKind::Upload});
        this->m_uploadOpen = true;
    }

    for (auto it = this->m_payloads.rbegin(); it != this->m_payloads.rend(); ++it) {
        if (it->kind == RecoveryPayloadKind::Upload) { return *it; }
    }

    // Unreachable: an open upload always has a payload
    this->m_payloads.push_back(RecoveryPayload{RecoveryPayloadKind::Upload});
    return this->m_payloads.back();
}

void RecoveryInterfaceExpert::processBulkOut(byte_array data) {
    InterfaceExpert::processBulkOut(data);
    if (data.second == 0) { return; }

    auto &upload = this->currentUpload();
    upload.data.insert(upload.data.end(), data.first, data.first + data.second);
}

void RecoveryInterfaceExpert::processBulkIn(byte_array data) {
    InterfaceExpert::processBulkIn(data);
    this->m_console.append(reinterpret_cast<const char *>(data.first), data.second);
}

bool RecoveryInterfaceExpert::isEmpty() {
    return this->m_payloads.empty() && InterfaceExpert::isEmpty();
}

std::vector<std::string> RecoveryInterfaceExpert::getCommands() {
    std::vector<std::string> result;
    for (const auto &payload: this->m_payloads) {
        if (payload.kind == RecoveryPayloadKind::Command) { result.push_back(payload.text); }
    }
    return result;
}

std::vector<ExtractedArtifact> RecoveryInterfaceExpert::getArtifacts() {
    std::vector<ExtractedArtifact> result;
    size_t index = 0;
    for (const auto &payload: this->m_payloads) {
        if (payload.kind == RecoveryPayloadKind::Upload && !payload.data.empty()) {
            result.push_back({(boost::format("recovery-upload-%1%.bin") % index++).str(), payload.data});
        }
    }
    if (!this->m_console.empty()) {
        auto *begin = reinterpret_cast<const std::byte *>(this->m_console.data());
        result.push_back({"recovery-console.txt", std::vector<std::byte>(begin, begin + this->m_console.size())});
    }
    return result;
}

std::string RecoveryInterfaceExpert::toString() {
    size_t commands = 0, uploads = 0;
    uint64_t uploadBytes = 0;
    for (const auto &payload: this->m_payloads) {
        if (payload.kind == RecoveryPayloadKind::Command) { commands++; }
        if (payload.kind == RecoveryPayloadKind::Upload) {
            uploads++;
            uploadBytes += payload.data.size();
        }
    }

    auto format = boost::format(
            "RecoveryInterfaceExpert (config=%1%, int=%2%) - %3% commands, %4% uploads (%5% bytes), "
            "%6% console bytes; %7% control packets (%8% bytes) and %9% bulk packets (%10% bytes)");

    return (format % (uint32_t) this->m_configuration % (uint32_t) this->m_interface % commands % uploads %
            uploadBytes % this->m_console.size() % this->m_controlPackets % this->m_controlBytes % this->m_packets %
            this->m_bytes).str();
}

std::string RecoveryInterfaceExpert::details() {
    std::string result;
    for (const auto &payload: this->m_payloads) {
        switch (payload.kind) {
            case RecoveryPayloadKind::Command:
                result += "      > " + printable(payload.text) + "\n";
                break;
            case RecoveryPayloadKind::Response:
                result += "      < " + printable(payload.text) + "\n";
                break;
            case RecoveryPayloadKind::Upload:
                result += (boost::format("      ^ upload of %1% bytes\n") % payload.data.size()).str();
                break;
        }
    }
    return result;
}
