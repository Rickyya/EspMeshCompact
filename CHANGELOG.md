# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- MeshCore (`McCompact`) can now transmit. `sendNodeInfo` and `sendGroupMsg` were
  empty function bodies, so the node could hear the mesh but nothing on the mesh
  ever learned it existed. It now sends signed adverts, group text messages and
  direct messages, and acknowledges what it receives.
- `McCompact::sendTextMessage` and the `OnTextMessage` callback for MeshCore
  direct messages, plus `OnAck` and `setAutoAck` for delivery confirmation.
  `OnAck` mirrors the shape of `MtCompact`'s `OnRouting`.
- MeshCore identity, contact and channel persistence via `McCompactFileIO`
  (NVS namespaces `meshcore` and `mcpriv`), mirroring `MtCompactFileIO`.
  Previously a fresh Ed25519 keypair was generated on every boot, so this node's
  address on the mesh changed on each power cycle and no peer could keep it as a
  contact.
- `McCompact::setClock` / `getCurrentTime`. MeshCore stamps adverts and messages
  with epoch seconds and uses them for replay detection; there was no time source.
- Duplicate-packet suppression (`McCompactSeenTable`), modelled on MeshCore's
  `SimpleMeshTables`. MeshCore floods, so every packet previously arrived and was
  reported once per route that reached the node.
- Optional repeater mode (`setRepeaterMode`, `setMaxFloodHops`), **off by default**,
  matching MeshCore's own `companion_radio` chat-node firmware. Adverts are only
  relayed once their signature verifies, and datagrams addressed to this node are
  consumed rather than relayed.
- `McCompact::getCurrentRSSI`, the instantaneous channel RSSI rather than the
  last packet's, matching MeshCore's `RadioLibWrapper::getCurrentRSSI`. A radio
  in receive reports a noise floor; one left in standby reports a constant, which
  is otherwise indistinguishable from a quiet band.
- `McCompact::getFrequencyError`, the carrier offset of the last received packet.
  LoRa tolerates about a quarter of the bandwidth, so an oscillator error between
  18 and 72 ppm at 869 MHz receives fine on a 250 kHz preset and is completely
  deaf on a 62.5 kHz one. Without this the two cases look identical.
- `McCompact::setRadioSyncWord` and `setRadioPreambleLength`, completing the
  runtime radio setters. The sync word is what separates MeshCore (0x12) from
  Meshtastic (0x2b) on an otherwise identical channel.

### Fixed

- **Every `setRadio*` method left the modem in standby**, so changing any radio
  parameter at runtime silently ended reception until the next reboot. RadioLib
  drops to standby to reconfigure and does not return to receive on its own;
  each setter now does.
- `startReceive()`'s return value was discarded at all three call sites. A
  failure to enter receive produced a node that heard nothing, logged nothing,
  and was indistinguishable from a band with no traffic on it. The listen task
  now reports it.
- `examples/mc_receiver` advertised a preamble of 16 symbols at SF 8. MeshCore's
  `RadioLibWrapper::preambleLengthForSF` gives 32 at SF 8 and below, applied by
  `setParams()` at boot over the 16 its `begin()` is handed.
- **MeshCore direct messages never worked.** The receive path used each contact's
  raw public key as the decryption key instead of the ECDH shared secret, so the
  MAC never matched and every `TXT_MSG`, `REQ`, `RESPONSE` and `PATH` was silently
  discarded. Now uses `MCC_MyNodeInfo::calcSharedSecret`, which had been compiled
  but unreferenced since it was written. The destination hash is also honoured
  rather than parsed and ignored, and the shared secret is cached per contact.
- **Remotely reachable buffer overflow.** `McCompact::decrypt` wrote
  `ceil(src_len/16)*16` bytes with no knowledge of the destination size, while the
  radio task accepts 255-byte packets and every destination is a 184-byte stack
  array. Reaching it required only a valid 2-byte MAC, which is trivial for a
  channel whose key is published, as MeshCore's public channel key is. `decrypt`
  and `MACThenDecrypt` now take a destination length.
- **MeshCore adverts were never authenticated.** The 64-byte Ed25519 signature was
  skipped, so any node could claim any identity, name and location. Adverts are now
  verified and stale timestamps rejected; `setVerifyAdverts(false)` opts out for
  protocol research.
- `encryptThenMAC` passed `dest` to `mbedtls_md_hmac`, which always writes a full
  32-byte digest, so each call overwrote the first 30 bytes of the ciphertext it had
  just produced. It also returned a status code rather than a length. Latent because
  its only two callers were the empty send stubs.
- `MCC_Header::generate_header` emitted path hops using the `path_size` member
  (always 1) rather than the `path_bytenum` argument, truncating any 2- or 3-byte
  path while declaring it at full width. `parse` declared a local `path_size` that
  shadowed the member, so the member never reflected the parsed value.
- `sendNeighborDiscoveryRequest` wrote its tag big-endian while both our parser and
  MeshCore read it little-endian.
- The MeshCore `TXT_MSG` handler computed its body length from the raw packet length
  against an offset into the decrypted buffer, reading past the plaintext.
- `McCompactHelpers::NodeInfoBuilder` takes micro-degrees, but `examples/mc_receiver`
  passed degrees, so `47.4979` became 47 micro-degrees. Harmless while the node could
  not advertise; not harmless now. `setMyLocation` also cast through `uint32_t`,
  the wrong signedness for southern and western coordinates.

### Changed

- `OnGroupMsg` now carries the timestamp and the sender name split from MeshCore's
  `"<sender>: <message>"` body, and unsupported `txt_type` values are dropped rather
  than reported as corrupt-looking chat messages. This is a breaking change to the
  callback signature.
- `McCompact::decrypt` and `McCompact::MACThenDecrypt` take a destination-length
  argument (see the overflow fix above). Breaking change if called directly.

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
  Off at runtime and, more importantly, compiled out unless
  `CONFIG_MTCOMPACT_TEXT_COMPRESSION` is set. Referencing the compressor at all
  links `unishox2_compress_lines`, about 4.2 kB of flash, which a runtime flag
  cannot avoid. Decompression is always built, so incoming compressed text is
  understood regardless.
- A `Kconfig` menu for the component, currently holding that one option.
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
