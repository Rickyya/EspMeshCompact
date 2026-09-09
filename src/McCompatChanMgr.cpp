#include "esp_log.h"
#include "McCompatChanMgr.hpp"
#include "CompactHelpers.hpp"
#include "McCompact.hpp"

McCompatChanMgr::McCompatChanMgr() {}
McCompatChanMgr::~McCompatChanMgr() {}
bool McCompatChanMgr::addChannel(const std::string& name, const uint8_t* secret, size_t size) {
    MCC_ChannelEntry entry;
    entry.name = name;
    memcpy(entry.secret, secret, size);
    CompactHelpers::sha256(entry.hash, sizeof(entry.hash), secret, size);
    entry.secret_len = size;
    channels.push_back(entry);
    return true;
}

bool McCompatChanMgr::addChannel(const std::string& name, const std::string& secret) {
    uint8_t key[32] = {0};
    auto len = CompactHelpers::keyFromString(secret, key);
    return addChannel(name, key, len);
}

MCC_ChannelEntry* McCompatChanMgr::getChannelByName(const std::string& name) {
    for (auto& channel : channels) {
        if (channel.name == name) {
            return &channel;
        }
    }
    return nullptr;
}

MCC_ChannelEntry* McCompatChanMgr::getChannelByIndex(size_t index) {
    if (index < channels.size()) {
        return &channels[index];
    }
    return nullptr;
}

size_t McCompatChanMgr::getChannelCount() {
    return channels.size();
}

MCC_ChannelEntry* McCompatChanMgr::getChannelByHashAndData(uint8_t* payload, size_t payload_len, uint8_t* decoded, size_t& out_decoded_len) {
    // todo
    for (auto& channel : channels) {
        // ESP_LOGI("ChanMgr", "Trying channel %s with hash 0x%02x  :   0x%02x ", channel.name.c_str(), channel.hash[0], payload[0]);
        if (channel.hash[0] == payload[0]) {
            ESP_LOGI("ChanMgr", "Channel %s matched hash", channel.name.c_str());
            auto lenn = McCompact::MACThenDecrypt(channel.secret, decoded, MAX_PACKET_PAYLOAD, payload + 1, payload_len - 1);
            if (lenn > 0) {  // success!
                out_decoded_len = lenn;
                ESP_LOGI("ChanMgr", "Decrypted with channel %s", channel.name.c_str());
                return &channel;
            } else {
                ESP_LOGI("ChanMgr", "Failed to decrypt with channel %s, lenn: %d", channel.name.c_str(), lenn);
            }
        }
    }
    out_decoded_len = 0;
    return nullptr;
}

bool McCompatChanMgr::serialize(std::vector<uint8_t>& out) const {
    out.clear();
    if (channels.size() > 255) return false;
    out.push_back((uint8_t)channels.size());
    for (const auto& ch : channels) {
        if (ch.name.size() > 255 || ch.secret_len > sizeof(ch.secret)) return false;
        out.push_back((uint8_t)ch.name.size());
        out.insert(out.end(), ch.name.begin(), ch.name.end());
        out.push_back(ch.secret_len);
        out.insert(out.end(), ch.secret, ch.secret + ch.secret_len);
    }
    return true;
}

// Leaves the live list untouched unless the whole blob parses.
bool McCompatChanMgr::deserialize(const std::vector<uint8_t>& in) {
    if (in.empty()) return false;
    size_t pos = 0;
    uint8_t count = in[pos++];
    std::vector<MCC_ChannelEntry> loaded;
    for (uint8_t i = 0; i < count; ++i) {
        if (pos >= in.size()) return false;
        uint8_t name_len = in[pos++];
        if (pos + name_len >= in.size()) return false;
        MCC_ChannelEntry entry;
        entry.name.assign((const char*)&in[pos], name_len);
        pos += name_len;
        uint8_t secret_len = in[pos++];
        if (secret_len > sizeof(entry.secret) || pos + secret_len > in.size()) return false;
        entry.secret_len = secret_len;
        memcpy(entry.secret, &in[pos], secret_len);
        pos += secret_len;
        CompactHelpers::sha256(entry.hash, sizeof(entry.hash), entry.secret, secret_len);
        loaded.push_back(entry);
    }
    channels = loaded;
    return true;
}