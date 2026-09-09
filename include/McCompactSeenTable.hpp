#pragma once

#include <stdint.h>
#include <string.h>
#include "CompactHelpers.hpp"

// Ring of truncated packet hashes, matching mesh::Packet::calculatePacketHash.
class McCompactSeenTable {
   public:
    static constexpr size_t MAX_HASHES = 160;
    static constexpr size_t HASH_SIZE = 8;

    McCompactSeenTable() {
        memset(hashes, 0, sizeof(hashes));
    }

    static void calcHash(uint8_t* out, uint8_t payload_type, const uint8_t* payload, size_t payload_len) {
        CompactHelpers::sha256(out, HASH_SIZE, &payload_type, 1, payload, (int)payload_len);
    }

    bool wasSeen(uint8_t payload_type, const uint8_t* payload, size_t payload_len) const {
        uint8_t hash[HASH_SIZE];
        calcHash(hash, payload_type, payload, payload_len);
        return contains(hash);
    }

    void markSeen(uint8_t payload_type, const uint8_t* payload, size_t payload_len) {
        uint8_t hash[HASH_SIZE];
        calcHash(hash, payload_type, payload, payload_len);
        store(hash);
    }

    bool checkAndMark(uint8_t payload_type, const uint8_t* payload, size_t payload_len) {
        uint8_t hash[HASH_SIZE];
        calcHash(hash, payload_type, payload, payload_len);
        if (contains(hash)) {
            dup_count++;
            return true;
        }
        store(hash);
        return false;
    }

    void clear() {
        memset(hashes, 0, sizeof(hashes));
        next_idx = 0;
        dup_count = 0;
    }

    uint32_t getDuplicateCount() const { return dup_count; }

   private:
    bool contains(const uint8_t* hash) const {
        const uint8_t* sp = hashes;
        for (size_t i = 0; i < MAX_HASHES; ++i, sp += HASH_SIZE) {
            if (memcmp(hash, sp, HASH_SIZE) == 0) return true;
        }
        return false;
    }

    void store(const uint8_t* hash) {
        memcpy(&hashes[next_idx * HASH_SIZE], hash, HASH_SIZE);
        next_idx = (next_idx + 1) % MAX_HASHES;
    }

    uint8_t hashes[MAX_HASHES * HASH_SIZE];
    size_t next_idx = 0;
    uint32_t dup_count = 0;
};
