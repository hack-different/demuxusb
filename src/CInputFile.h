//
// Created by Rick Mark on 11/14/20.
//

#ifndef DEUSBMUX_CINPUTFILE_H
#define DEUSBMUX_CINPUTFILE_H

#include <boost/filesystem.hpp>
#include <boost/iostreams/device/mapped_file.hpp>
#include <memory>

#include <pcapng.h>

#include "CDevice.h"

namespace fs = boost::filesystem;
namespace io = boost::iostreams;

class CInputFile {
public:
    explicit CInputFile(fs::path path);

    void parse();
protected:
    fs::path m_path;
    io::mapped_file_source m_mappedInputFile;
    std::map<uint32_t, std::unique_ptr<CDevice>> m_devices;
};


#endif //DEUSBMUX_CINPUTFILE_H
