//
// Created by Rick Mark on 11/14/20.
//

#ifndef demuxusb_CINPUTFILE_H
#define demuxusb_CINPUTFILE_H

#include "../demuxusb.hpp"
#include "../usb/Device.h"


namespace demuxusb {

    // Correlates the submission and completion events of USB transfers on one device address and hands completed
    //  transfers to the Device.  Capture formats differ in which event carries the payload (OUT data travels with the
    //  submission, IN data with the completion), so both are retained and the right one chosen at completion.
    class DeviceHandle {
    public:
        explicit DeviceHandle(uint64_t id) : m_id{id} {
            this->m_sessions.push_back(std::make_unique<Device>(id));
        }

        // Starts a control transfer.  data is any OUT payload that followed the setup packet.
        void beginControl(uint8_t endpoint, uint64_t id, const usb_setup_t &setup, byte_array data = {nullptr, 0});

        // Adds payload from an intermediate stage (USBPcap emits a separate DATA stage).
        void appendControl(uint8_t endpoint, uint64_t id, byte_array data);

        // Completes a control transfer and dispatches it.  data is the payload of the completion event.
        void endControl(uint8_t endpoint, uint64_t id, byte_array data);

        void beginBulk(uint8_t endpoint, uint64_t id, byte_array data = {nullptr, 0});

        void endBulk(uint8_t endpoint, uint64_t id, byte_array data);

        // Drops an in-flight transfer that completed with an error.
        void abort(uint8_t endpoint, uint64_t id);

        // The current device session on this address
        Device *device() { return this->m_sessions.back().get(); }

        // Every session seen on this address.  A new session begins when the address re-enumerates as a different
        //  device, e.g. DFU -> recovery -> normal mode during a restore.
        std::vector<Device *> devices();

        [[nodiscard]] uint64_t getUnmatchedCount() const { return this->m_unmatched; }

    private:
        struct Pending {
            bool control{};
            usb_setup_t setup{};
            std::vector<std::byte> data{};
        };

        typedef std::pair<uint8_t, uint64_t> transfer_key;

        void dispatchControl(uint8_t endpoint, const usb_setup_t &setup, byte_array data);

        uint64_t m_id;
        uint64_t m_unmatched{};
        std::map<transfer_key, Pending> m_inflight{};
        std::vector<std::unique_ptr<Device>> m_sessions{};
    };

    class InputFile {
    public:
        explicit InputFile(fs::path path);

        virtual ~InputFile() = default;

        // Opens a capture, choosing the reader from the file's magic number.  Throws ParseError if the format is not
        //  recognised.
        static std::unique_ptr<InputFile> open(const fs::path &path);

        virtual void parse() = 0;

        // Devices in the order they first appeared in the capture
        std::vector<Device *> getDevices();

        // Problems with individual packets (truncation, unknown link types, unmatched completions...).  Only the first
        //  MAX_WARNINGS are retained; getWarningCount() has the total.
        [[nodiscard]] const std::vector<std::string> &getWarnings() const { return this->m_warnings; }

        [[nodiscard]] uint64_t getWarningCount() const { return this->m_warningCount; }

        [[nodiscard]] uint64_t getPacketCount() const { return this->m_packetCount; }

        // Dispatches one captured USB packet of the given link type.  Returns false if it could not be decoded.
        bool processPacket(uint32_t linktype, const std::byte *data, size_t length);

        static const size_t MAX_WARNINGS = 100;

    protected:
        bool process_darwin_packet(const std::byte *data, size_t length);

        bool process_usbpcap_packet(const std::byte *data, size_t length);

        bool process_usbmon_packet(const std::byte *data, size_t length, bool mmapped);

        DeviceHandle &handleFor(uint64_t id);

        void warn(const std::string &warning);

        [[nodiscard]] const std::byte *data() const { return this->m_data; }

        [[nodiscard]] size_t size() const { return this->m_size; }

        fs::path m_path;
        io::mapped_file_source m_mappedInputFile;
        const std::byte *m_data{};
        size_t m_size{};
        uint64_t m_packetCount{};
        uint64_t m_warningCount{};
        std::vector<std::string> m_warnings{};
        std::set<uint32_t> m_unsupportedLinktypes{};
        std::map<uint64_t, std::shared_ptr<DeviceHandle>> m_devices;
        std::vector<uint64_t> m_deviceOrder{};
    };
}

#endif //demuxusb_CINPUTFILE_H
