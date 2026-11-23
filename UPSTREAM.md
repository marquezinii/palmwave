# Source provenance

Palmwave is published with an independent Git history. Its initial source snapshot is derived from [ItsNotRexed/Leonida](https://github.com/ItsNotRexed/Leonida) at commit [`dde82f27e994337094afc476ded0b1426f6e87e2`](https://github.com/ItsNotRexed/Leonida/commit/dde82f27e994337094afc476ded0b1426f6e87e2), under GPL-3.0.

Original source copyright headers, LICENSE, technical documentation and bundled third-party notices are retained. The initial import does not claim original authorship of the inherited implementation. Historical measurements in the documentation are not new Palmwave test results.

The `third_party/rpf9-explorer` submodule remains pinned to `e2939175c36a02d6ed25fa2c217b9d6ddcb8b516` from [breadbbbb/rpf9-explorer](https://github.com/breadbbbb/rpf9-explorer).

The initial Palmwave changes are branding, project documentation and a separate local measurement record. No runtime code from XCL, Five Star, OpenScarlett or xbox-emulator has been imported in this initial commit. Future imports must record their source revision, license, authorship, rationale and tests here or in a component notice.

Public implementation notes from other projects can help identify a question to test. They do not substitute for a local trace. Do not copy GPL-2.0-only code into this GPL-3.0 project. Preserve MIT and other applicable notices when incorporating compatible components.

## XErrorReport export

The flat `XErrorReport` export and its synthetic runtime/loader tests are adapted from dracbat's implementation, rebased by Kam471 in [Leonida PR #6582](https://github.com/ItsNotRexed/Leonida/pull/6582), commit [`4224c53b145ea9e68eb579817d772fe7276b943a`](https://github.com/Kam471/Leonida/commit/4224c53b145ea9e68eb579817d772fe7276b943a), under GPL-3.0. A local offline boot stopped when `GetProcAddress` could not resolve this name. The export delegates to the existing offline `IXErrorImpl` report handler; `xgameruntime` and `run_tiny_gdk_load` cover lookup and callback delivery, with an additional message/context check in Palmwave.

## Executable TLS index zero

The early TLS-index-zero reservation, host-template clone and synthetic `/GA` loader tests are adapted from mariuslozo-boop's [Leonida PR #4313](https://github.com/ItsNotRexed/Leonida/pull/4313), final revision [`a9085b661c3637ce4d154dab3c0d5b4a436d90ec`](https://github.com/mariuslozo-boop/Leonida/commit/a9085b661c3637ce4d154dab3c0d5b4a436d90ec), under GPL-3.0, including Roberto0953's rebase and existing-thread correction. The upstream commits credit Claude Opus 5.5 as co-author. Local offline debugging established direct slot-zero access while the loader had assigned the executable another TLS index. The runtime now reserves zero before its C++ initializers and keeps its own TLS at a separate, OS-backed home index; the imported tests cover host/guest isolation, existing and new threads, reservation refusal, repeated loads and temporary-clone cleanup. Their local execution results are recorded separately in `STATUS.md`.

## DirectStorage factory

`runtime/shims/dstorage.cpp`, `runtime/shims/dstorage_xs_abi.h` and `runtime/test/dstorage_test.cpp` are adapted from mariuslozo-boop's [Leonida PR #4348](https://github.com/ItsNotRexed/Leonida/pull/4348), final revision `45c39cf6f4148aebbc8a88d476bea755887d35bd`, under GPL-3.0. The PR was closed unmerged, and its author permitted it to be taken over. The upstream commits credit Claude Opus 5.5 as co-author.

A local offline boot stopped at `xcrdapi.dll!DStorageGetFactory`, with factory id `62959d11-d45f-4884-a0a3-f7862ea243ae`. The PR's CPU file and memory queues, status arrays, fences, events, cancellation and error records are kept as written.

Palmwave changed the following:
- The PR learned unknown interface ids at run time and accepted the PC `IDStorageStatusArray` id. Both are replaced by a strict table that holds the measured factory and file ids for their respective objects.
- `OpenFile` no longer falls back to the host `CreateFileW` when the file-system shim is absent.
- `xcrdapi.dll` is registered as the primary name.
- Tests were added for raw factory slots 8 and 10, the named file IID and raw `GetHandle` slot 4, strict-id rejection and unsupported-request sentinels. The file test checks caller-owned handle lifetime across explicit close and final release.

FiveStar (GPL-3.0-or-later) and OpenScarlett (MIT) were consulted for ABI facts only, through source review; no code was copied from either. Microsoft's public Xbox documentation confirms `GetHandle`, whose signature differs from the siblings' `GetFileInformation` assumptions. The base adaptation was written with Claude Code (Claude Opus 5.5); OpenAI Codex added the measured file IID, handle-ownership tests and documentation correction. Test results are recorded separately. See [docs/directstorage.md](docs/directstorage.md).

## DirectStorage zlib requests (candidate)

The zlib request path in `runtime/shims/dstorage.cpp`, the `ZlibError` failure reasons in `runtime/src/shader_library.h`, the four decompression error constants in `runtime/shims/dstorage_xs_abi.h` and the zlib cases in `runtime/test/dstorage_test.cpp` and `runtime/test/shader_library_test.cpp` are adapted from KkelebekK's [Leonida PR #7180](https://github.com/ItsNotRexed/Leonida/pull/7180), head `00551ba0099699f35893ecd58a9ec34a2b54e95c` (its zlib-only commit on top of PR #4348), under GPL-3.0. The PR was open and unmerged when it was adapted. Its physical-page follow-up, PR #7652, was not used.

Only the zlib delta was applied, onto Palmwave's strict interface-id shim; the measured factory and file ids, `GetHandle` and the `GAME:`/overlay file opens are unchanged. Like the PR, a request inflates from a staging copy of its source and writes the destination only when the stream verifies at exactly `DestinationSize` bytes. BCPack, swizzle and physical pages, alone or with zlib, still fail with `E_NOTIMPL`. The PR records that the meaning of `E_DSTORAGE_ZLIB_BAD_HEADER`/`_BAD_DATA`/`_PARITY_FAIL` is inferred from their names, and that remains unverified. Palmwave added tests for zlib with physical pages, an empty source, a bad stored length, a truncated Adler-32, in-place inflation from the start of the destination, canaries past every destination, an over-limit and a null destination, error records and events, cancelled zlib requests and fence ordering, plus inflater limit checks.

This is a CPU-only candidate prepared ahead of a measured title DirectStorage zlib request. It is not integrated with a boot and makes no boot, frame or GPU claim. It was written with Claude Code (the CLI reports `claude-opus-5-5`, Claude Opus 5.5), and coordinated and reviewed with OpenAI Codex. Local MSVC 19.51 validation passed both focused CTests (`dstorage`, `shader_library`) in 0.44 seconds; DirectStorage reported 552 checks and zero failures. These are CPU tests, with no GPU execution.

## Device slot 26 struct return

The `GetCustomHeapProperties` ABI adapter and synthetic proxy regression are adapted from sennecools's [Leonida PR #6964](https://github.com/ItsNotRexed/Leonida/pull/6964), revision `ed70b4c92002ce76934646c384dc661e1d02c491`, under GPL-3.0. A local offline debugger confirmed the return buffer in RCX and the device proxy in RDX, matching the inherited boot-chain notes. The adapter passes the request to the real device, copies its complete answer into the supplied buffer and returns that buffer's address; the PC calling convention remains supported.

Palmwave strengthens the synthetic regression with a native C++ call through `ID3D12Device::GetCustomHeapProperties` for the PC convention. The console call checks truncation of the two 32-bit arguments, untouched surrounding bytes, one backend call and preserved proxy identity and reference counts. With the old forwarder this regression raised `0xC0000005`; after the fix all six `xs_proxy_mock` CTests passed in 0.20 seconds with MSVC 19.51 on CPU only. No GPU or game rendering validation is implied. The adaptation and local validation were assisted by OpenAI Codex.

## Project identity and tools

Palmwave is developed under [Moon AI](https://realmoon.ai), the AI platform created by Stellarr Studio. The maintainer account is [fablinoo](https://github.com/fablinoo). The initial setup was AI-assisted with OpenAI Codex and OpenAI image generation; Claude Code was assigned a read-only source review. This records the actual tool provenance.
