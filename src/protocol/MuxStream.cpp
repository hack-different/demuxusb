//
// Created by Rick Mark on 11/14/20.
//

#include "MuxStream.h"

void MuxStream::receiveBytes(size_t length, std::byte *start) {
    this->m_bytesIn += length;

    if (this->shouldMaterializePlist())
    {

    }
}

void MuxStream::sendBytes(size_t length, std::byte *start) {
    this->m_bytesOut += length;

    if (this->shouldMaterializePlist() && length > sizeof(uint32_t))
    {
        plist_service_item* item = nullptr;
        // This is often the first request
        if (this->m_inItems.empty()) {
            uint32_t size = *((uint32_t*)start);
            item = new plist_service_item{size, 0, static_cast<std::byte *>(malloc(size))};
            start += sizeof(uint32_t);
            length -= sizeof(uint32_t);

            this->m_inItems.push_back(std::make_unique<plist_service_item>(*item));
        } else {
            item = this->m_inItems[this->m_inItems.size() - 1].get();

            if (item->size == item->received) {
                uint32_t size = *((uint32_t*)start);
                item = new plist_service_item{size, 0, static_cast<std::byte *>(malloc(size))};
                start += sizeof(uint32_t);
                length -= sizeof(uint32_t);

                this->m_inItems.push_back(std::make_unique<plist_service_item>(*item));
            }
        }

        auto handled_bytes = this->handleBytes(*item, length, start);
        if (handled_bytes != length) {
            auto recursive_length = length - handled_bytes;
            auto* recursive_data = start + handled_bytes;

            this->receiveBytes(recursive_length, recursive_data);
        }
    }
}

MuxStream::~MuxStream() {
    for (auto& in_item : this->m_inItems) {
        if (in_item->data != nullptr) {
            free(in_item->data);
            in_item->data = nullptr;
        }
        if (in_item->plist != nullptr) {
            plist_free(in_item->plist);
            in_item->plist = nullptr;
        }
    }
    for (auto& out_item : this->m_outItems) {
        if (out_item->data != nullptr) {
            free(out_item->data);
            out_item->data = nullptr;
        }
        if (out_item->plist != nullptr) {
            plist_free(out_item->plist);
            out_item->plist = nullptr;
        }
    }
}

void MuxStream::materialize(plist_service_item& item) {
    plist_from_memory(reinterpret_cast<const char *>(item.data), item.size, &item.plist);

    if (PLIST_IS_DICT(item.plist)) {
        plist_dict_iter it{};
        char* key = nullptr;
        plist_t value;
        plist_dict_new_iter(item.plist, &it);


    }
}

size_t MuxStream::handleBytes(plist_service_item &item, size_t length, std::byte *data) {
    auto remaining = item.size - item.received;
    auto available = std::min((size_t)remaining, (size_t)length);
    std::memcpy(item.data + item.received, data, available);

    if (item.received == item.size) { this->materialize(item); }

    return available;
}
