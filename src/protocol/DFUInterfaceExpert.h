//
// Created by Rick Mark on 11/16/20.
//

#ifndef demuxusb_DFUINTERFACEEXPERT_H
#define demuxusb_DFUINTERFACEEXPERT_H

#include "../demuxusb.hpp"
#include "InterfaceExpert.h"

enum dfu_request {
    DFU_DETACH = 0,
    DFU_DNLOAD = 1,
    DFU_UPLOAD = 2,
    DFU_GETSTATUS = 3,
    DFU_CLRSTATUS = 4,
    DFU_GETSTATE = 5,
    DFU_ABORT = 6
};

// Follows the USB DFU 1.1 class protocol: reassembles DNLOAD transfers into images, tracks the device state machine
//  from GETSTATUS / GETSTATE responses and records protocol anomalies worth a forensic look.
class DFUInterfaceExpert : public InterfaceExpert {
public:
    DFUInterfaceExpert(uint8_t configuration, uint8_t interface) : InterfaceExpert(configuration, interface) {};

    void processControlOut(const usb_setup_t &setup, byte_array data) override;

    void processControlIn(const usb_setup_t &setup, byte_array data) override;

    std::string toString() override;

    std::string details() override;

    std::vector<ExtractedArtifact> getArtifacts() override;

    void setTransferSize(uint16_t transferSize) { this->m_transferSize = transferSize; }

    // Completed DNLOAD images (terminated by a zero length DNLOAD), followed by any partial image still in flight.
    std::vector<std::vector<std::byte>> getImages();

    [[nodiscard]] const std::vector<std::string> &getAnomalies() const { return this->m_anomalies; }

    [[nodiscard]] int getLastState() const { return this->m_lastState; }

    [[nodiscard]] int getLastStatus() const { return this->m_lastStatus; }

    static std::string stateName(int state);

protected:
    void anomaly(const std::string &description);

    uint16_t m_transferSize{};
    std::vector<std::vector<std::byte>> m_images{};
    std::vector<std::byte> m_currentImage{};
    uint64_t m_uploadBytes{};
    std::map<uint8_t, uint32_t> m_requestCounts{};
    std::vector<std::string> m_anomalies{};
    int m_lastState{-1};
    int m_lastStatus{-1};
};


#endif //demuxusb_DFUINTERFACEEXPERT_H
