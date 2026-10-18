cmake_minimum_required(VERSION 3.17)
project(demuxusb CXX)
enable_testing()

find_package(PkgConfig REQUIRED)


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(DEMUXUSB_SANITIZE "Build with address and undefined behaviour sanitizers" OFF)
if (DEMUXUSB_SANITIZE)
    add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif ()

pkg_check_modules(PLIST REQUIRED IMPORTED_TARGET libplist-2.0)

find_package(Boost REQUIRED COMPONENTS container filesystem iostreams unit_test_framework)

add_library(demuxusb_lib STATIC
        src/file/InputFile.cpp src/file/InputFile.h
        src/file/PcapNGInputFile.cpp src/file/PcapNGInputFile.h
        src/file/PcapInputFile.cpp src/file/PcapInputFile.h
        src/usb/Device.cpp src/usb/Device.h
        src/protocol/MuxStream.cpp src/protocol/MuxStream.h
        src/protocol/InterfaceExpert.cpp src/protocol/InterfaceExpert.h
        src/protocol/DFUInterfaceExpert.cpp src/protocol/DFUInterfaceExpert.h
        src/protocol/RecoveryInterfaceExpert.cpp src/protocol/RecoveryInterfaceExpert.h
        src/protocol/USBMUXInterfaceExpert.cpp src/protocol/USBMUXInterfaceExpert.h)
set_target_properties(demuxusb_lib PROPERTIES OUTPUT_NAME demuxusb)

target_include_directories(demuxusb_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/ext)
target_compile_options(demuxusb_lib PRIVATE -Wall)
target_link_libraries(demuxusb_lib PUBLIC PkgConfig::PLIST Boost::container Boost::iostreams Boost::filesystem)
if (PLIST_VERSION VERSION_GREATER_EQUAL 2.3)
    target_compile_definitions(demuxusb_lib PRIVATE HAVE_PLIST_MEM_FREE)
endif ()

add_executable(demuxusb src/main.cpp)
target_link_libraries(demuxusb PRIVATE demuxusb_lib)


# Testing framework
add_executable(demuxusb_test_suite test/test_main.cpp test/CaptureBuilder.h test/test_PcapNGInputFile.cpp
        test/test_PcapInputFile.cpp test/test_Device.cpp test/test_USBMUX.cpp test/test_DFU.cpp
        test/test_Recovery.cpp test/test_Robustness.cpp)
target_link_libraries(demuxusb_test_suite PRIVATE demuxusb_lib Boost::unit_test_framework)
add_test(NAME tests COMMAND demuxusb_test_suite)
