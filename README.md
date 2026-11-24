<p align="center">
  <img src="assets/palmwave-logo.png" alt="Palmwave logo, a palm tree and a wave forming the letter P" width="360">
</p>

<h1 align="center">Palmwave</h1>
<p align="center"><strong>From console code to a PC frame.</strong></p>
<p align="center">Xbox Series X|S titles on Windows x64 · Direct3D 12 · RDNA2 → DXIL</p>
<p align="center">
  <a href="https://realmoon.ai">Made with Moon AI</a> ·
  <a href="#status">Status</a> ·
  <a href="#getting-started">Getting started</a> ·
  <a href="https://github.com/fablinoo/palmwave/issues">Issues</a>
</p>

Palmwave is a Windows compatibility layer that runs an Xbox Series X|S title
natively, from files the user supplies. It is made with **Moon AI**, the AI
platform created by **Stellarr Studio**. The title's x86-64 code runs unchanged
inside our process; Palmwave supplies the platform underneath it and recompiles
the console's shaders to DXIL for Direct3D 12, ahead of time where it can and on
first use where it must. See [docs/architecture.md](docs/architecture.md).

**This repository contains no game files, code or assets.** You need your own
copy of the title.

## Status

<p align="center">
  <img alt="Stage: in game" src="https://img.shields.io/badge/stage-in%20game-1f883d?style=for-the-badge">
  <img alt="World: drawn" src="https://img.shields.io/badge/world-drawn-1f883d?style=for-the-badge">
  <img alt="Lighting: in progress" src="https://img.shields.io/badge/lighting-in%20progress-bf8700?style=for-the-badge">
  <img alt="Speed: 2.5 to 4 fps" src="https://img.shields.io/badge/speed-2.5%E2%80%934%20fps-bf8700?style=for-the-badge">
  <img alt="Playable: not yet" src="https://img.shields.io/badge/playable-not%20yet-6e7781?style=for-the-badge">
</p>

**Not playable yet: 2.5 to 4 frames per second, and shadowed areas are black.**
Developed and tested on one machine: Windows 11, NVIDIA RTX 5080 (driver
621.14), AMD Ryzen 9 9950X3D, 61 GB RAM. The Series X profile is used
throughout.

| Stage | State |
|-------|-------|
| Boot, menus, start screen | works, 30 fps |
| Opening cutscene | plays from start to finish, in order, lit: title cards, credits cards, night and dusk shots |
| Saves | the title autosaves a checkpoint and loads it again |
| First gameplay scene (the prologue) | renders: characters, textures, sky, sunlit exterior; HUD, pause menu and pad input work |
| Light in shadow | missing: the title's ray-traced bounce light and ray-traced lamp and sun shadows do not run yet, so anything the sun does not hit directly is black |
| Frame rate in the world | 2-4 fps: 420-450 ms per frame by default, 240-350 ms with the newer switches (`PALMWAVE_RELAY=2`, or `PALMWAVE_LATE_WAIT=3 PALMWAVE_WIDE_BACK=1 PALMWAVE_CARD_BUFFERS_MIN_MB=1 PALMWAVE_CARD_BUFFERS_MB=1024`); it started at 3-6 seconds per frame |
| Stability | no driver hang or device loss in normal runs (a hang introduced by the wave64 update was found and fixed: its barrier form for 64-lane programs is now off unless `PALMWAVE_WAVE_WIDE=1`) |

### What is done

Rendering:
- Meshlet culling read its scene buffers as the wrong kind of view, so the world drew almost nothing; fixed.
- Per-draw descriptor tables could run out, after which draws used slots never made for them and the card hung; fixed.
- Light packed by the title's compute shaders was read back through empty copies (sky, probes, fog); fixed for every mip level and layer.
- The fog reconstruction, shadow-mask, hair, blend-shape skinning, fire and several material programs did not translate; fixed.
- Gathers at a named mip level (with and without an offset) read level 0; fixed.
- Implicit-level samples outside pixel shaders produced invalid DXIL in 730 compute programs; fixed.
- Rectangle lists, depth and stencil twins for depth textures written by compute, texture read-backs.
- Ray tracing, in progress and off by default: the title's own structure-builder programs translate, title memory can be read by raw address from shaders, the hardware ray/box and ray/triangle test is emulated in DXIL (checked against a reference on the GPU), and 59 of the 64 trace programs translate. Nothing is lit by it yet. See [docs/fork-ray-tracing.md](docs/fork-ray-tracing.md).

Speed and memory:
- Waits for memory values, write-watch queries and page queries no longer walk page tables per call.
- Large title buffers get a copy in card memory, with shader writes found by hashing on the card.
- Submissions can be sent ahead of the wait for the card (`PALMWAVE_LATE_WAIT`), with a relay thread that lands results as the card finishes (`PALMWAVE_RELAY`): in a first five-minute run the card's idle time behind the submitting thread fell from about 136 to 30 ms per frame.
- Title memory can be opened as a graphics heap so that buffers are used in place instead of copied (`PALMWAVE_INPLACE`, first stage only; a probe confirmed the approach on the test GPU).
- The bindless texture queue no longer uploads stale views; the view table no longer grows without bound; the pipeline library is mapped instead of read into memory.
- An intermittent device loss was traced to the pipeline library file being replaced while mapped; fixed.
- Translated shaders get clean-up passes over the finished module (dead code, constant folding, reuse of values, branch facts); the pack's host instruction count falls by about 43 %, worth about 20 ms of GPU time per frame on the test machine (`PALMWAVE_SHADER_OPT=0` turns them off).

Most of the newer speed changes sit behind environment switches that are off
by default (`PALMWAVE_LATE_WAIT`, `PALMWAVE_RELAY`, `PALMWAVE_WIDE_BACK`,
`PALMWAVE_CARD_BUFFERS_MIN_MB`, `PALMWAVE_INPLACE`, `PALMWAVE_BVH_BUILDER`, ...).
Each is described in a comment where it is read.

### Known problems

- Everything in shadow is black (see above).
- The interior of the first gameplay room is sometimes not drawn after the cutscene hands over to gameplay.
- Around 24 GB of memory is in use during play.
- A few style lints are switched off for the newer code in the workspace `Cargo.toml` until it is tidied.
- Many changes here are specific to this title and this GPU, and some are marked `shortcut:` in the code where they approximate what the console does.

## What each part does

| Subsystem | Today | Needs |
|-----------|-------|-------|
| Loader | Runs the title's executable and its DLLs in our process; crash reports name the faulting code and are kept in `crashes.log` | – |
| Runtime services | One signed-in offline user, licences, packages, saves that survive a kill at any step, text entry dialog | Speech is silent; the dialog and relaunch are untested live |
| Network | Offline by design: the title cannot reach the network | – |
| Files and streaming | Archives and streamed data read, zlib and BCPack decoded in parallel | Loading is limited by writes into title memory |
| Input | XInput pads, keyboard, keyboard stand-in pad | Trigger rumble |
| Audio | Menu, video and loading sound; XMA voices and Opus streams decode | Reverb is silent; Opus results unverified against the title |
| GPU: menus | Draws, compute and presentation on host D3D12; only changed title memory is copied | Frame pacing drifts on one screen in release builds |
| GPU: world | Mesh pipelines, shader-written buffers, descriptor lifetimes, indirect execution (arguments clamped), pipelines made by worker threads and kept on disk | Most of the scene is still missing (untranslated shaders drop draws); frames take seconds; real queries, depth bounds, HDR and scaling are missing |
| Shaders | Console GPU code recompiled to DXIL at first use, cached on disk. Menus: all. World: mesh 8,191 of 8,506, vertex/pixel 10,617 of 12,346, compute 1,643 of 2,539 | 64-bit image atomics, merged geometry stages, the ray-tracing builder's shaders; some compute programs need a GPU with 64-lane waves |
| Textures | Console tiled layouts and detiling for the modes the title uses | Render-target metadata planes |
| Series S mode | Runs through the menus to the loading screen | The world, as above |

## Getting started

Requirements: Windows 10/11 x64, a CPU with AVX2, a Direct3D 12 GPU with
shader model 6.6 (and mesh shaders for the world), Rust (pinned in
`rust-toolchain.toml`).

The title also needs several GiB of available Windows commit at startup.
Check **Committed** under **Task Manager → Performance → Memory**. If
`crashes.log` reports Windows error 1455, close other applications or increase
the Windows paging file size, then retry.

```powershell
cargo xtask install-hooks                   # asset-policy check on every commit
copy palmwave.example.toml palmwave.toml    # then set your paths (outside the repo)
cargo test --workspace                      # needs no game data
cargo run -p palmwave -- config             # check your paths
```

Run the title:

```powershell
cargo build --release -p palmwave
target\release\palmwave.exe run <game dir>\<the title's exe> --execute
```

The window caption shows the framerate. Saves, settings and `crashes.log`
are kept under the data folder (default `%LOCALAPPDATA%\palmwave\data`), derived
data under the cache folder.

Input: XInput pads. With none connected the keyboard is a pad while the
title's window has focus:

| Keys | Pad |
|------|-----|
| W A S D / I J K L | left / right stick |
| arrows | D-pad |
| Enter, Backspace, X, Y | A, B, X, Y |
| Q / E, Z / C | shoulders, triggers |
| F / H | stick clicks |
| Esc, Tab | Menu, View |

## Commands

```
palmwave inspect <exe|dll> [--imports] [--exports]
palmwave census [paths...] [--missing] [--strict]
palmwave lib-exports <import.lib> [--list]
palmwave shader-info [paths...] [--verbose] [--disasm]
palmwave translate [paths...] [--validate] [--pipelines] [--list] [--jobs N]
palmwave run <exe> [--execute] [--unimplemented trap|zero] [--strict]
            [--console series-x|series-s] [-- <title arguments>]
palmwave config
```

Logging: `PALMWAVE_LOG=palmwave_loader=trace,info` (a release build logs
info and above).

## Debugging

A release build has no debugging aids. Build them in with

```powershell
cargo build --profile diag -p palmwave --features diagnostics   # target\diag\palmwave.exe
```

| Variable | Effect |
|----------|--------|
| `PALMWAVE_LOG=<target>=debug,...` | debug and trace output |
| `PALMWAVE_STACKS=20,60` | every thread's stack at those seconds |
| `PALMWAVE_WATCH=<rva>,...` | logs a title u32 when it changes |
| `PALMWAVE_KEYS=30:Enter,35:X` | presses keys at those seconds, focus or not |
| `PALMWAVE_CRASH_SAVE=<rva>:<len>,...` | saves title memory to the cache folder on a crash |
| `PALMWAVE_CAPTURE=<dir>` | saves presented frames (powers of two, or `PALMWAVE_CAPTURE_FRAMES=<list>`) |
| `PALMWAVE_DUMP_TARGETS=1` | with the two above, saves every render target of those frames |
| `PALMWAVE_D3D12_DEBUG=1` | host debug layer |
| `PALMWAVE_D3D12_GBV=1` | with the above, GPU-based validation (bad descriptor use reported, not faulted; slow) |
| `PALMWAVE_D3D12_DRED=1` | on a GPU failure, the allocation hit and the commands in flight |
| `PALMWAVE_PIPELINE_WAIT_MS=24` | reduce the per-frame wait for background pipeline creation from the default of 500 ms. A draw can be omitted until its pipeline is ready |
| `PALMWAVE_SKIP_PIPELINES=<hash>,...` | omit draws and dispatches, including indirect ones, whose pipeline contains a listed 16-digit shader hash |
| `PALMWAVE_GPU_TIMING=1` | with the above, each list of a submission run and timed on its own; slow lists logged with their slowest commands |
| `PALMWAVE_D3D12_WARP=1` | host device on the software adapter (slow; cannot reset the driver) |
| `PALMWAVE_SHADER_LOOP_LIMIT=<n>` | translated shader loops stop after `n` iterations per lane instead of 2^20 (own shader cache entries); if a lower `n` makes world frames much faster, loops hitting the limit are what makes them slow |
| `PALMWAVE_AUDIO_CAPTURE=<file.wav>` | records what the title plays (our process only) |
| `PALMWAVE_XMA_DUMP=<dir>` | saves the first 32 XMA voices' context and input |

Every build appends crashes and host GPU failures to `crashes.log` in the
title's data folder.

## License

Palmwave is distributed under **GPL-3.0-or-later**, Copyright (C) 2026
fablinoo. The Palmwave logo keeps the license under which it was previously
published. [License](LICENSE) · [Logo license](assets/LICENSE.palmwave-logo)

**AI used:** [realmoon.ai](https://realmoon.ai) (Moon AI)
