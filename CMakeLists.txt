cmake_minimum_required(VERSION 3.17)
project(demuxusb)
enable_testing()


find_package(PkgConfig REQUIRED)


set(CMAKE_CXX_STANDARD 20)

pkg_check_modules(PLIST REQUIRED IMPORTED_TARGET libplist-2.0)

find_package(Boost REQUIRED COMPONENTS container filesystem iostreams unit_test_framework)

add_executable(demuxusb src/main.cpp src/InputFile.cpp src/InputFile.h
        src/Device.cpp src/Device.h src/MuxStream.cpp src/MuxStream.h src/PcapNGInputFile.cpp
        src/PcapNGInputFile.h src/InterfaceExpert.cpp src/InterfaceExpert.h src/DFUInterfaceExpert.cpp
        src/DFUInterfaceExpert.h src/RecoveryInterfaceExpert.cpp src/RecoveryInterfaceExpert.h
        src/USBMUXInterfaceExpert.cpp src/USBMUXInterfaceExpert.h src/USBDescriptor.cpp src/USBDescriptor.h)

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