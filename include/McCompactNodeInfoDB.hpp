#pragma once

#include <stdio.h>
#include <inttypes.h>
#include <ctype.h>
#include <string.h>
#include "McCompactStructs.hpp"
#include "esp_random.h"
#include <vector>

/**
 * @brief Handles an in-memory database of node information.
 *
 */
class NodeInfoCoreDB {
   public:
    static constexpr size_t MAX_NODES = 50;

    // Iterator for NodeInfoCoreDB
    class iterator {
       public:
        iterator(MCC_Nodeinfo* nodeinfos, bool* valid, size_t idx)
            : nodeinfos_(nodeinfos), valid_(valid), idx_(idx) {
            advance_to_valid();
        }
        iterator& operator++() {
            ++idx_;
            advance_to_valid();
            return *this;
        }
        MCC_Nodeinfo& operator*() {
            assert(idx_ < MAX_NODES && "Attempted to dereference an end() or invalid iterator");
            return nodeinfos_[idx_];
        }
        MCC_Nodeinfo* operator->() {
            assert(idx_ < MAX_NODES && "Attempted to dereference an end() or invalid iterator");
            return &nodeinfos_[idx_];
        }
        bool operator!=(const iterator& other) const { return idx_ != other.idx_; }
        bool operator==(const iterator& other) const { return idx_ == other.idx_; }

       private:
        void advance_to_valid() {
            while (idx_ < MAX_NODES && !valid_[idx_]) ++idx_;
        }
        MCC_Nodeinfo* nodeinfos_;
        bool* valid_;
        size_t idx_;
    };

    iterator begin() { return iterator(nodeinfos, valid, 0); }
    iterator end() { return iterator(nodeinfos, valid, MAX_NODES); }

    MCC_Nodeinfo* getByPubKey(const uint8_t* pubkey) {
        for (size_t i = 0; i < MAX_NODES; ++i) {
            if (valid[i] && memcmp(nodeinfos[i].pubkey, pubkey, sizeof(nodeinfos[i].pubkey)) == 0) {
                return &nodeinfos[i];
            }
        }
        return nullptr;
    }

    MCC_Nodeinfo* getByIndex(size_t index) {
        if (index < MAX_NODES && valid[index]) {
            return &nodeinfos[index];
        }
        return nullptr;
    }

    MCC_Nodeinfo* getRandomNode() {
        uint8_t cnt = 0;
        for (size_t i = 0; i < MAX_NODES; ++i) {
            if (valid[i]) {
                ++cnt;
            }
        }
        if (cnt == 0) return nullptr;
        size_t target = esp_random() % cnt;
        cnt = 0;
        for (size_t i = 0; i < MAX_NODES; ++i) {
            if (valid[i]) {
                if (cnt == target) {
                    return &nodeinfos[i];
                }
                cnt++;
            }
        }
        return nullptr;  // should never reach here
    }

    /**
     * @brief Adds or updates a node in the database.
     *
     * @param info The node information to add or update.
     * @return true if a new node was added, false if an existing node was updated.
     */
    bool addOrUpdate(const MCC_Nodeinfo& info) {
        // Try to update existing
        for (size_t i = 0; i < MAX_NODES; ++i) {
            if (valid[i] && nodeinfos[i] == info) {
                nodeinfos[i] = info;
                return false;
            }
        }
        // Add new if space
        for (size_t i = 0; i < MAX_NODES; ++i) {
            if (!valid[i]) {
                nodeinfos[i] = info;
                valid[i] = true;
                return true;
            }
        }
        // No space:  LRU (overwrite the oldest entry)
        size_t oldest_idx = 0;
        uint32_t oldest_time = nodeinfos[0].timestamp;
        for (size_t i = 1; i < MAX_NODES; ++i) {
            if (valid[i] && nodeinfos[i].timestamp < oldest_time) {
                oldest_time = nodeinfos[i].timestamp;
                oldest_idx = i;
            }
        }
        nodeinfos[oldest_idx] = info;
        return true;
    }

    /**
     * @brief Removes a node from the database.
     *
     * @param info The node information to remove.
     */
    void remove(const MCC_Nodeinfo& info) {
        for (size_t i = 0; i < MAX_NODES; ++i) {
            if (valid[i] && nodeinfos[i] == info) {
                valid[i] = false;
                memset(nodeinfos[i].pubkey, 0, sizeof(nodeinfos[i].pubkey));
                return;
            }
        }
    }
    /**
     * @brief Removes all entries from the database and the position information too.
     *
     */
    void clearAll() {
        for (size_t i = 0; i < MAX_NODES; ++i) {
            valid[i] = false;
            memset(nodeinfos[i].pubkey, 0, sizeof(nodeinfos[i].pubkey));
        }
    }

    bool serialize(std::vector<uint8_t>& data) const {
        data.clear();
        uint8_t count = 0;
        for (size_t i = 0; i < MAX_NODES; ++i) {
            if (valid[i]) ++count;
        }
        data.push_back(count);
        for (size_t i = 0; i < MAX_NODES; ++i) {
            if (!valid[i]) continue;
            const MCC_Nodeinfo& n = nodeinfos[i];
            if (n.name.size() > 255) return false;
            data.insert(data.end(), n.pubkey, n.pubkey + sizeof(n.pubkey));
            appendLE32(data, n.timestamp);
            data.push_back(n.flags);
            appendLE32(data, (uint32_t)n.latitude_i);
            appendLE32(data, (uint32_t)n.longitude_i);
            data.push_back(n.has_location ? 1 : 0);
            data.push_back((uint8_t)n.name.size());
            data.insert(data.end(), n.name.begin(), n.name.end());
        }
        return true;
    }

    bool deserialize(const std::vector<uint8_t>& data) {
        if (data.empty()) return false;
        size_t pos = 0;
        uint8_t count = data[pos++];
        if (count > MAX_NODES) return false;

        MCC_Nodeinfo loaded[MAX_NODES];
        for (uint8_t i = 0; i < count; ++i) {
            MCC_Nodeinfo& n = loaded[i];
            if (pos + sizeof(n.pubkey) + 4 + 1 + 4 + 4 + 1 + 1 > data.size()) return false;
            memcpy(n.pubkey, &data[pos], sizeof(n.pubkey));
            pos += sizeof(n.pubkey);
            n.timestamp = readLE32(&data[pos]);
            pos += 4;
            n.flags = data[pos++];
            n.latitude_i = (int32_t)readLE32(&data[pos]);
            pos += 4;
            n.longitude_i = (int32_t)readLE32(&data[pos]);
            pos += 4;
            n.has_location = data[pos++] != 0;
            uint8_t name_len = data[pos++];
            if (pos + name_len > data.size()) return false;
            n.name.assign((const char*)&data[pos], name_len);
            pos += name_len;
        }

        clearAll();
        for (uint8_t i = 0; i < count; ++i) {
            nodeinfos[i] = loaded[i];
            valid[i] = true;
        }
        return true;
    }

   private:
    static void appendLE32(std::vector<uint8_t>& out, uint32_t v) {
        out.push_back((uint8_t)(v & 0xFF));
        out.push_back((uint8_t)((v >> 8) & 0xFF));
        out.push_back((uint8_t)((v >> 16) & 0xFF));
        out.push_back((uint8_t)((v >> 24) & 0xFF));
    }

    static uint32_t readLE32(const uint8_t* p) {
        return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    }

    MCC_Nodeinfo nodeinfos[MAX_NODES];
    bool valid[MAX_NODES] = {};  // stores if the index is taken or free
};
