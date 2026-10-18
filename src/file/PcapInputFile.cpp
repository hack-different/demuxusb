#include "PcapInputFile.h"


namespace demuxusb {
    void PcapInputFile::parse() {
        pcap_file_header_t header{};
        if (!readStruct(this->data(), this->size(), header)) {
            throw ParseError("Truncated pcap file header");
        }

        bool swapped;
        switch (header.magic) {
            case PCAP_MAGIC_MICROSECONDS:
            case PCAP_MAGIC_NANOSECONDS:
                swapped = false;
                break;
            case PCAP_MAGIC_MICROSECONDS_SWAPPED:
            case PCAP_MAGIC_NANOSECONDS_SWAPPED:
                swapped = true;
                break;
            default:
                throw ParseError("Not a pcap file: bad magic");
        }

        auto value = [swapped](uint32_t field) { return swapped ? boost::endian::endian_reverse(field) : field; };

        // The upper bits of the link type field carry FCS information
        auto linktype = value(header.linktype) & 0x0FFFFFFFU;
        size_t offset = sizeof(pcap_file_header_t);

        while (offset < this->size()) {
            auto remaining = this->size() - offset;
            pcap_record_header_t record{};
            if (!readStruct(this->data() + offset, remaining, record)) {
                this->warn("Capture ends with a truncated record header");
                break;
            }

            auto included = value(record.incl_len);
            if (included > remaining - sizeof(pcap_record_header_t)) {
                this->warn((boost::format("Record at offset %1% overruns the file; stopping") % offset).str());
                break;
            }

            this->processPacket(linktype, this->data() + offset + sizeof(pcap_record_header_t), included);
            offset += sizeof(pcap_record_header_t) + included;
        }
    }
}
