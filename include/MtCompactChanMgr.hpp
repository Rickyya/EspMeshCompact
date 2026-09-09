#pragma once

#include <stdint.h>
#include <string>
#include <vector>
#include "MtCompactStructs.hpp"
#include <stddef.h>

class MtCompatChanMgr {
   public:
    void addDefaultChannels();
    void addDefaultEncryption(std::string name);

    void addChannel(std::string name, uint8_t* secret, size_t secret_len = 32) {
        MTC_ChannelEntry entry(name, secret, secret_len);
        for (const auto& ch : channels) {
            if (ch == entry) {
                return;  // Channel already exists
            }
        }
        channels.push_back(entry);
    }

    MTC_ChannelEntry* getChannelByName(const std::string& name) {
        for (auto& ch : channels) {
            if (ch.name == name) {
                return &ch;
            }
        }
        return nullptr;
    }

    MTC_ChannelEntry* getChannelByHash(uint8_t hash) {
        for (auto& ch : channels) {
            if (ch.hash[0] == hash) {
                return &ch;
            }
        }
        return nullptr;
    }

    /**
     * @brief Serialise the channel list into a flat buffer.
     *
     * Layout: count (1), then per entry
     *   name length (1) | name | secret length (1) | secret
     * The hash is recomputed on load rather than stored, so it can never
     * disagree with the name and secret it is derived from.
     */
    bool serialize(std::vector<uint8_t>& out) const {
        out.clear();
        if (channels.size() > 255) {
            return false;
        }
        out.push_back((uint8_t)channels.size());
        for (const auto& ch : channels) {
            if (ch.name.size() > 255 || ch.secret_len > sizeof(ch.secret)) {
                return false;
            }
            out.push_back((uint8_t)ch.name.size());
            out.insert(out.end(), ch.name.begin(), ch.name.end());
            out.push_back((uint8_t)ch.secret_len);
            out.insert(out.end(), ch.secret, ch.secret + ch.secret_len);
        }
        return true;
    }

    /**
     * @brief Replace the channel list from a buffer written by serialize().
     *
     * The existing list is left untouched unless the whole buffer parses, so a
     * truncated or corrupt blob cannot leave a half-populated channel table.
     */
    bool deserialize(const std::vector<uint8_t>& in) {
        size_t pos = 0;
        if (in.empty()) {
            return false;
        }
        uint8_t count = in[pos++];
        std::vector<MTC_ChannelEntry> loaded;
        for (uint8_t i = 0; i < count; ++i) {
            if (pos >= in.size()) return false;
            uint8_t name_len = in[pos++];
            if (pos + name_len >= in.size()) return false;
            MTC_ChannelEntry entry;
            entry.name.assign((const char*)&in[pos], name_len);
            pos += name_len;
            uint8_t secret_len = in[pos++];
            if (secret_len > sizeof(entry.secret) || pos + secret_len > in.size()) return false;
            entry.secret_len = secret_len;
            memcpy(entry.secret, &in[pos], secret_len);
            pos += secret_len;
            entry.calcHash();
            loaded.push_back(entry);
        }
        channels = loaded;
        return true;
    }

    std::vector<MTC_ChannelEntry> channels;
};