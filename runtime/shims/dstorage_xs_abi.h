// Xbox DirectStorage ABI: the console API (dstorage_xs.h on the console) a title calls through DStorageGetFactory.
//
// Provenance: adapted from upstream PR #4348 by mariuslozo-boop (commit 45c39cf6f4148aebbc8a88d476bea755887d35bd,
// GPL-3.0); see UPSTREAM.md. Palmwave's own declarations: names, member order and types follow the public GDK reference
// (learn.microsoft.com/gaming/gdk/docs/reference/system/dstorage); no Microsoft header is copied. Palmwave changes:
// the interface-id section (measured factory and file ids, no PC id accepted for a console object) and slot constants.
//
// VERIFY (docs/directstorage.md "What is not measured"):
//   - Vtable order. The reference lists methods alphabetically; the order below is the PC dstorage.h order for the
//     methods both APIs share, with the console-only ones where the GDK overview lists them. Public-source review
//     agrees on the factory's SetStagingBufferSize at slot 8 and CreateQueue1 at slot 10, which this order gives
//     (kFactorySlot* below; dstorage_test calls both through the raw vtable). These ordinals are not locally measured.
//   - The capacity bounds, DSTORAGE_REQUEST_MAX_NAME and the E_DSTORAGE_* values are the PC header's.
#pragma once
#include <windows.h>
#include <unknwn.h>

#include <cstddef>
#include <cstdint>

struct ID3D12Fence;  // d3d12.h; the shim calls Signal through it

namespace dsx {

#pragma warning(push)
#pragma warning(disable : 4201)  // nameless struct/union: the reference declares the members that way

// ---- Constants and enumerations -----------------------------------------------------------------------------------

constexpr uint16_t kMinQueueCapacity = 0x80;    // DSTORAGE_MIN_QUEUE_CAPACITY (PC value)
constexpr uint16_t kMaxQueueCapacity = 0x2000;  // DSTORAGE_MAX_QUEUE_CAPACITY (PC value)
constexpr size_t kRequestMaxName = 64;          // DSTORAGE_REQUEST_MAX_NAME (PC value)
constexpr uint64_t kMaxRequestBytes = 1ull << 30;           // DSTORAGE_MAX_REQUEST_SIZE: source + destination
constexpr uint32_t kMax4kPageDestinationSize = 32u << 20;  // DSTORAGE_MAX_4K_PAGE_DESTINATION_SIZE

enum DSTORAGE_PRIORITY : int8_t {
    DSTORAGE_PRIORITY_LOW = -1,
    DSTORAGE_PRIORITY_NORMAL = 0,
    DSTORAGE_PRIORITY_HIGH = 1,
    DSTORAGE_PRIORITY_REALTIME = 2,
    DSTORAGE_PRIORITY_FIRST = DSTORAGE_PRIORITY_LOW,
    DSTORAGE_PRIORITY_LAST = DSTORAGE_PRIORITY_REALTIME,
    DSTORAGE_PRIORITY_COUNT = 4,
};

enum DSTORAGE_REQUEST_SOURCE_TYPE : uint64_t {
    DSTORAGE_REQUEST_SOURCE_FILE = 0,
    DSTORAGE_REQUEST_SOURCE_MEMORY = 1,
};

enum DSTORAGE_BCPACK_MODE : uint64_t {
    DSTORAGE_BCPACK_MODE_NONE = 0,
    DSTORAGE_BCPACK_MODE_BC1 = 1,
    DSTORAGE_BCPACK_MODE_BC2 = 2,
    DSTORAGE_BCPACK_MODE_BC3 = 3,
    DSTORAGE_BCPACK_MODE_BC4 = 4,
    DSTORAGE_BCPACK_MODE_BC5 = 5,
    DSTORAGE_BCPACK_MODE_BC6H = 6,
    DSTORAGE_BCPACK_MODE_BC7 = 7,
};

enum DSTORAGE_SWIZZLE_MODE : uint64_t {
    DSTORAGE_SWIZZLE_MODE_NONE = 0,
    DSTORAGE_SWIZZLE_MODE_4K_4K_8K = 1,
    DSTORAGE_SWIZZLE_MODE_4K_4K_24K = 2,
    DSTORAGE_SWIZZLE_MODE_4K_12K_4K_4K_8K = 3,
    DSTORAGE_SWIZZLE_MODE_4K_12K_4K_12K = 4,
};

enum DSTORAGE_COMMAND_TYPE : int32_t {
    DSTORAGE_COMMAND_TYPE_NONE = -1,
    DSTORAGE_COMMAND_TYPE_REQUEST = 0,
    DSTORAGE_COMMAND_TYPE_STATUS = 1,
    DSTORAGE_COMMAND_TYPE_SIGNAL = 2,
};
// Not in the console enum: the PC API's DSTORAGE_COMMAND_TYPE_EVENT, used here for a failed EnqueueSetEvent.
constexpr DSTORAGE_COMMAND_TYPE kCommandTypeEvent = static_cast<DSTORAGE_COMMAND_TYPE>(3);

enum DSTORAGE_DEBUG : uint32_t {
    DSTORAGE_DEBUG_NONE = 0x00,
    DSTORAGE_DEBUG_SHOW_ERRORS = 0x01,
    DSTORAGE_DEBUG_BREAK_ON_ERROR = 0x02,
    DSTORAGE_DEBUG_RECORD_OBJECT_NAMES = 0x04,
};

// The staging-buffer sizes the GDK overview lists as valid (its enumeration page is not public).
constexpr uint32_t kStagingBufferSizes[] = {0, 16u << 20, 20u << 20, 24u << 20, 28u << 20, 32u << 20};

// ---- Errors (FACILITY_GAME, the PC dstorageerr.h values) ----------------------------------------------------------

constexpr HRESULT E_DSTORAGE_INVALID_QUEUE_CAPACITY = static_cast<HRESULT>(0x89240003L);
constexpr HRESULT E_DSTORAGE_END_OF_FILE = static_cast<HRESULT>(0x89240007L);
constexpr HRESULT E_DSTORAGE_REQUEST_TOO_LARGE = static_cast<HRESULT>(0x89240008L);
constexpr HRESULT E_DSTORAGE_ACCESS_VIOLATION = static_cast<HRESULT>(0x89240009L);
constexpr HRESULT E_DSTORAGE_FILE_NOT_OPEN = static_cast<HRESULT>(0x8924000BL);
constexpr HRESULT E_DSTORAGE_RESERVED_FIELDS = static_cast<HRESULT>(0x8924000CL);
constexpr HRESULT E_DSTORAGE_INVALID_BCPACK_MODE = static_cast<HRESULT>(0x8924000DL);
constexpr HRESULT E_DSTORAGE_INVALID_SWIZZLE_MODE = static_cast<HRESULT>(0x8924000EL);
constexpr HRESULT E_DSTORAGE_INVALID_DESTINATION_SIZE = static_cast<HRESULT>(0x8924000FL);
constexpr HRESULT E_DSTORAGE_QUEUE_CLOSED = static_cast<HRESULT>(0x89240010L);
constexpr HRESULT E_DSTORAGE_INVALID_QUEUE_PRIORITY = static_cast<HRESULT>(0x89240013L);
constexpr HRESULT E_DSTORAGE_INDEX_BOUND = static_cast<HRESULT>(0x89240015L);
constexpr HRESULT E_DSTORAGE_INVALID_FILE_HANDLE = static_cast<HRESULT>(0x89240017L);
constexpr HRESULT E_DSTORAGE_INVALID_FILE_OFFSET = static_cast<HRESULT>(0x8924001AL);
constexpr HRESULT E_DSTORAGE_INVALID_SOURCE_TYPE = static_cast<HRESULT>(0x8924001BL);
constexpr HRESULT E_DSTORAGE_INVALID_INTERMEDIATE_SIZE = static_cast<HRESULT>(0x8924001CL);
constexpr HRESULT E_DSTORAGE_STAGING_BUFFER_LOCKED = static_cast<HRESULT>(0x8924001FL);
constexpr HRESULT E_DSTORAGE_INVALID_STAGING_BUFFER_SIZE = static_cast<HRESULT>(0x89240020L);
constexpr HRESULT E_DSTORAGE_INVALID_FENCE = static_cast<HRESULT>(0x89240022L);
constexpr HRESULT E_DSTORAGE_INVALID_STATUS_ARRAY = static_cast<HRESULT>(0x89240023L);
constexpr HRESULT E_DSTORAGE_INVALID_MEMORY_QUEUE_PRIORITY = static_cast<HRESULT>(0x89240024L);
// The decompression failures (adapted from upstream PR #7180, KkelebekK, commit 00551ba0, GPL-3.0).
// E_DSTORAGE_DECOMPRESSION_ERROR ("a generic error has happened during decompression") is on the public DirectStorage
// constants page. The three zlib codes have no public description: their names and values are those of the
// header-generated bindings of the `direct-storage` crate 0.5.1 (docs/directstorage.md "Sources"). Which stream fault
// the console reports with each is inferred from the names only (PARITY_FAIL read as the Adler-32 check).
constexpr HRESULT E_DSTORAGE_DECOMPRESSION_ERROR = static_cast<HRESULT>(0x89240030L);
constexpr HRESULT E_DSTORAGE_ZLIB_BAD_HEADER = static_cast<HRESULT>(0x89240031L);
constexpr HRESULT E_DSTORAGE_ZLIB_BAD_DATA = static_cast<HRESULT>(0x89240032L);
constexpr HRESULT E_DSTORAGE_ZLIB_PARITY_FAIL = static_cast<HRESULT>(0x89240033L);

// ---- Structures ---------------------------------------------------------------------------------------------------

// DSTORAGE_REQUEST_OPTIONS as the reference declares it. The shim reads AsUINT64 through the bit positions below
// (MSVC allocates bit-fields from bit 0 up); dstorage_test checks that the two agree.
union DSTORAGE_REQUEST_OPTIONS {
    struct {
        uint64_t ZlibDecompress : 1;
        DSTORAGE_BCPACK_MODE BcpackMode : 4;
        DSTORAGE_SWIZZLE_MODE SwizzleMode : 16;
        uint64_t DestinationIsPhysicalPages : 1;
        DSTORAGE_REQUEST_SOURCE_TYPE SourceType : 1;
        uint64_t SourceIsPhysicalPages : 1;
        uint64_t Reserved : 40;
    };
    uint64_t AsUINT64;
};
static_assert(sizeof(DSTORAGE_REQUEST_OPTIONS) == 8);

namespace options {
constexpr uint64_t kZlib = 1ull << 0;
constexpr unsigned kBcpackShift = 1;
constexpr uint64_t kBcpackMask = 0xFull << kBcpackShift;
constexpr unsigned kSwizzleShift = 5;
constexpr uint64_t kSwizzleMask = 0xFFFFull << kSwizzleShift;
constexpr uint64_t kDestinationPages = 1ull << 21;
constexpr uint64_t kSourceMemory = 1ull << 22;  // SourceType == DSTORAGE_REQUEST_SOURCE_MEMORY
constexpr uint64_t kSourcePages = 1ull << 23;
constexpr uint64_t kReservedMask = ~((1ull << 24) - 1);

constexpr bool zlib(uint64_t o) { return (o & kZlib) != 0; }
constexpr uint64_t bcpack(uint64_t o) { return (o & kBcpackMask) >> kBcpackShift; }
constexpr uint64_t swizzle(uint64_t o) { return (o & kSwizzleMask) >> kSwizzleShift; }
constexpr DSTORAGE_REQUEST_SOURCE_TYPE source_type(uint64_t o) {
    return (o & kSourceMemory) ? DSTORAGE_REQUEST_SOURCE_MEMORY : DSTORAGE_REQUEST_SOURCE_FILE;
}
}  // namespace options

struct IDStorageFileX;
struct IDStorageStatusArrayX;

struct DSTORAGE_REQUEST {
    DSTORAGE_REQUEST_OPTIONS Options;
    union {
        void* Destination;
        struct {
            PULONG_PTR DestinationPageArray;
            UINT16 DestinationPageOffset;
        };
    };
    UINT32 DestinationSize;
    union {
        struct {
            IDStorageFileX* File;
            UINT64 FileOffset;
        };
        const void* Source;
        struct {
            PULONG_PTR SourcePageArray;
            UINT16 SourcePageOffset;
        };
    };
    UINT32 SourceSize;
    UINT32 IntermediateSize;
    UINT64 CancellationTag;
    const CHAR* Name;
};
static_assert(offsetof(DSTORAGE_REQUEST, Destination) == 8 && offsetof(DSTORAGE_REQUEST, DestinationPageOffset) == 16);
static_assert(offsetof(DSTORAGE_REQUEST, DestinationSize) == 24 && offsetof(DSTORAGE_REQUEST, File) == 32);
static_assert(offsetof(DSTORAGE_REQUEST, FileOffset) == 40 && offsetof(DSTORAGE_REQUEST, Source) == 32);
static_assert(offsetof(DSTORAGE_REQUEST, SourcePageOffset) == 40 && offsetof(DSTORAGE_REQUEST, SourceSize) == 48);
static_assert(offsetof(DSTORAGE_REQUEST, IntermediateSize) == 52 && offsetof(DSTORAGE_REQUEST, CancellationTag) == 56);
static_assert(offsetof(DSTORAGE_REQUEST, Name) == 64 && sizeof(DSTORAGE_REQUEST) == 72);

struct DSTORAGE_QUEUE_DESC {
    DSTORAGE_REQUEST_SOURCE_TYPE SourceType;
    UINT16 Capacity;
    DSTORAGE_PRIORITY Priority;
    const CHAR* Name;
};
static_assert(offsetof(DSTORAGE_QUEUE_DESC, Capacity) == 8 && offsetof(DSTORAGE_QUEUE_DESC, Priority) == 10);
static_assert(offsetof(DSTORAGE_QUEUE_DESC, Name) == 16 && sizeof(DSTORAGE_QUEUE_DESC) == 24);

union DSTORAGE_QUEUE_OPTIONS {
    struct {
        uint64_t AutoSubmitDisabled : 1;
        uint64_t Reserved : 63;
    };
    uint64_t AsUINT64;
};
static_assert(sizeof(DSTORAGE_QUEUE_OPTIONS) == 8);

struct DSTORAGE_QUEUE_DESC1 {
    DSTORAGE_REQUEST_SOURCE_TYPE SourceType;
    UINT16 Capacity;
    DSTORAGE_PRIORITY Priority;
    const CHAR* Name;
    DSTORAGE_QUEUE_OPTIONS Options;
};
static_assert(offsetof(DSTORAGE_QUEUE_DESC1, Options) == 24 && sizeof(DSTORAGE_QUEUE_DESC1) == 32);

struct DSTORAGE_QUEUE_INFO {
    DSTORAGE_QUEUE_DESC Desc;
    UINT16 EmptySlotCount;
    UINT16 RequestCountUntilAutoSubmit;
};
static_assert(offsetof(DSTORAGE_QUEUE_INFO, EmptySlotCount) == 24 && sizeof(DSTORAGE_QUEUE_INFO) == 32);

struct DSTORAGE_QUEUE_INFO1 {
    DSTORAGE_QUEUE_DESC1 Desc;
    UINT16 EmptySlotCount;
    UINT16 RequestCountUntilAutoSubmit;
};
static_assert(offsetof(DSTORAGE_QUEUE_INFO1, EmptySlotCount) == 32 && sizeof(DSTORAGE_QUEUE_INFO1) == 40);

// The reference declares the three failure kinds as unnamed structs inside DSTORAGE_ERROR_RECORD; they are named
// here (Palmwave's names) so the shim can fill them. The layout is the same.
struct DSTORAGE_ERROR_REQUEST {
    DSTORAGE_REQUEST_OPTIONS Options;
    void* Destination;
    UINT32 DestinationSize;
    WCHAR Filename[MAX_PATH];
    UINT64 FileOffset;
    const void* Source;
    UINT32 SourceSize;
    UINT64 CancellationTag;
    CHAR Name[kRequestMaxName];
};
struct DSTORAGE_ERROR_STATUS {
    IDStorageStatusArrayX* StatusArray;
    UINT32 Index;
};
struct DSTORAGE_ERROR_SIGNAL {
    ID3D12Fence* Fence;
    UINT64 Value;
};
struct DSTORAGE_ERROR_FIRST_FAILURE {
    HRESULT HResult;
    DSTORAGE_COMMAND_TYPE CommandType;
    union {
        DSTORAGE_ERROR_REQUEST Request;
        DSTORAGE_ERROR_STATUS Status;
        DSTORAGE_ERROR_SIGNAL Signal;
    };
};
struct DSTORAGE_ERROR_RECORD {
    UINT32 FailureCount;
    DSTORAGE_ERROR_FIRST_FAILURE FirstFailure;
};
static_assert(offsetof(DSTORAGE_ERROR_RECORD, FirstFailure) == 8);
static_assert(offsetof(DSTORAGE_ERROR_FIRST_FAILURE, Request) == 8);
static_assert(offsetof(DSTORAGE_ERROR_REQUEST, Filename) == 20 && offsetof(DSTORAGE_ERROR_REQUEST, FileOffset) == 544);
static_assert(offsetof(DSTORAGE_ERROR_REQUEST, Name) == 576 && sizeof(DSTORAGE_ERROR_REQUEST) == 640);
static_assert(sizeof(DSTORAGE_ERROR_RECORD) == 656);

struct DSTORAGE_PRIORITY_STATS {
    UINT32 CurrentQueueCount;
    UINT32 MaxQueueCount;
    UINT32 CumulativeQueueCount;
    UINT32 TotalStatusCount;
    UINT32 TotalFenceCount;
    UINT32 PendingRequestCount;
    UINT32 TotalRequestCount;
    UINT64 TotalRequestSourceByteCount;
    UINT64 TotalRequestDestinationByteCount;
};
static_assert(sizeof(DSTORAGE_PRIORITY_STATS) == 48);

struct DSTORAGE_STATS {
    DSTORAGE_PRIORITY_STATS LowPriorityStats;
    DSTORAGE_PRIORITY_STATS NormalPriorityStats;
    DSTORAGE_PRIORITY_STATS HighPriorityStats;
    DSTORAGE_PRIORITY_STATS RealtimePriorityStats;
    UINT8 CurrentOpenVirtualDiskCount;
    UINT8 MaxOpenVirtualDiskCount;
    UINT32 CumulativeOpenVirtualDiskCount;
    UINT32 PendingStorageRequestCount;
    UINT32 TotalStorageRequestCount;
    UINT32 StorageRetries;
    UINT32 StorageErrors;
    UINT32 PendingDecompressionRequestCount;
    UINT32 TotalDecompressionRequestCount;
    UINT32 DecompressionErrors;
    UINT32 StagingBufferStallCount;
    UINT32 CurrentOpenFileCount;
    UINT32 MaxOpenFileCount;
    UINT32 CumulativeOpenFileCount;
    UINT32 CurrentTitleMemoryUsageBytes;
    UINT32 MaxTitleMemoryUsageBytes;
    UINT32 ErrorCount;
};

#pragma warning(pop)

// ---- Interfaces, in the vtable order chosen above ---------------------------------------------------------------

struct IDStorageStatusArrayX : IUnknown {
    virtual bool STDMETHODCALLTYPE IsComplete(UINT32 index) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetHResult(UINT32 index) = 0;
};

struct IDStorageFileX : IUnknown {
    virtual void STDMETHODCALLTYPE Close() = 0;
    // The public Xbox reference specifies HANDLE GetHandle(), not the PC GetFileInformation(out*) method.
    virtual HANDLE STDMETHODCALLTYPE GetHandle() = 0;
};

struct IDStorageQueueX : IUnknown {
    virtual void STDMETHODCALLTYPE EnqueueRequest(const DSTORAGE_REQUEST* request) = 0;
    virtual void STDMETHODCALLTYPE EnqueueStatus(IDStorageStatusArrayX* status_array, UINT32 index) = 0;
    virtual void STDMETHODCALLTYPE EnqueueSignal(ID3D12Fence* fence, UINT64 value) = 0;
    virtual void STDMETHODCALLTYPE Submit() = 0;
    virtual void STDMETHODCALLTYPE CancelRequestsWithTag(UINT64 mask, UINT64 value) = 0;
    virtual void STDMETHODCALLTYPE Close() = 0;
    virtual HANDLE STDMETHODCALLTYPE GetErrorEvent() = 0;
    virtual void STDMETHODCALLTYPE RetrieveErrorRecord(DSTORAGE_ERROR_RECORD* record) = 0;
    virtual void STDMETHODCALLTYPE Query(DSTORAGE_QUEUE_INFO* info) = 0;
};

struct IDStorageQueueX1 : IDStorageQueueX {
    virtual void STDMETHODCALLTYPE EnqueueSetEvent(HANDLE handle) = 0;
};

struct IDStorageQueueX2 : IDStorageQueueX1 {
    virtual void STDMETHODCALLTYPE Query1(DSTORAGE_QUEUE_INFO1* info) = 0;
};

struct IDStorageFactoryX : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE CreateQueue(const DSTORAGE_QUEUE_DESC* desc, REFIID riid, void** ppv) = 0;
    virtual HRESULT STDMETHODCALLTYPE OpenFile(const WCHAR* path, REFIID riid, void** ppv) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateStatusArray(UINT32 capacity, PCSTR name, REFIID riid, void** ppv) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetCpuAffinity(UINT64 affinity) = 0;
    virtual void STDMETHODCALLTYPE SetDebugFlags(UINT32 flags) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetStagingBufferSize(UINT32 size) = 0;
};

struct IDStorageFactoryX1 : IDStorageFactoryX {
    virtual void STDMETHODCALLTYPE GetStats(DSTORAGE_STATS* stats) = 0;
};

struct IDStorageFactoryX2 : IDStorageFactoryX1 {
    virtual HRESULT STDMETHODCALLTYPE CreateQueue1(const DSTORAGE_QUEUE_DESC1* desc, REFIID riid, void** ppv) = 0;
};

// The factory's vtable slots in the order above (IUnknown is 0-2). Public-source review corroborates slots 8 and 10;
// dstorage_test calls these through the raw vtable, not through the C++ declarations. See docs/directstorage.md.
constexpr unsigned kFactorySlotCreateQueue = 3;
constexpr unsigned kFactorySlotOpenFile = 4;
constexpr unsigned kFactorySlotCreateStatusArray = 5;
constexpr unsigned kFactorySlotSetCpuAffinity = 6;
constexpr unsigned kFactorySlotSetDebugFlags = 7;
constexpr unsigned kFactorySlotSetStagingBufferSize = 8;
constexpr unsigned kFactorySlotGetStats = 9;
constexpr unsigned kFactorySlotCreateQueue1 = 10;

using DStorageGetFactoryFn = HRESULT(WINAPI*)(REFIID riid, void** ppv);

// ---- Interface ids -------------------------------------------------------------------------------------------------
//
// Strict: an object is handed out only for IUnknown or for an id listed here for its kind. The public OpenScarlett
// header names these console ids; local calls confirmed the two requests below. Nothing is learned at
// run time, and no PC DirectStorage id is accepted for a console object, whatever its method shape.

// The id the retail executable passes to DStorageGetFactory (confirmed by the local offline boot). Which interface
// version it names comes from the public OpenScarlett header: IDStorageFactoryX2. Its method order remains sourced,
// rather than locally measured.
constexpr GUID kRetailFactoryId = {0x62959d11, 0xd45f, 0x4884, {0xa0, 0xa3, 0xf7, 0x86, 0x2e, 0xa2, 0x43, 0xae}};
// IDStorageFileX: public IID corroborated by OpenFile; the Microsoft Xbox API specifies Close and GetHandle.
constexpr GUID kRetailFileId = {0x43704f9f, 0x27c6, 0x4a9f, {0x88, 0x15, 0xd1, 0x65, 0x27, 0x89, 0x4d, 0xf4}};

enum class ObjectKind { Factory, Queue, File, StatusArray };

struct KnownInterfaceId {
    GUID id;
    ObjectKind kind;
    const char* what;
};

constexpr KnownInterfaceId kKnownInterfaceIds[] = {
    {kRetailFactoryId, ObjectKind::Factory, "retail DStorageGetFactory id"},
    {kRetailFileId, ObjectKind::File, "IDStorageFileX id"},
    // Queue and status-array ids: none measured yet. A creating call with one is E_NOINTERFACE and logs it.
};

// The PC DirectStorage ids (dstorage.h 1.3.0). Refused with a log line that names them; their objects have the PC
// shape, and a matching method order does not make a PC id this console ABI.
struct PcInterfaceId {
    GUID id;
    const char* name;
};
constexpr PcInterfaceId kPcInterfaceIds[] = {
    {{0x6924ea0c, 0xc3cd, 0x4826, {0xb1, 0x0a, 0xf6, 0x4f, 0x4e, 0xd9, 0x27, 0xc1}}, "IDStorageFactory"},
    {{0x5de95e7b, 0x955a, 0x4868, {0xa7, 0x3c, 0x24, 0x3b, 0x29, 0xf4, 0xb8, 0xda}}, "IDStorageFile"},
    {{0x82397587, 0x7cd5, 0x453b, {0xa0, 0x2e, 0x31, 0x37, 0x9b, 0xd6, 0x46, 0x56}}, "IDStorageStatusArray"},
    {{0xcfdbd83f, 0x9e06, 0x4fda, {0x8e, 0xa5, 0x69, 0x04, 0x21, 0x37, 0xf4, 0x9b}}, "IDStorageQueue"},
    {{0xdd2f482c, 0x5eff, 0x41e8, {0x9c, 0x9e, 0xd2, 0x37, 0x4b, 0x27, 0x81, 0x28}}, "IDStorageQueue1"},
    {{0xb1c9d643, 0x3a49, 0x44a2, {0xb4, 0x6f, 0x65, 0x36, 0x49, 0x47, 0x0d, 0x18}}, "IDStorageQueue2"},
    {{0xdeb54c52, 0xeca8, 0x46b3, {0x82, 0xa7, 0x03, 0x1b, 0x72, 0x26, 0x26, 0x53}}, "IDStorageQueue3"},
    {{0x97179b2f, 0x2c21, 0x49ca, {0x82, 0x91, 0x4e, 0x1b, 0xf4, 0xa1, 0x60, 0xdf}}, "IDStorageCustomDecompressionQueue"},
    {{0x0d47c6c9, 0xe61a, 0x4706, {0x93, 0xb4, 0x68, 0xbf, 0xe3, 0xf4, 0xaa, 0x4a}}, "IDStorageCustomDecompressionQueue1"},
    {{0x84ef5121, 0x9b43, 0x4d03, {0xb5, 0xc1, 0xcc, 0x34, 0x60, 0x6b, 0x26, 0x2d}}, "IDStorageCompressionCodec"},
};

}  // namespace dsx
