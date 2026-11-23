# DirectStorage (`xcrdapi.dll!DStorageGetFactory`)

The retail executable imports one DirectStorage function, `XCRDAPI.dll!DStorageGetFactory`
([runtime_imports.md](runtime_imports.md#the-retail-executable-rt-5)). Its single call site passes the address of an
interface id and of a global, then tests the global for null ([audio_hal_exe_calls.md](audio_hal_exe_calls.md)).
The local offline boot stopped at this import. A null factory leads to a fatal assert in the title
([retail_boot_chain.md](retail_boot_chain.md)). `runtime/shims/dstorage.cpp` serves it. The shim is adapted from
upstream PR #4348 ([UPSTREAM.md](../UPSTREAM.md#directstorage-factory)).

## What is measured, sourced or assumed

| Fact | Status | Where it comes from |
|---|---|---|
| Module `XCRDAPI.dll` and the shape `HRESULT DStorageGetFactory(REFIID, void**)` | measured | the executable's import table and call site |
| Factory id `{62959d11-d45f-4884-a0a3-f7862ea243ae}` | measured | the local offline boot (the id the title passes). The public OpenScarlett header (MIT) names it `IDStorageFactoryX2` |
| Factory slot 8 `SetStagingBufferSize(UINT32)` and slot 10 `CreateQueue1(desc1, REFIID, void**)` | sourced, reviewed | the PR's declaration order. The native review matched it against FiveStar (GPL-3.0-or-later) and OpenScarlett. `dstorage_test` pins both slots through the raw vtable |
| Factory slots 3-7 and 9 | PR order, unverified | PC `dstorage.h` order plus the GDK overview. FiveStar agrees on 4 (`OpenFile`) and 5 (`CreateStatusArray`) |
| File id `{43704f9f-27c6-4a9f-8815-d16527894df4}` | measured | the local `OpenFile` request; the public OpenScarlett header names it `IDStorageFileX_Xbox` |
| Queue and status-array interface ids | **not measured** | none is accepted yet; see below |
| File API | primary documentation; ordinal sourced | Microsoft's Xbox `IDStorageFileX` exposes `Close()` and `HANDLE GetHandle()`. The PR places these at slots 3 and 4; the synthetic test pins this table, without claiming a local retail slot measurement |
| Queue inheritance | primary documentation; ordinals sourced | Microsoft's `IDStorageQueueX` includes `EnqueueSignal`; X1 adds `EnqueueSetEvent`, and X2 adds `Query1`. This matches the PR's inheritance, not OpenScarlett's swapped signal/event methods or additional slots 14-15 |
| Struct layouts, `E_DSTORAGE_*` values, capacity bounds | sourced | the GDK reference and the PC `dstorage.h`/`dstorageerr.h` 1.3.0, pinned with `static_assert` in `dstorage_xs_abi.h` |
| `E_DSTORAGE_DECOMPRESSION_ERROR` `0x89240030` | sourced | Microsoft's public DirectStorage constants page ("a generic error during decompression") |
| `E_DSTORAGE_ZLIB_BAD_HEADER` / `_BAD_DATA` / `_PARITY_FAIL` `0x89240031`-`33` | values sourced; **meaning inferred** | names and values from the header-generated bindings of the `direct-storage` crate 0.5.1 (as recorded by PR #7180); no public description. Header / deflate data / Adler-32 is a reading of the names, not a measurement |
| zlib `DestinationSize` larger than `SourceSize`; zlib `FileOffset` 16-byte aligned | sourced | `E_DSTORAGE_INVALID_DESTINATION_SIZE`'s description and `DSTORAGE_REQUEST` (as recorded by PR #7180) |

## Interface ids: strict

The creating calls (`DStorageGetFactory`, `CreateQueue`, `CreateQueue1`, `OpenFile`, `CreateStatusArray`) and every
`QueryInterface` apply the same rule. They accept `IUnknown`, or an id that `kKnownInterfaceIds` in `dstorage_xs_abi.h`
lists for that kind of object. Today that list has the factory and file ids above. Any other id gets `E_NOINTERFACE`,
with `*ppv` cleared and nothing created or opened. Refused ids include unknown ones, another kind's id, system COM
ids, and every PC DirectStorage id, including `IDStorageStatusArray`, whose shape happens to match. Each refusal is
logged once per caller as `[rt] dstorage: <caller> asked for {id}: <reason> (E_NOINTERFACE)`. Nothing is learned at
run time, so the same id is refused again. The PR's policy of accepting and remembering any new id was removed.

An unrecognized queue or status-array id still returns `E_NOINTERFACE` and is logged for the next investigation.
Published candidate ids do not establish their whole method table. A new supported id needs evidence for its API
and tests of the corresponding table; the next local call determines which interface needs attention.

## File API and handle ownership

The public Xbox reference specifies [`IDStorageFileX::GetHandle()`][file-handle], returning a readable Win32 handle
with no output-buffer parameter. The caller owns and closes that handle. The PC API instead has
[`IDStorageFile::GetFileInformation(BY_HANDLE_FILE_INFORMATION*)`][pc-file-info]. FiveStar explicitly marks its
corresponding method unobserved, and OpenScarlett substitutes its own eight-byte `DSTORAGE_FILE_INFO`. Neither
substitution establishes an Xbox output structure, so this shim retains the documented `GetHandle` signature.

[`IDStorageFileX::Close()`][file-close] closes the file regardless of COM reference count, waits for outstanding I/O,
and prevents later DirectStorage requests. Final `Release` also closes it. Here, `ReOpenFile` gives each `GetHandle`
caller an independently owned handle and file position; those handles remain valid after the object closes or is
released. The existing pending-request accounting and close behavior are unchanged.

[file-handle]: https://learn.microsoft.com/en-us/gaming/gdk/docs/reference/system/dstorage/interfaces/idstoragefilex/methods/idstoragefilex_gethandle?view=gdk-2510
[file-close]: https://learn.microsoft.com/en-us/gaming/gdk/docs/reference/system/dstorage/interfaces/idstoragefilex/methods/idstoragefilex_close?view=gdk-2510
[pc-file-info]: https://learn.microsoft.com/en-us/windows/win32/dstorage/dstorage/nf-dstorage-idstoragefile-getfileinformation

## What the shim does

These parts are unchanged from the PR except where noted.

- **Factory:** one object per process that is never freed. The factory itself does not change between aliases:
  `xcrdapi.dll`, `dstorage_xs.dll` and `dstorage.dll` all resolve to the same `DStorageGetFactory`. Each call
  behaves like this:
  - `SetStagingBufferSize` accepts 0/16/20/24/28/32 MiB and is refused while a queue or file is live. It stores
    the size only. No staging buffer is allocated, because these direct reads do not use one.
  - `SetCpuAffinity` and `SetDebugFlags` store their values only.
  - `GetStats` reports the counters the shim keeps.
- **Files:** `OpenFile` calls the registry's `kernel32.dll!CreateFileW`, which is `file_system.cpp`. A `GAME:` path
  therefore gets the same mapping, overlay and `..`/drive sandbox as the title's own opens. A host path goes to the
  host, as the title's own `CreateFileW` does. **Palmwave change:** the PR fell back to the host `CreateFileW` when
  that shim was missing. This shim instead returns `E_NOTIMPL` and opens nothing. `GetHandle` returns a separate
  read-only handle from `ReOpenFile`. `Close` waits for the requests already submitted against the file.
- **Queues:** capacity must be `0x80`..`0x2000`, and a memory queue must be `REALTIME`. Auto-submit fires at half
  capacity unless it is disabled. An enqueue into a full queue waits. One CPU worker thread per queue runs the
  submitted entries in order:
  - A file request is a positioned `ReadFile` of exactly `SourceSize` bytes.
  - A memory request is a guarded `memmove`.
  - A status entry, `ID3D12Fence::Signal` (PC slot 10, which the `d3d12_xs` fence proxy forwards) and
    `EnqueueSetEvent` each complete only after every entry before them.
  - `CancelRequestsWithTag` marks requests that have not started. A cancelled request counts as a success with
    nothing read.
  - Failures are recorded in the status entry, the error record and the error event.
- **zlib requests** (`ZlibDecompress` alone; adapted from upstream PR #7180, see below). Not yet requested by the
  title in a local run, so this is prepared ahead of a measured DirectStorage zlib request, not a boot fix:
  - Checks, after the reserved-bit, source-type and mode checks: `IntermediateSize` must be 0 (it only exists for
    zlib + BCPack); `DestinationSize` must be larger than `SourceSize` (`_INVALID_DESTINATION_SIZE`); the unpaged
    32 MiB destination limit and the 1 GiB request limit (`_REQUEST_TOO_LARGE`); a file `FileOffset` that is not
    16-byte aligned (`_INVALID_FILE_OFFSET`); then the null-pointer and closed-file checks every request has.
  - The worker copies `SourceSize` bytes of the file or memory into a staging buffer and inflates them into a second
    one with the runtime's RFC 1950 inflater (`palmwave::zlib_inflate`, `runtime/src/shader_library.h`, the one the
    shader library uses). The inflater never grows its output past `DestinationSize`. Only a stream whose header,
    deflate data and Adler-32 verify and whose result is exactly `DestinationSize` bytes is copied to `Destination`.
    A failed stream therefore leaves the destination exactly as it was (no zero fill), and a memory source inside its
    own destination (in-place loading) is safe. Both staging buffers are at most 32 MiB; an allocation failure is
    `E_OUTOFMEMORY`. Bytes after the Adler-32 are not read (an assumption: the reference does not say).
  - Failures: a bad header (not deflate, a window over 32 KiB, a bad check value, a preset dictionary)
    `E_DSTORAGE_ZLIB_BAD_HEADER`; bad or truncated deflate data or a missing Adler-32 `_ZLIB_BAD_DATA`; an Adler-32
    mismatch `_ZLIB_PARITY_FAIL`; more or fewer bytes than `DestinationSize` `E_DSTORAGE_DECOMPRESSION_ERROR` (no
    code names a size mismatch). An unreadable source or unwritable destination stays `E_DSTORAGE_ACCESS_VIOLATION`
    and a short file `E_DSTORAGE_END_OF_FILE`. They go through the same status entry, error record and error event.
  - A cancelled zlib request is treated like any cancelled request: it completes as a success, reads and writes
    nothing and is not counted as a failure or a decompression.
  - `GetStats`: `TotalDecompressionRequestCount` counts the requests that reached the inflate (checks passed, source
    read), `PendingDecompressionRequestCount` the inflates running, `DecompressionErrors` the streams that failed.
  - The inflater decodes one bit at a time on the queue's worker: correct, not fast.
- **Unsupported requests fail honestly:**
  - BCPack (modes 1-7) and swizzle (modes 1-4), alone or with zlib, return `E_NOTIMPL`.
  - Physical source or destination pages, alone or with zlib, return `E_NOTIMPL`.
  - An unpaged buffer over 32 MiB returns `E_DSTORAGE_REQUEST_TOO_LARGE`.
  - Each of these checks runs before the destination is touched, so such a request never completes as a success
    with empty or stale data.
  - Invalid modes, reserved bits and size mismatches return the matching `E_DSTORAGE_*` error.

Nothing here goes online, sends telemetry or touches the GPU. Logging goes to stderr only.

## Tests

`runtime/test/dstorage_test.cpp` (ctest `dstorage`) uses synthetic files in `%TEMP%`, with no game data and no GPU:

- **Inherited from the PR:** layout and option bits, `GAME:` and overlay reads, offsets, EOF, cancellation,
  auto-submit and a full queue, status arrays, fence ordering, events, error records, file and queue close with
  requests in flight, an unwritable destination, four concurrent producers, memory queues, and statistics.
- **Added for Palmwave:**
  - Raw-vtable calls to slot 8 and slot 10 on the factory that `xcrdapi.dll` returns for the retail id. The test
    checks size validation and the lock, and that the queue slot 10 creates really reads file bytes.
  - The named file IID and raw slots 0/2/3/4: interface identity, rejected-IID output clearing, readable handles,
    independent file positions, and caller-owned handles surviving explicit `Close` and final `Release`.
  - Strict-id rejection: each id is asked twice, every refused output is cleared, and a refused id creates no queue
    and opens no file.
  - Unsupported requests leave a sentinel destination unchanged and are recorded as failures, including zlib with
    BCPack, swizzle or physical source/destination pages, which also never reach the inflate.
- **zlib (`test_zlib`, adapted from PR #7180 and extended):** synthetic streams only: a dynamic-Huffman and a
  fixed-Huffman stream made with Python's `zlib` from text the test rebuilds, and stored-block streams the test
  builds (one stored block followed by the dynamic blocks). File and memory sources, in place with the source at
  the end and at the start of its destination, padding after the trailer; each header, data, Adler-32 and size
  failure, an empty source, a bad stored length, an unaligned offset, `IntermediateSize`, a source past the end of
  the file, an unreadable source, an unwritable, null or over-32 MiB destination. Buffered destinations have canaries
  past `DestinationSize`; malformed-stream and request-validation failures must leave the buffer unchanged. The error records, error events and
  decompression statistics are checked, as are two cancelled zlib requests (success, nothing written, no inflate)
  and a fence that must see the inflated bytes in place. `shader_library_test` checks the inflater's failure reasons
  and that its output never passes the limit.

`unimplemented_policy_test` checks that `xcrdapi.dll` / `XCRDAPI.dll!DStorageGetFactory` is classified as returning
an HRESULT, and that other `xcrdapi.dll` names are not.

## Not done

- The queue and status interface ids and local verification of sourced method ordinals, as described above.
- A measured title zlib request: the zlib path has synthetic tests only, and which faults the console reports with
  each `E_DSTORAGE_ZLIB_*` code is inferred.
- BCPack (`tools/bcpack.py` is the reference decoder), then swizzle, as stages after the inflate and before the
  destination write.
- Physical pages (this needs `xmem` page-number translation).
- Routing writes into Vulkan-backed GPU memory.
- Parallel reads.

## Sources and licences

| Source | Use |
|---|---|
| [upstream PR #4348](https://github.com/ItsNotRexed/Leonida/pull/4348), mariuslozo-boop, commit `45c39cf6f4148aebbc8a88d476bea755887d35bd`, GPL-3.0 | Code adapted (`dstorage.cpp`, `dstorage_xs_abi.h`, `dstorage_test.cpp`) |
| [upstream PR #7180](https://github.com/ItsNotRexed/Leonida/pull/7180), KkelebekK, commit `00551ba0099699f35893ecd58a9ec34a2b54e95c`, GPL-3.0 (open, unmerged) | zlib request path, `ZlibError` reasons in `shader_library.h`, the four decompression error constants and the zlib tests, adapted onto the strict-id shim |
| [`direct-storage` crate](https://docs.rs/direct-storage/0.5.1/src/direct_storage/bindings.rs.html) 0.5.1, MIT/Apache-2.0 | The names and values of `E_DSTORAGE_ZLIB_*` only (as recorded by PR #7180); no code |
| [RFC 1950](https://www.rfc-editor.org/rfc/rfc1950), [RFC 1951](https://www.rfc-editor.org/rfc/rfc1951) | The zlib and DEFLATE formats |
| ThatRecompDev/five-star-recomp `ff2a5dd95094fdceb5efdca9fd3236b7fa97076e`, GPL-3.0-or-later | Factory slots corroborated by source review; its unobserved file method is distinguished from the Xbox API; no code copied |
| californiaAlmonds/OpenScarlett `0cbc742aa607bb19e3a580bfb81442cfcd99e39c`, MIT | Interface ids and names; conflicting method assumptions are not adopted; no code copied |
| GDK DStorage reference and overview on Microsoft Learn; PC `dstorage.h`/`dstorageerr.h` 1.3.0 | Names, signatures, structures, error values (as recorded by the PR); facts only |
| Microsoft [QueueX1][queue-x1] and [QueueX2][queue-x2] interface references | Xbox inheritance: X1 adds `EnqueueSetEvent`, X2 adds `Query1`; no speculative auto-submit methods appended |

[queue-x1]: https://learn.microsoft.com/en-us/gaming/gdk/docs/reference/system/dstorage/interfaces/idstoragequeuex1/idstoragequeuex1?view=gdk-2604
[queue-x2]: https://learn.microsoft.com/en-us/gaming/gdk/docs/reference/system/dstorage/interfaces/idstoragequeuex2/idstoragequeuex2?view=gdk-2604
