// Meshtastic receive demo: joins the default LongFast channel, prints incoming
// text messages, node info and positions.
//
// Its second job is to be the thing that links MtCompact in CI -- examples/mc_receiver
// exercises only the MeshCore half, so a break in MtCompact used to go unnoticed.

#include <stdio.h>
#include <inttypes.h>
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_mac.h"

#include "MtCompact.hpp"
#include "MtCompactHelpers.hpp"

// Heltec WSL V3 pinout.
// https://github.com/meshtastic/firmware/blob/81828c6244daede254cf759a0f2bd939b2e7dd65/variants/heltec_wsl_v3/variant.h
Radio_PINS radio_pins = {9, 11, 10, 8, 14, 12, 13};

// EU_868 LongFast.
LoraConfig lora_config_mt = {
    /*.frequency = */ 869.525,
    /*.bandwidth = */ 250.0,
    /*.spreading_factor = */ 11,
    /*.coding_rate = */ 5,
    /*.sync_word = */ 0x2b,
    /*.preamble_length = */ 16,
    /*.output_power = */ 22,
    /*.tcxo_voltage = */ 1.8,
    /*.use_regulator_ldo = */ false,
};

MtCompact mesh;

static uint32_t nodeIdFromMac() {
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    return ((uint32_t)mac[2] << 24) | ((uint32_t)mac[3] << 16) | ((uint32_t)mac[4] << 8) | mac[5];
}

extern "C" void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    mesh.setDebugMode(true);
    mesh.RadioInit(RadioType::SX1262, radio_pins, lora_config_mt);
    // Channels persist across reboots; seed the defaults only on first boot.
    mesh.loadChannels();
    if (mesh.chan_mgr.channels.empty()) {
        mesh.chan_mgr.addDefaultChannels();
        mesh.saveChannels();
    }

    uint32_t node_id = nodeIdFromMac();
    std::string short_name = "MTL";
    std::string long_name = "MtListener";
    MtCompactHelpers::NodeInfoBuilder(mesh.getMyNodeInfo(), node_id, short_name, long_name, 0);

    mesh.loadPrivKey();
    mesh.loadNodeDb();
    mesh.printKeysHex();

    mesh.setOnMessage([](MCT_Header& header, MCT_TextMessage& message) {
        printf("Text from 0x%08" PRIx32 " (chan %u, rssi %.0f snr %.1f): %s\n",
               header.srcnode, message.chan, header.rssi, header.snr, message.text.c_str());
    });

    mesh.setOnNodeInfoMessage([](MCT_Header& header, MCT_NodeInfo& nodeinfo, bool needReply, bool newNode) {
        printf("NodeInfo from 0x%08" PRIx32 ": %s (%s)%s\n",
               header.srcnode, nodeinfo.long_name, nodeinfo.short_name, newNode ? " [new]" : "");
    });

    mesh.setOnRouting([](MCT_Header& header, MCT_Routing& routing) {
        static const char* kind[] = {"ACK", "NAK", "ROUTE_REQUEST", "ROUTE_REPLY"};
        printf("Routing from 0x%08" PRIx32 ": %s for packet 0x%08" PRIx32 " (error %u)\n",
               header.srcnode, kind[routing.type], routing.request_id, routing.error_reason);
    });

    mesh.setOnWaypointMessage([](MCT_Header& header, MCT_Waypoint& waypoint) {
        printf("Waypoint 0x%08" PRIx32 " from 0x%08" PRIx32 ": %s (%zu stored)\n",
               waypoint.id, header.srcnode, waypoint.name, mesh.waypoint_db.count());
    });

    mesh.setOnPositionMessage([](MCT_Header& header, MCT_Position& position, bool needReply) {
        printf("Position from 0x%08" PRIx32 ": %.5f, %.5f alt %" PRId32 "\n",
               header.srcnode, position.latitude_i / 1e7, position.longitude_i / 1e7, position.altitude);
    });

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(60000));
        if (mesh.nodeinfo_db.needsSave()) {
            mesh.saveNodeDb();
        }
    }
}
