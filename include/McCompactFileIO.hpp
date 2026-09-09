#pragma once

#include "McCompactNodeInfoDB.hpp"
#include "McCompatChanMgr.hpp"

class McCompactFileIO {
   public:
    static const uint8_t FILEIO_VERSION = 1;

    static bool saveNodeDb(NodeInfoCoreDB& db);
    static bool loadNodeDb(NodeInfoCoreDB& db);

    static bool saveChannels(McCompatChanMgr& chan_mgr);
    static bool loadChannels(McCompatChanMgr& chan_mgr);

    // Stores the 64-byte Ed25519 private key; the public key is re-derived on load.
    static bool savePrivateKey(MCC_MyNodeInfo& my_nodeinfo);
    static bool loadPrivateKey(MCC_MyNodeInfo& my_nodeinfo);
};
