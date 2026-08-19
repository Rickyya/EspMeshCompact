# EspMeshCompact -- ESP-IDF component registration.
#
# Sources are listed explicitly rather than globbed: with file(GLOB) CMake does
# not re-run when a file is added, so a consumer who pulls a new revision would
# silently build without it. See the ESP-IDF build system guide, "File Globbing
# & Incremental Builds". This is not hypothetical here -- the ed25519 glob read
# "*.c0" rather than "*.c" for months and nobody noticed the library was absent.

set(mesh_srcs
    "src/CompactHelpers.cpp"
    "src/McCompact.cpp"
    "src/McCompactHelpers.cpp"
    "src/McCompactStructs.cpp"
    "src/McCompatChanMgr.cpp"
    "src/MtCompact.cpp"
    "src/MtCompactChanMgr.cpp"
    "src/MtCompactFileIO.cpp"
    "src/MtCompactHelpers.cpp"
    "src/MtMessageStore.cpp"
    "src/unishox2.cpp"
    )

# nanopb runtime.
set(nanopb_srcs
    "src/pb_common.c"
    "src/pb_decode.c"
    "src/pb_encode.c"
    )

# Generated Meshtastic protobuf definitions (nanopb 0.4.9.1).
set(meshtastic_srcs
    "meshtastic/admin.pb.cpp"
    "meshtastic/apponly.pb.cpp"
    "meshtastic/atak.pb.cpp"
    "meshtastic/cannedmessages.pb.cpp"
    "meshtastic/channel.pb.cpp"
    "meshtastic/clientonly.pb.cpp"
    "meshtastic/config.pb.cpp"
    "meshtastic/connection_status.pb.cpp"
    "meshtastic/device_ui.pb.cpp"
    "meshtastic/deviceonly.pb.cpp"
    "meshtastic/interdevice.pb.cpp"
    "meshtastic/localonly.pb.cpp"
    "meshtastic/mesh.pb.cpp"
    "meshtastic/module_config.pb.cpp"
    "meshtastic/mqtt.pb.cpp"
    "meshtastic/paxcount.pb.cpp"
    "meshtastic/portnums.pb.cpp"
    "meshtastic/powermon.pb.cpp"
    "meshtastic/remote_hardware.pb.cpp"
    "meshtastic/rtttl.pb.cpp"
    "meshtastic/storeforward.pb.cpp"
    "meshtastic/telemetry.pb.cpp"
    "meshtastic/xmodem.pb.cpp"
    )

# Vendored rweather/arduinolibs Crypto, pruned to what is actually reachable:
# Curve25519 for the Meshtastic PKI path, SHA256 for the shared-secret hash,
# and RNG for key generation. BigNumberUtil/Hash/Cipher/ChaCha/NoiseSource are
# their transitive dependencies. The AES classes went with src/aes-ccm.cpp.
set(crypto_srcs
    "lib/Crypto/BigNumberUtil.cpp"
    "lib/Crypto/ChaCha.cpp"
    "lib/Crypto/Cipher.cpp"
    "lib/Crypto/Crypto.cpp"
    "lib/Crypto/Curve25519.cpp"
    "lib/Crypto/Hash.cpp"
    "lib/Crypto/NoiseSource.cpp"
    "lib/Crypto/RNG.cpp"
    "lib/Crypto/SHA256.cpp"
    )

set(ed25519_srcs
    "lib/ed25519/add_scalar.c"
    "lib/ed25519/fe.c"
    "lib/ed25519/ge.c"
    "lib/ed25519/key_exchange.c"
    "lib/ed25519/keypair.c"
    "lib/ed25519/sc.c"
    "lib/ed25519/seed.c"
    "lib/ed25519/sha512.c"
    "lib/ed25519/sign.c"
    "lib/ed25519/verify.c"
    )

# REQUIRES vs PRIV_REQUIRES: anything reachable from a public header must be
# public. include/EspHal.h pulls in driver/gpio.h, driver/spi_master.h and
# esp_timer.h; include/MtCompact.hpp pulls in RadioLib.h, mbedtls/aes.h and
# mbedtls/ccm.h. nvs_flash is used only by src/MtCompactFileIO.cpp, so it stays
# private. freertos/esp_hw_support/soc/hal/log are common requirements and are
# implicit.
#
# INCLUDE_DIRS is a public interface -- every dependent gets these on its
# include path. "." is required because the generated protobuf headers include
# each other as "meshtastic/<name>.pb.h". "lib/Crypto" is required because
# include/MtCompactHelpers.hpp includes "Curve25519.h", "RNG.h" and "Crypto.h";
# "lib/ed25519" because src/McCompactStructs.cpp includes "ed_25519.h".
idf_component_register(
    SRCS ${mesh_srcs} ${nanopb_srcs} ${meshtastic_srcs} ${crypto_srcs} ${ed25519_srcs}
    INCLUDE_DIRS "." "include" "lib/Crypto" "lib/ed25519"
    REQUIRES driver esp_timer mbedtls radiolib
    PRIV_REQUIRES nvs_flash
    )
