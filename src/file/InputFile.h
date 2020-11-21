//
// Created by Rick Mark on 11/14/20.
//

#ifndef demuxusb_CINPUTFILE_H
#define demuxusb_CINPUTFILE_H

#include "../demuxusb.hpp"
#include "../usb/Device.h"


namespace demuxusb {

    class DeviceHandle {
    public:
        explicit DeviceHandle(uint64_t id) {
            this->m_device = std::make_unique<Device>(id);
        }

        void beginControl(uint8_t endpoint, uint64_t id, usb_setup_t *setup) {
            if (this->m_inflight[endpoint].contains(id)) {
                std::cerr << "Already had inflight operation on " << std::dec << (uint32_t) endpoint << " with id "
                          << std::hex << id << std::endl;
                return;
            }
            this->m_inflight[endpoint].insert(id);
            this->m_controlSetup[endpoint][id] = *setup;
        }

        usb_setup_t endControl(uint8_t endpoint, uint64_t id) {
            if (!this->m_inflight[endpoint].contains(id)) {
                std::cerr << "Control completion on endpoint " << std::dec << (uint32_t) endpoint
                          << " could not find submit for " << std::hex << id << std::endl;
                return usb_setup_t{};
            }

            if (!this->m_controlSetup[endpoint].contains(id)) {
                std::cerr << "Control completion on endpoint " << std::dec << (uint32_t) endpoint << " with id " << id
                          << " missing setup info" << std::endl;
                return usb_setup_t{};
            }

            auto result = this->m_controlSetup[endpoint][id];
            this->m_controlSetup[endpoint].erase(id);
            this->m_inflight[endpoint].erase(id);
            return result;
        }

        void beginBulk(uint8_t endpoint, uint64_t id) {

            if (this->m_inflight[endpoint].contains(id)) {
                std::cerr << "Bulk already had inflight operation on " << std::dec << (uint32_t) endpoint << " with id "
                          << std::hex << id << std::endl;
                return;
            }
            this->m_inflight[endpoint].insert(id);
        }

        void endBulk(uint8_t endpoint, uint64_t id) {
            if (!this->m_inflight[endpoint].contains(id)) {
                std::cerr << "Bulk completion on endpoint " << std::dec << (uint32_t) endpoint << " with id "
                          << std::hex << id << " does not have matching submit." << std::endl;
                return;
            }
            this->m_inflight[endpoint].erase(id);
        }

        Device *device() { return this->m_device.get(); }

    private:
        std::unique_ptr<Device> m_device;
        std::array<std::set<uint64_t>, UINT8_MAX> m_inflight{};
        std::array<std::map<uint64_t, usb_setup_t>, UINT8_MAX> m_controlSetup{};
    };

    typedef struct {
        uint8_t endpoint;
        uint32_t length;

    } usb_frame_info_t;

    class InputFile {
    public:
        explicit InputFile(fs::path path);

        virtual ~InputFile() = default;

        virtual void parse() = 0;

        std::vector<Device *> getDevices();

    protected:
        fs::path m_path;
        io::mapped_file_source m_mappedInputFile;
        std::map<uint64_t, std::shared_ptr<DeviceHandle>> m_devices;
    };
}

#endif //demuxusb_CINPUTFILE_H
