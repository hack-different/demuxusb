cmake_minimum_required(VERSION 3.17)
project(demuxusb)
enable_testing()


find_package(PkgConfig REQUIRED)


set(CMAKE_CXX_STANDARD 20)

pkg_check_modules(PLIST REQUIRED IMPORTED_TARGET libplist-2.0)

find_package(Boost REQUIRED COMPONENTS container filesystem iostreams unit_test_framework)

add_executable(demuxusb src/main.cpp src/file/InputFile.cpp src/file/InputFile.h
        src/usb/Device.cpp src/usb/Device.h src/protocol/MuxStream.cpp src/protocol/MuxStream.h src/file/PcapNGInputFile.cpp
        src/file/PcapNGInputFile.h src/protocol/InterfaceExpert.cpp src/protocol/InterfaceExpert.h src/protocol/DFUInterfaceExpert.cpp
        src/protocol/DFUInterfaceExpert.h src/protocol/RecoveryInterfaceExpert.cpp src/protocol/RecoveryInterfaceExpert.h
        src/protocol/USBMUXInterfaceExpert.cpp src/protocol/USBMUXInterfaceExpert.h src/usb/USBDescriptor.cpp src/usb/USBDescriptor.h)

target_include_directories(demuxusb PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/ext)
target_precompile_headers(demuxusb PRIVATE src/demuxusb.hpp)
target_link_libraries(demuxusb PUBLIC PkgConfig::PLIST Boost::container
        Boost::iostreams Boost::filesystem)


# Testing framework
add_executable(demuxusb_test_suite test/test_main.cpp test/test_PcapNGInputFile.cpp)
target_precompile_headers(demuxusb_test_suite REUSE_FROM demuxusb)
target_link_libraries(demuxusb_test_suite PUBLIC Boost::unit_test_framework)
target_include_directories(demuxusb_test_suite PRIVATE ${Boost_INCLUDE_DIRS})
add_test(NAME tests COMMAND demuxusb_test_suite)