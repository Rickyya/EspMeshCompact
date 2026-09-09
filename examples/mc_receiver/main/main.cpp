#include <stdio.h>
#include <inttypes.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

#include "McCompact.hpp"
#include "McCompactHelpers.hpp"

Radio_PINS radio_pins = {9, 11, 10, 8, 14, 12, 13};  // Default radio pins for Heltec WSL V3. // https://github.com/meshtastic/firmware/blob/81828c6244daede254cf759a0f2bd939b2e7dd65/variants/heltec_wsl_v3/variant.h

LoraConfig lora_config_mc = {
    /*.frequency = */ 869.618,   // config
    /*.bandwidth = */ 62.5,      // config
    /*.spreading_factor = */ 8,  // config
    /*.coding_rate = */ 8,       // config
    /*.sync_word = */ 0x12,
    /*.preamble_length = */ 16,
    /*.output_power = */ 22,  // config
    /*.tcxo_voltage = */ 1.8,
    /*.use_regulator_ldo = */ false,
};  //

McCompact mesh;

extern "C" void app_main(void) {
    // nvsinit.must be done!
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    mesh.setDebugMode(true);  // Enable debug logging
    mesh.RadioInit(RadioType::SX1262, radio_pins, lora_config_mc);
    mesh.chan_mgr.addChannel("TTest", "38c42a7bf8c329d8e7759ac8b1f83d96");
    mesh.chan_mgr.addChannel("Public", "8b3387e9c5cdea6ac9e5edbaa115cd72");
    mesh.chan_mgr.addChannel("hungary", "d2ad7e4009b727fb4ee5c1ff51694e5e");
    mesh.chan_mgr.addChannel("ping", "3cae16fd067ba9c32a98be22e9b98525");
    mesh.chan_mgr.addChannel("info", "ce51a275a0a0507c43d1651d78292320");

    mesh.setOnRaw([](const uint8_t* data, size_t len) {
        printf("Raw data received: ");
        for (size_t i = 0; i < len; ++i) {
            printf("%02X ", data[i]);
        }
        printf("\n");
    });

    mesh.setOnNodeInfo([](const MCC_Nodeinfo& info) {
        printf("Node Info received: NodeID=%u, Name=%s\n",
               *((uint8_t*)&info.pubkey[28]),
               info.name.c_str());
    });
    mesh.setOnGroupMsg([](const MCC_ChannelEntry& channel, uint32_t timestamp, const std::string& sender, const std::string& msg) {
        printf("Group Msg on %s at %" PRIu32 " from %s: %s\n", channel.name.c_str(), timestamp, sender.c_str(), msg.c_str());
    });
    mesh.setOnTextMessage([](const MCC_Nodeinfo& sender, uint32_t timestamp, uint8_t txt_type, const std::string& msg) {
        printf("DM from %s at %" PRIu32 " (type %u): %s\n", sender.name.c_str(), timestamp, txt_type, msg.c_str());
        // Echo it back. The callback hands out a const view, so re-find the
        // contact in the DB to get a reference we can cache the secret on.
        for (auto& peer : mesh.nodeinfo_db) {
            if (memcmp(peer.pubkey, sender.pubkey, 32) == 0) {
                mesh.sendTextMessage(peer, "ack: " + msg);
                break;
            }
        }
    });

    mesh.setOnAck([](const uint8_t* peer_pubkey, uint32_t ack_code, bool matched) {
        if (matched) {
            printf("Delivered: %02x%02x... acknowledged 0x%08" PRIx32 "\n", peer_pubkey[0], peer_pubkey[1], ack_code);
        } else {
            printf("ACK 0x%08" PRIx32 " seen, not for us\n", ack_code);
        }
    });

    std::string name = "TestNode";
    // NodeInfoBuilder takes micro-degrees, not degrees.
    McCompactHelpers::NodeInfoBuilder(mesh.getMyNodeInfo(), name, 47497900, 19040200, MCC_NODEINFO_FLAGS::IS_CHAT_NODE);

    // Reuse the stored identity so this node keeps the same address across reboots.
    mesh.loadPrivKey();
    mesh.loadChannels();
    mesh.loadNodeDb();

    // No RTC here; a real deployment would set this from SNTP or a GPS fix.
    mesh.setClock(1757000000);

    int tick = 0;
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(30000));
        if (tick % 4 == 0) {
            mesh.sendMyNodeInfo();  // advertise ourselves
        } else if (tick % 4 == 2) {
            MCC_ChannelEntry* pub = mesh.chan_mgr.getChannelByName("Public");
            if (pub) mesh.sendGroupMsg(*pub, "hello from EspMeshCompact");
        } else {
            mesh.sendNeighborDiscoveryRequest(255);
        }
        tick++;
    }
}