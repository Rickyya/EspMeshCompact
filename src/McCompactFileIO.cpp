#include "McCompactFileIO.hpp"
#include <nvs_flash.h>
#include <nvs.h>
#include <vector>
#include "esp_log.h"

static const char* TAG = "McCompactFileIO";

static bool saveBlob(const char* key, const std::vector<uint8_t>& buffer) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("meshcore", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return false;
    }

    err = nvs_set_u32(handle, "fileio_ver", McCompactFileIO::FILEIO_VERSION);
    if (err != ESP_OK) {
        nvs_close(handle);
        return false;
    }

    err = nvs_set_blob(handle, key, buffer.data(), buffer.size());
    if (err != ESP_OK) {
        nvs_close(handle);
        return false;
    }

    err = nvs_commit(handle);
    nvs_close(handle);
    return err == ESP_OK;
}

static bool loadBlob(const char* key, std::vector<uint8_t>& buffer) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("meshcore", NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return false;
    }

    uint32_t fileio_ver = 0;
    err = nvs_get_u32(handle, "fileio_ver", &fileio_ver);
    if (err != ESP_OK || fileio_ver != McCompactFileIO::FILEIO_VERSION) {
        nvs_close(handle);
        ESP_LOGI(TAG, "NVS fileio_ver mismatch or not found.");
        return false;
    }

    size_t required_size = 0;
    err = nvs_get_blob(handle, key, nullptr, &required_size);
    if (err != ESP_OK || required_size == 0) {
        nvs_close(handle);
        return false;
    }

    buffer.resize(required_size);
    err = nvs_get_blob(handle, key, buffer.data(), &required_size);
    nvs_close(handle);
    return err == ESP_OK;
}

bool McCompactFileIO::saveNodeDb(NodeInfoCoreDB& db) {
    std::vector<uint8_t> buffer;
    if (!db.serialize(buffer)) return false;
    return saveBlob("nodedb", buffer);
}

bool McCompactFileIO::loadNodeDb(NodeInfoCoreDB& db) {
    std::vector<uint8_t> buffer;
    if (!loadBlob("nodedb", buffer)) return false;
    return db.deserialize(buffer);
}

bool McCompactFileIO::saveChannels(McCompatChanMgr& chan_mgr) {
    std::vector<uint8_t> buffer;
    if (!chan_mgr.serialize(buffer)) return false;
    return saveBlob("channels", buffer);
}

bool McCompactFileIO::loadChannels(McCompatChanMgr& chan_mgr) {
    std::vector<uint8_t> buffer;
    if (!loadBlob("channels", buffer)) return false;
    return chan_mgr.deserialize(buffer);
}

bool McCompactFileIO::savePrivateKey(MCC_MyNodeInfo& my_nodeinfo) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("mcpriv", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace 'mcpriv' for writing private key. Err: %d", err);
        return false;
    }

    err = nvs_set_blob(handle, "priv_key", my_nodeinfo.priv_key, sizeof(my_nodeinfo.priv_key));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write private key to NVS. Err: %d", err);
        nvs_close(handle);
        return false;
    }

    err = nvs_commit(handle);
    nvs_close(handle);
    return err == ESP_OK;
}

bool McCompactFileIO::loadPrivateKey(MCC_MyNodeInfo& my_nodeinfo) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("mcpriv", NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace 'mcpriv' for reading private key. Err: %d", err);
        return false;
    }

    uint8_t priv[sizeof(my_nodeinfo.priv_key)];
    size_t required_size = sizeof(priv);
    err = nvs_get_blob(handle, "priv_key", priv, &required_size);
    nvs_close(handle);
    if (err != ESP_OK || required_size != sizeof(priv)) {
        ESP_LOGE(TAG, "Failed to read private key from NVS. Err: %d", err);
        return false;
    }

    my_nodeinfo.setPrivateKey(priv);
    return true;
}
