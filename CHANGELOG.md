# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed

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

- `examples/mt_listener`, a Meshtastic receive demo. Until now no example linked
  `MtCompact`, so nothing ever compile-checked the Meshtastic half of the component.

### Removed

- `EspMeshCompact/CMakeLists.txt` and `EspMeshCompact/idf_component.yml`. Neither
  was reachable by the build.

### Fixed

- `idf_component.yml` now declares `license`, `targets`, `examples` and upload
  `files.exclude` rules. The repository previously carried three `idf_component.yml`
  files claiming two different versions (`0.8.0` and `0.0.3`); there is now one,
  and `library.json` agrees with it.

## [0.8.0]

- Prior releases are not documented in this file.
