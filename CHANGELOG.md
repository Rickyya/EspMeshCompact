# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed

- Replaced the vendored `src/aes-ccm.cpp` (a copy of hostap's AES-CCM built on the
  rweather `AESSmall256` software cipher) with mbedtls CCM. mbedtls was already a
  dependency, and on the ESP32-S3 it routes through the AES accelerator.
  The wire format is unchanged and was verified byte-for-byte: the old
  implementation was checked against an independent AES-256-CCM reference
  (iv_len 13, tag_len 8) and the new call sequence reproduces the previous
  ciphertext and tag exactly on all test vectors, including the empty-plaintext
  and exact-block-multiple cases.
- Pruned `lib/Crypto/` from 37 to 9 source files. Only Curve25519 (Meshtastic PKI),
  SHA256 (shared-secret hash) and RNG (key generation) are reachable, plus their
  transitive dependencies. This is a maintenance change only -- the linker was
  already discarding the unused objects, and both examples build to byte-identical
  images before and after.
- Restructured the repository into a valid ESP-IDF component. The component
  sources moved from `EspMeshCompact/` to the repository root, and the demo
  application moved from `main/` to `examples/mc_receiver/`, which now depends
  on the component via `override_path` instead of recompiling its sources.

  Previously the root `CMakeLists.txt` declared an IDF *project* while the root
  `idf_component.yml` declared a *component*, so packing the repository shipped
  the demo app and the project file inside the component. `EspMeshCompact/CMakeLists.txt`
  was never used at all -- nothing set `EXTRA_COMPONENT_DIRS`, so IDF never
  discovered the directory as a component and `main/CMakeLists.txt` rebuilt every
  source itself through `../EspMeshCompact/*` globs.
- Sources are listed explicitly in `CMakeLists.txt` instead of being collected
  with `file(GLOB_RECURSE)`, so adding a file no longer requires a manual
  `idf.py reconfigure` to be picked up. The previous globbing had already caused
  a real defect: the ed25519 pattern read `*.c0` rather than `*.c`, so that
  library silently was not built.
- `nvs_flash` moved from `REQUIRES` to `PRIV_REQUIRES`; it is only used by
  `src/MtCompactFileIO.cpp`.
- The `RADIOLIB_EXCLUDE_*` compile definitions documented in the README are now
  also applied by the example projects.

### Added

- `setOnRouting()`, delivering ACK/NAK for packets this node sent. `MCT_Routing`
  reports ACK, NAK, ROUTE_REQUEST or ROUTE_REPLY plus the `request_id` of the
  packet being reported on.
- `WaypointDB`, exposed as `MtCompact::waypoint_db`. Received waypoints are now
  stored, keyed by waypoint id, instead of only firing a callback.
- Native callbacks for the telemetry variants that were previously dropped: air
  quality, power, local stats, health and host metrics.
- Native callbacks for `ADMIN_APP` and `STORE_FORWARD_APP`. This component still
  implements neither; it just stops throwing the decoded message away.
- `saveChannels()` / `loadChannels()`. The channel list now survives a reboot,
  as the node database and private key already did.
- `setTextCompression()`, opt-in Unishox2 compression for outgoing plain text.
  Off by default.
- A GitHub Actions workflow building both examples for esp32s3 on IDF v5.5.5 and
  checking that the packed component contains no app or build output.

- `examples/mt_listener`, a Meshtastic receive demo. Until now no example linked
  `MtCompact`, so nothing ever compile-checked the Meshtastic half of the component.

### Removed

- `EspMeshCompact/CMakeLists.txt` and `EspMeshCompact/idf_component.yml`. Neither
  was reachable by the build.

### Fixed

- ACK transmit, dead since 655b80b, works again. It had been sending
  `request_id` taken from the received payload -- 0 for an ordinary message --
  so the originator could never match an ACK to its pending packet. It now
  references the acked packet's id.
- The PKI transmit path bounds-checks the CCM overhead against the output
  buffer and honours `encryptCurve25519()`'s return value; previously a failed
  key agreement transmitted an uninitialised buffer.
- `decryptCurve25519()` rejects packets shorter than the PKI overhead instead of
  underflowing `size_t`.
- The "unsupported radio type" fallback no longer leaks `radio` and `hal`, and
  the SX1262 substitution it advertises now actually takes effect.
- `McCompact::encryptThenMAC` keyed its HMAC with 32 bytes while
  `MACThenDecrypt` verifies with 16, so nothing it produced could be verified.
- A decrypted MeshCore payload was passed to `ESP_LOGI("%s")` without being
  NUL-terminated, reading past the end of the buffer.
- Three call sites used libc `random()` with no `srandom()`, returning the same
  sequence on every boot. One seeded entropy for Curve25519 key generation.

- `idf_component.yml` now declares `license`, `targets`, `examples` and upload
  `files.exclude` rules. The repository previously carried three `idf_component.yml`
  files claiming two different versions (`0.8.0` and `0.0.3`); there is now one,
  and `library.json` agrees with it.

## [0.8.0]

- Prior releases are not documented in this file.
