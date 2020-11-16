//
// Created by Rick Mark on 11/14/20.
//

#ifndef DEUSBMUX_CINPUTFILE_H
#define DEUSBMUX_CINPUTFILE_H

#include <boost/filesystem.hpp>
#include <boost/iostreams/device/mapped_file.hpp>
#include <memory>

#include <pcapng.h>

#include "Device.h"

#define USB_ENDPOINT_IN(endpoint) (endpoint & 0x80)
#define USB_ENDPOINT_OUT(endpoint) (!(endpoint & 0x80))
#define USB_ENDPOINT_ID(endpoint) (endpoint & 0x7F)

namespace deusbmux {
    namespace fs = boost::filesystem;
    namespace io = boost::iostreams;

    typedef struct {
        uint8_t endpoint;
        uint32_t length;

    } usb_frame_info_t;

    class InputFile {
    public:
        explicit InputFile(fs::path path);

        virtual void parse() = 0;
        std::map<uint32_t, std::shared_ptr<Device>> getDevices();

    protected:
        fs::path m_path;
        io::mapped_file_source m_mappedInputFile;
        std::map<uint32_t, std::shared_ptr<Device>> m_devices;
    };
}

#endif //DEUSBMUX_CINPUTFILE_H
