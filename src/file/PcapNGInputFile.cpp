//
// Created by Rick Mark on 11/15/20.
//

#include "PcapNGInputFile.h"


namespace demuxusb {
    // Block type + length at the start and the repeated length at the end
    static const size_t BLOCK_OVERHEAD = sizeof(block_t) + sizeof(uint32_t);

    struct pcapng_interface {
        uint16_t linktype;
        uint32_t snaplen;
    };

    void PcapNGInputFile::parse() {
        const auto *location = this->data();
        size_t offset = 0;
        std::vector<pcapng_interface> interfaces;

        while (offset < this->size()) {
            auto remaining = this->size() - offset;
            auto *blockStart = location + offset;

            block_t block{};
            if (remaining < BLOCK_OVERHEAD || !readStruct(blockStart, remaining, block)) {
                this->warn("Capture ends with a truncated block");
                break;
            }

            if (offset == 0 && block.type != TYPE_SECTION_HEADER) {
                throw ParseError("Not a PCAPNG file: missing section header block");
            }

            if (block.type == TYPE_SECTION_HEADER) {
                pcapng_shb_t sectionHeader{};
                if (!readStruct(blockStart + sizeof(block_t), remaining - sizeof(block_t), sectionHeader)) {
                    throw ParseError("Truncated PCAPNG section header block");
                }
                if (sectionHeader.byte_order_magic == PCAPNG_BYTE_ORDER_MAGIC_SWAPPED) {
                    throw ParseError("Big endian PCAPNG captures are not supported");
                }
                if (sectionHeader.byte_order_magic != PCAPNG_BYTE_ORDER_MAGIC) {
                    throw ParseError("Corrupt PCAPNG section header byte order magic");
                }
                // Interface ids are scoped to their section
                interfaces.clear();
            }

            if (block.length < BLOCK_OVERHEAD || block.length % 4 != 0 || block.length > remaining) {
                if (offset == 0) { throw ParseError("Corrupt PCAPNG section header block length"); }
                this->warn((boost::format("Corrupt or truncated block at offset %1%; stopping") % offset).str());
                break;
            }

            uint32_t trailingLength = 0;
            readStruct(blockStart + block.length - sizeof(uint32_t), sizeof(uint32_t), trailingLength);
            if (trailingLength != block.length) {
                if (offset == 0) { throw ParseError("Corrupt PCAPNG section header block"); }
                this->warn((boost::format("Block at offset %1% has mismatched trailing length; stopping") % offset).str());
                break;
            }

            auto *body = blockStart + sizeof(block_t);
            auto bodyLength = block.length - BLOCK_OVERHEAD;

            switch (block.type) {
                case TIME_INTERFACE_DESCRIPTOR_BLOCK: {
                    pcap_interface_descriptor_t interfaceDescriptor{};
                    if (!readStruct(body, bodyLength, interfaceDescriptor)) {
                        this->warn("Truncated interface description block");
                        break;
                    }
                    interfaces.push_back({interfaceDescriptor.linktype, interfaceDescriptor.snaplen});
                    break;
                }

                case TYPE_ENHANCED_PACKET_BLOCK: {
                    pcapng_epb_t epb{};
                    if (!readStruct(body, bodyLength, epb)) {
                        this->warn("Truncated enhanced packet block");
                        break;
                    }
                    if (epb.interface_id >= interfaces.size()) {
                        this->warn((boost::format("Packet references undeclared interface %1%") %
                                    epb.interface_id).str());
                        break;
                    }
                    if (epb.captured_len > bodyLength - sizeof(pcapng_epb_t)) {
                        this->warn("Enhanced packet block data overruns the block");
                        break;
                    }
                    this->processPacket(interfaces[epb.interface_id].linktype, body + sizeof(pcapng_epb_t),
                                        epb.captured_len);
                    break;
                }

                case TYPE_SIMPLE_PACKET_BLOCK: {
                    pcapng_spb_t spb{};
                    if (!readStruct(body, bodyLength, spb) || interfaces.empty()) {
                        this->warn("Simple packet block without an interface");
                        break;
                    }
                    size_t captured = std::min<size_t>(spb.packet_len, bodyLength - sizeof(pcapng_spb_t));
                    if (interfaces[0].snaplen != 0) { captured = std::min<size_t>(captured, interfaces[0].snaplen); }
                    this->processPacket(interfaces[0].linktype, body + sizeof(pcapng_spb_t), captured);
                    break;
                }

                default:
                    // Name resolution, statistics, custom blocks...
                    break;
            }

            offset += block.length;
        }
    }

    PcapNGInputFile::~PcapNGInputFile() = default;
}
