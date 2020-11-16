cmake_minimum_required(VERSION 3.17)
project(deusbmux)

find_package(PkgConfig REQUIRED)


set(CMAKE_CXX_STANDARD 20)

pkg_check_modules(PLIST REQUIRED IMPORTED_TARGET libplist-2.0 )

find_package( Boost REQUIRED COMPONENTS container filesystem iostreams )

add_executable(deusbmux src/main.cpp src/InputFile.cpp src/InputFile.h
        src/Device.cpp src/Device.h src/MuxStream.cpp src/MuxStream.h src/PcapNGInputFile.cpp src/PcapNGInputFile.h src/InterfaceExpert.cpp src/InterfaceExpert.h src/DFUInterfaceExpert.cpp src/DFUInterfaceExpert.h src/RecoveryInterfaceExpert.cpp src/RecoveryInterfaceExpert.h src/USBMUXInterfaceExpert.cpp src/USBMUXInterfaceExpert.h src/USBDescriptor.cpp src/USBDescriptor.h)

target_include_directories(deusbmux PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/ext)

target_link_libraries(deusbmux PUBLIC PkgConfig::PLIST Boost::container
        Boost::iostreams Boost::filesystem)