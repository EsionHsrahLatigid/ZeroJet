# ZeroJet

ZeroJet is a YUP-based stereo dual-head fractional-delay through-zero-style flanger. Two moving delay heads crossfade smoothly around the dry path with signed feedback, color damping, stereo spread derived from Zero, dry/wet blend, and bounded output. It builds from this project directory, using the adjacent `../yup` checkout when present.

## Identity

- App ID: `jp.ehl.zerojet`
- Plugin ID: `jp.ehl.zerojet`
- AU subtype: `ZrJt`
- Vendor: `ehl_`; AU manufacturer: `EHL1`
- Version: `0.1.0`
- Type: stereo input/output effect, no MIDI
- macOS formats: Standalone, VST3, AUv2
- Windows formats: Standalone, VST3

## Parameters

- `Rate`: delay-head modulation speed.
- `Depth`: delay-head excursion.
- `Center`: center delay time.
- `Feedback`: signed feedback around the delay line.
- `Color`: feedback damping/opening.
- `Zero`: through-zero blend intensity and stereo phase spread.
- `Mix`: dry/wet blend.

## Research basis

The variable-delay core follows [Physical Audio Signal Processing: Fractional Delay Filters](https://www.dsprelated.com/freebooks/pasp/Fractional_Delay_Filters.html). The through-zero and moving-head survey included Aalto University's work on [virtual analog flanging](https://aaltodoc.aalto.fi/items/36c67cbf-02c5-40fa-b586-01724427baaa). ZeroJet uses two crossfaded delay heads, signed bounded feedback, loop damping, and a dry-path blend to create the perceived zero crossing without requiring a physical tape-machine model.

## Standalone Audition

Standalone builds compile a small audition source behind `YUP_AUDIO_PLUGIN_ENABLE_STANDALONE`. The audition enable and type controls are runtime/UI state only: they are not host parameters and are not serialized. VST3/AU builds compile the no-generator branch, keep the signal path strictly input -> effect -> output, and preserve hosted silence.

The standalone editor shows input/output meters and audition controls. If the YUP standalone macro is unavailable, the editor fails closed as a plain parameter grid with no audition path.

## Build

Clone with `--recurse-submodules`, or initialize the shared [yup-ehl-design-module](https://github.com/EsionHsrahLatigid/yup-ehl-design-module) before configuring:

```sh
git submodule update --init
```

```sh
cmake --preset engine-debug
cmake --build --preset engine-debug
ctest --preset engine-debug
```

```sh
cmake --preset plugin-release
cmake --build --preset plugin-release
ctest --preset plugin-release
```

Release bundles are staged under the stable `artifacts/plugin-release/<platform-arch>/` tree. `build/` is CMake's internal workspace:

- `zerojet_release_bundles`
- `zerojet_standalone_plugin`
- `zerojet_vst3_plugin`
- `zerojet_au_plugin` on Apple platforms

On macOS, the local bundle paths are:

- `artifacts/plugin-release/macos-arm64/standalone/zerojet_standalone_plugin.app`
- `artifacts/plugin-release/macos-arm64/vst3/zerojet_vst3_plugin.vst3`
- `artifacts/plugin-release/macos-arm64/au/zerojet_au_plugin.component`

Windows uses `artifacts/plugin-release/windows-x64/` with `standalone/` and `vst3/` directories.

## CI

`.github/workflows/ci.yml` is the required CI entrypoint for pushes to `main`, pull requests, and manual runs. A lightweight Linux classifier always runs. Changes limited to `README.md`, `DESIGN.md`, `LICENSE`, `docs/**`, or `.github/ISSUE_TEMPLATE/**` skip the heavy jobs; every other change runs Debug tests and Release bundle builds on macOS arm64 and Windows x64. Manual dispatches default to forcing both heavy jobs.

Successful heavy runs upload two immutable, 14-day artifacts:

- `ZeroJet-latest-macos-arm64`, containing `ZeroJet-latest-macos-arm64.zip` and `SHA256SUMS.txt`
- `ZeroJet-latest-windows-x64`, containing `ZeroJet-latest-windows-x64.zip` and `SHA256SUMS.txt`

`.github/workflows/release.yml` is the only `v*` tag workflow. It performs no compilation. The Ubuntu release job resolves lightweight or annotated tags to a commit, requires the tag version to match the CMake project version, requires one successful `CI` push run on `main` for that exact SHA, downloads exactly the two expected artifacts, verifies their SHA-256 manifests and ZIP integrity, then publishes versioned assets such as `ZeroJet-0.1.0-macos-arm64.zip` and `ZeroJet-0.1.0-windows-x64.zip`. Publication uses a draft release whose asset list is sanitized and rechecked to contain exactly those two ZIPs. A missing, expired, ambiguous, or mismatched provenance chain fails closed.

Release operator sequence: merge or push the version commit to `main`, wait for both platform jobs and `CI Summary` to pass, then create and push the version tag. GitHub CLI 2.x or newer is required by the release runner. Never move or reuse a published tag; correct the source and use the next patch version instead.

## Layout

- `include/zerojet/` contains the realtime-safe DSP engine API and local DSP primitives.
- `source/` contains the engine implementation and YUP plugin/editor/state wrapper.
- `tests/` contains deterministic engine regression tests plus hosted and standalone plugin-wrapper bridge tests.
- `cmake/` contains the project-local macOS icon conversion workaround used by the standalone target.
