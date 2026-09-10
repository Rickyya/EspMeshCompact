#pragma once

#include <stdio.h>
#include <inttypes.h>
#include <ctype.h>
#include <string.h>
#include "RadioStructs.hpp"
#include "RadioLib.h"
#include "EspHal.h"
#include "esp_random.h"
#include "mbedtls/aes.h"
#include "mbedtls/md.h"
#include <string>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <deque>
#include "McCompactStructs.hpp"
#include "McCompactNodeInfoDB.hpp"
#include "McCompatChanMgr.hpp"
#include "McCompactOutQueue.hpp"
#include "McCompactSeenTable.hpp"
#include "McCompactFileIO.hpp"
#include "mbedtls/constant_time.h"
#include "esp_timer.h"
#include <ctime>

#define MAX_PACKET_PAYLOAD 184
#define PUB_KEY_SIZE 32
#define PRV_KEY_SIZE 64
#define SEED_SIZE 32
#define SIGNATURE_SIZE 64
#define CIPHER_KEY_SIZE 16
#define CIPHER_BLOCK_SIZE 16
#define CIPHER_MAC_SIZE 2
#define MAX_ADVERT_DATA_SIZE 32
#define MAX_TEXT_LEN 160

// MeshCore TXT_TYPE_* values, carried in the byte after the timestamp.
#define MCC_TXT_TYPE_PLAIN 0
#define MCC_TXT_TYPE_CLI_DATA 1
#define MCC_TXT_TYPE_SIGNED_PLAIN 2

class McCompact {
   public:
    McCompact();
    ~McCompact();
    bool RadioInit(RadioType radio_type, Radio_PINS& radio_pins, LoraConfig& lora_config);  // Initializes the radio with the given configuration and pins

    using OnRaw = void (*)(const uint8_t* data, size_t len);
    using OnNodeInfo = void (*)(const MCC_Nodeinfo& info);
    // MeshCore group text carries "<sender>: <message>"; sender is split out here.
    using OnGroupMsg = void (*)(const MCC_ChannelEntry& channel, uint32_t timestamp, const std::string& sender, const std::string& msg);

    using OnTextMessage = void (*)(const MCC_Nodeinfo& sender, uint32_t timestamp, uint8_t txt_type, const std::string& msg);
    // matched is true when this ACK confirms a message we sent; peer_pubkey is
    // then the recipient that acknowledged it, and null otherwise.
    using OnAck = void (*)(const uint8_t* peer_pubkey, uint32_t ack_code, bool matched);

    void setOnRaw(OnRaw cb) { onRaw = cb; }
    void setOnNodeInfo(OnNodeInfo cb) { onNodeInfo = cb; }
    void setOnGroupMsg(OnGroupMsg cb) { onGroupMsg = cb; }
    void setOnTextMessage(OnTextMessage cb) { onTextMessage = cb; }
    void setOnAck(OnAck cb) { onAck = cb; }

    // Whether to reply with an ACK when a direct text message arrives.
    void setAutoAck(bool enabled) { auto_ack = enabled; }

    // Whether to check the Ed25519 signature on incoming adverts. Leaving this
    // off accepts forged identities, and is only useful for protocol research.
    void setVerifyAdverts(bool enabled) { verify_adverts = enabled; }

    /**
     * @brief Rebroadcast flood packets, extending the mesh.
     *
     * Off by default, matching MeshCore's own companion firmware, where
     * Mesh::allowPacketForward() returns false in the base class and the
     * chat-node role ships with repeat disabled. The repeater, room-server and
     * sensor roles are the ones that forward out of the box.
     *
     * Every enabled node adds airtime, so turn this on deliberately.
     */
    void setRepeaterMode(bool enabled) { repeater_mode = enabled; }
    bool isRepeaterMode() const { return repeater_mode; }

    // Longest path a forwarded packet may carry before this node stops extending it.
    void setMaxFloodHops(uint8_t hops) { max_flood_hops = hops; }

    void getLastSignalData(float& rssi_out, float& snr_out) {
        rssi_out = rssi;
        snr_out = snr;
    }

    /**
     * @brief Instantaneous RSSI, i.e. the noise floor, rather than the
     *        signal of the last packet.
     *
     * A radio actually listening reports roughly -95 to -125 dBm depending
     * on band noise. A reading of 0, or one pinned to a constant, means the
     * modem is not in receive mode and no packet can ever arrive.
     * MeshCore exposes the same value as RadioLibWrapper::getCurrentRSSI.
     */
    float getCurrentRSSI();

    /**
     * @brief Carrier offset of the last received packet, in Hz.
     *
     * LoRa tolerates roughly a quarter of the bandwidth in offset, so 62.5 kHz
     * bandwidth allows about +/-15.6 kHz (18 ppm at 869 MHz) while 250 kHz
     * allows +/-62.5 kHz (72 ppm). An oscillator between those two receives
     * fine on a wide preset and is completely deaf on a narrow one.
     */
    float getFrequencyError();

    // Radio settings on the fly
    bool setRadioFrequency(float freq);
    bool setRadioSpreadingFactor(uint8_t sf);
    bool setRadioBandwidth(uint32_t bw);
    bool setRadioCodingRate(uint8_t cr);
    bool setRadioPower(int8_t power);
    // 0x12 is MeshCore (RADIOLIB_SX126X_SYNC_WORD_PRIVATE); Meshtastic uses 0x2b.
    bool setRadioSyncWord(uint8_t sync_word);
    bool setRadioPreambleLength(uint16_t symbols);

    NodeInfoCoreDB nodeinfo_db{};
    McCompatChanMgr chan_mgr{};

    // MeshCore stamps adverts and messages with epoch seconds, and uses them to
    // reject replayed adverts. Falls back to time(NULL) until setClock() is called.
    void setClock(uint32_t epoch_secs) {
        clock_base = epoch_secs;
        clock_set_us = esp_timer_get_time();
        clock_is_set = true;
    }
    uint32_t getCurrentTime() const {
        if (!clock_is_set) return (uint32_t)time(NULL);
        return clock_base + (uint32_t)((esp_timer_get_time() - clock_set_us) / 1000000);
    }

    void saveNodeDb() {
        McCompactFileIO::saveNodeDb(nodeinfo_db);
    }
    void loadNodeDb() {
        McCompactFileIO::loadNodeDb(nodeinfo_db);
    }

    void saveChannels() {
        McCompactFileIO::saveChannels(chan_mgr);
    }
    void loadChannels() {
        McCompactFileIO::loadChannels(chan_mgr);
    }

    void savePrivKey() {
        McCompactFileIO::savePrivateKey(my_nodeinfo);
    }
    // Restores the stored identity, generating and persisting a new one on first boot.
    void loadPrivKey() {
        if (!McCompactFileIO::loadPrivateKey(my_nodeinfo)) {
            my_nodeinfo.generateKeyPair();
            McCompactFileIO::savePrivateKey(my_nodeinfo);
        }
    }

    static int decrypt(const uint8_t* shared_secret, uint8_t* dest, size_t dest_len, const uint8_t* src, int src_len);
    static int encrypt(const uint8_t* shared_secret, uint8_t* dest, const uint8_t* src, int src_len);
    static int encryptThenMAC(const uint8_t* shared_secret, uint8_t* dest, const uint8_t* src, int src_len);
    static int MACThenDecrypt(const uint8_t* shared_secret, uint8_t* dest, size_t dest_len, const uint8_t* src, int src_len);
    static int secure_memcmp(const void* a, const void* b, size_t size);

    // To enable or disable this module's logging to serial
    void setDebugMode(bool enabled) {
        debugmode = enabled;
    }
    /**
     * @brief Set the Send Enabled
     *
     * @param enabled If set to false, no packets will be sent, but we still receive them.
     */
    void setSendEnabled(bool enabled) {
        is_send_enabled = enabled;
    }

    // nodeinfo settings

    void setMyNames(const std::string& name) {
        my_nodeinfo.name = name;
    }
    // Degrees; the wire format carries micro-degrees. Signed, so the southern
    // and western hemispheres survive the conversion.
    void setMyLocation(float latitude, float longitude) {
        my_nodeinfo.latitude_i = static_cast<int32_t>(latitude * 1e6);
        my_nodeinfo.longitude_i = static_cast<int32_t>(longitude * 1e6);
        my_nodeinfo.has_location = true;
    }

    void setMyPrivateKey(const uint8_t* priv_key) {
        my_nodeinfo.setPrivateKey(priv_key);
    }
    void generateMyKeyPair() {
        my_nodeinfo.generateKeyPair();
    }

    void setMyTypeFlags(MCC_NODEINFO_FLAGS flag) {  // MCC_NODEINFO_FLAGS::IS_CHAT_NODE,  ... only 1
        my_nodeinfo.flags = (uint8_t)flag;
    }

    MCC_MyNodeInfo* getMyNodeInfo() {
        return &my_nodeinfo;
    }

    // senders
    void sendNodeInfo(const MCC_MyNodeInfo& info);
    void sendMyNodeInfo() {
        sendNodeInfo(my_nodeinfo);
    }
    void sendGroupMsg(const MCC_ChannelEntry& channel, const std::string& msg);
    void sendTextMessage(MCC_Nodeinfo& peer, const std::string& msg, uint8_t attempt = 0);
    void sendNeighborDiscoveryRequest(uint8_t filter = 15, std::vector<uint32_t> path = {});

   private:
    RadioType radio_type;
    bool RadioListen();    // inits the listening thread for the radio
    bool RadioSendInit();  // inits the sending thread for the radio. consumes the out_queue
                           // handlers

    // internal events
    void intOnNodeInfo(MCC_Nodeinfo& info);  // internal handler for nodeinfo packets

    // decoding
    int16_t ProcessPacket(uint8_t* data, int len, McCompact* mshcomp);  // Process the packet, decode it, and call the appropriate handler

    static void task_listen(void* pvParameters);  // Task for listening to the radio and processing incoming packets
    static void task_send(void* pvParameters);    // Task for sending packets from the out_queue

    float rssi, snr;  // store last signal data

    EspHal* hal;           // = new EspHal(9, 11, 10);
    PhysicalLayer* radio;  // SX1262 radio = new Module(hal, 8, 14, 12, 13);

    mutable std::mutex mtx_radio;
    bool need_run = true;  // thread exit flag

    bool debugmode = false;  // if true, enables debug logging

    bool is_send_enabled = true;  // if false, disables sending of packets

    uint32_t clock_base = 0;
    int64_t clock_set_us = 0;
    bool clock_is_set = false;

    McCompactSeenTable seen_table;

    MCC_MyNodeInfo my_nodeinfo;
    OnRaw onRaw = nullptr;
    OnNodeInfo onNodeInfo = nullptr;
    OnGroupMsg onGroupMsg = nullptr;
    OnTextMessage onTextMessage = nullptr;
    OnAck onAck = nullptr;

    bool auto_ack = true;
    bool verify_adverts = true;
    bool repeater_mode = false;
    uint8_t max_flood_hops = 8;

    void retransmitFlood(const MCC_Header& header, const uint8_t* data, int len);

    // Messages we have sent and are still expecting an ACK for. Small ring;
    // the oldest entry is silently overwritten.
    struct PendingAck {
        uint32_t expected;
        uint8_t peer_pubkey[PUB_KEY_SIZE];
        bool valid;
    };
    static constexpr size_t MAX_PENDING_ACKS = 8;
    PendingAck pending_acks[MAX_PENDING_ACKS] = {};
    size_t next_pending_ack = 0;

    void sendAckFor(const uint8_t* plain, int plain_len, const uint8_t* sender_pubkey);

    McCompactOutQueue out_queue;  // Outgoing queue for packets to be sent
};
