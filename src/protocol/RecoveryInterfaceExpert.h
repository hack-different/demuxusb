//
// Created by Rick Mark on 11/16/20.
//

#ifndef demuxusb_RECOVERYINTERFACEEXPERT_H
#define demuxusb_RECOVERYINTERFACEEXPERT_H

#include "../demuxusb.hpp"
#include "InterfaceExpert.h"

enum class RecoveryPayloadKind {
    Command,  // Host -> iBoot command string (vendor OUT request, e.g. "setenv auto-boot true")
    Response, // iBoot -> host control response (e.g. the value returned for "getenv")
    Upload    // Host -> iBoot file upload over the bulk OUT endpoint (iBSS, iBEC, ramdisk, ...)
};

struct RecoveryPayload {
    RecoveryPayloadKind kind;
    uint8_t request{};
    std::string text{};
    std::vector<std::byte> data{};
};

// Decodes the iBoot recovery mode protocol as spoken by libirecovery: commands are NUL terminated strings sent as
//  vendor control transfers, files are announced by a 0x41 control transfer and then streamed over bulk OUT, and the
//  iBoot console is read back over bulk IN.
class RecoveryInterfaceExpert : public InterfaceExpert {
public:
    RecoveryInterfaceExpert(uint8_t configuration, uint8_t interface) : InterfaceExpert(configuration, interface) {};

    void processControlOut(const usb_setup_t &setup, byte_array data) override;

    void processControlIn(const usb_setup_t &setup, byte_array data) override;

    void processBulkOut(byte_array data) override;

    void processBulkIn(byte_array data) override;

    std::string toString() override;

    std::string details() override;

    std::vector<ExtractedArtifact> getArtifacts() override;

    bool isEmpty() override;

    [[nodiscard]] const std::vector<RecoveryPayload> &getPayloads() const { return this->m_payloads; }

    std::vector<std::string> getCommands();

    [[nodiscard]] const std::string &getConsole() const { return this->m_console; }

protected:
    RecoveryPayload &currentUpload();

    std::vector<RecoveryPayload> m_payloads{};
    bool m_uploadOpen{false};
    std::string m_console{};
};


#endif //demuxusb_RECOVERYINTERFACEEXPERT_H
