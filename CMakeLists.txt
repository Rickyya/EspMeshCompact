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

# Vendored crypto. Both of these are slated for replacement by
# espressif/libsodium; see CHANGELOG.md.
set(crypto_srcs
    "lib/Crypto/AES128.cpp"
    "lib/Crypto/AES192.cpp"
    "lib/Crypto/AES256.cpp"
    "lib/Crypto/AESCommon.cpp"
    "lib/Crypto/AESEsp32.cpp"
    "lib/Crypto/AuthenticatedCipher.cpp"
    "lib/Crypto/BLAKE2b.cpp"
    "lib/Crypto/BLAKE2s.cpp"
    "lib/Crypto/BigNumberUtil.cpp"
    "lib/Crypto/BlockCipher.cpp"
    "lib/Crypto/CTR.cpp"
    "lib/Crypto/ChaCha.cpp"
    "lib/Crypto/ChaChaPoly.cpp"
    "lib/Crypto/Cipher.cpp"
    "lib/Crypto/Crypto.cpp"
    "lib/Crypto/Curve25519.cpp"
    "lib/Crypto/EAX.cpp"
    "lib/Crypto/Ed25519.cpp"
    "lib/Crypto/GCM.cpp"
    "lib/Crypto/GF128.cpp"
    "lib/Crypto/GHASH.cpp"
    "lib/Crypto/HKDF.cpp"
    "lib/Crypto/Hash.cpp"
    "lib/Crypto/KeccakCore.cpp"
    "lib/Crypto/NoiseSource.cpp"
    "lib/Crypto/OMAC.cpp"
    "lib/Crypto/P521.cpp"
    "lib/Crypto/Poly1305.cpp"
    "lib/Crypto/RNG.cpp"
    "lib/Crypto/SHA224.cpp"
    "lib/Crypto/SHA256.cpp"
    "lib/Crypto/SHA3.cpp"
    "lib/Crypto/SHA384.cpp"
    "lib/Crypto/SHA512.cpp"
    "lib/Crypto/SHAKE.cpp"
    "lib/Crypto/XOF.cpp"
    "lib/Crypto/XTS.cpp"
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
# esp_timer.h; include/MtCompact.hpp pulls in RadioLib.h and mbedtls/aes.h.
# nvs_flash is used only by src/MtCompactFileIO.cpp, so it stays private.
# freertos/esp_hw_support/soc/hal/log are common requirements and are implicit.
#
# INCLUDE_DIRS is a public interface -- every dependent gets these on its
# include path. "." is required because the generated protobuf headers include
# each other as "meshtastic/<name>.pb.h". "lib/Crypto" and "lib/ed25519" are
# required because include/MtCompactHelpers.hpp includes "Curve25519.h" and
# src/McCompactStructs.cpp includes "ed_25519.h". Both drop out once the
# libsodium port lands.
idf_component_register(
    SRCS ${mesh_srcs} ${nanopb_srcs} ${meshtastic_srcs} ${crypto_srcs} ${ed25519_srcs}
    INCLUDE_DIRS "." "include" "lib/Crypto" "lib/ed25519"
    REQUIRES driver esp_timer mbedtls radiolib
    PRIV_REQUIRES nvs_flash
    )
