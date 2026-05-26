#include <new>
#include <stddef.h>

#include "../../include/vk.h"

#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace {

constexpr std::size_t kAllocAlignment = 16;
constexpr std::size_t kDefaultArenaSize = 256 * 1024;
constexpr std::size_t kMinSplitPayload = 32;

struct free_block {
    std::size_t size;
    free_block* next;
};

struct arena_header {
    arena_header* next;
    std::size_t size;
};

static arena_header* g_arenas = nullptr;
static free_block* g_free_list = nullptr;

constexpr auto align_up(std::size_t value, std::size_t alignment) noexcept -> std::size_t
{
    return (value + alignment - 1) & ~(alignment - 1);
}

constexpr auto arena_header_size() noexcept -> std::size_t
{
    return align_up(sizeof(arena_header), kAllocAlignment);
}

constexpr auto free_block_size() noexcept -> std::size_t
{
    return align_up(sizeof(free_block), kAllocAlignment);
}

[[noreturn]] void runtime_abort() noexcept
{
    const vk_api_t* api = vk_get_api();
    if (api != nullptr && api->vk_exit != nullptr) {
        api->vk_exit(1);
    }

#if defined(_MSC_VER)
    __debugbreak();
    for (;;) {
    }
#else
    __builtin_trap();
#endif
}

auto runtime_kernel_alloc(std::size_t size) noexcept -> void*
{
    const vk_api_t* api = vk_get_api();
    if (api == nullptr || api->vk_malloc == nullptr) {
        return nullptr;
    }
    return api->vk_malloc(static_cast<vk_usize>(size));
}

auto block_payload(free_block* block) noexcept -> void*
{
    return reinterpret_cast<unsigned char*>(block) + free_block_size();
}

auto block_from_payload(void* ptr) noexcept -> free_block*
{
    return reinterpret_cast<free_block*>(
        reinterpret_cast<unsigned char*>(ptr) - free_block_size());
}

auto block_end(free_block* block) noexcept -> unsigned char*
{
    return reinterpret_cast<unsigned char*>(block_payload(block)) + block->size;
}

void coalesce_with_next(free_block* block) noexcept
{
    while (block->next != nullptr &&
           block_end(block) == reinterpret_cast<unsigned char*>(block->next)) {
        block->size += free_block_size() + block->next->size;
        block->next = block->next->next;
    }
}

auto request_arena(std::size_t min_payload) noexcept -> bool
{
    const std::size_t payload = align_up(min_payload, kAllocAlignment);
    std::size_t total_size =
        arena_header_size() + free_block_size() + payload;

    if (total_size < kDefaultArenaSize) {
        total_size = kDefaultArenaSize;
    }
    total_size = align_up(total_size, kAllocAlignment);

    void* raw = runtime_kernel_alloc(total_size);
    if (raw == nullptr) {
        return false;
    }

    auto* arena = reinterpret_cast<arena_header*>(raw);
    arena->next = g_arenas;
    arena->size = total_size;
    g_arenas = arena;

    auto* block = reinterpret_cast<free_block*>(
        reinterpret_cast<unsigned char*>(raw) + arena_header_size());
    block->size = total_size - arena_header_size() - free_block_size();
    block->next = nullptr;

    if (g_free_list == nullptr ||
        reinterpret_cast<unsigned char*>(block) <
            reinterpret_cast<unsigned char*>(g_free_list)) {
        block->next = g_free_list;
        g_free_list = block;
        coalesce_with_next(block);
        return true;
    }

    free_block* current = g_free_list;
    while (current->next != nullptr &&
           reinterpret_cast<unsigned char*>(current->next) <
               reinterpret_cast<unsigned char*>(block)) {
        current = current->next;
    }

    block->next = current->next;
    current->next = block;
    coalesce_with_next(block);
    coalesce_with_next(current);
    return true;
}

auto runtime_alloc(std::size_t size) noexcept -> void*
{
    if (size == 0) {
        size = 1;
    }

    size = align_up(size, kAllocAlignment);

    for (free_block** link = &g_free_list; *link != nullptr; link = &(*link)->next) {
        free_block* block = *link;
        if (block->size < size) {
            continue;
        }

        const std::size_t remaining = block->size - size;
        if (remaining >= free_block_size() + kMinSplitPayload) {
            auto* split = reinterpret_cast<free_block*>(
                reinterpret_cast<unsigned char*>(block_payload(block)) + size);
            split->size = remaining - free_block_size();
            split->next = block->next;
            *link = split;
            block->size = size;
        } else {
            *link = block->next;
        }

        block->next = nullptr;
        return block_payload(block);
    }

    if (!request_arena(size)) {
        return nullptr;
    }

    return runtime_alloc(size);
}

void runtime_free(void* ptr) noexcept
{
    if (ptr == nullptr) {
        return;
    }

    free_block* block = block_from_payload(ptr);
    block->next = nullptr;

    if (g_free_list == nullptr ||
        reinterpret_cast<unsigned char*>(block) <
            reinterpret_cast<unsigned char*>(g_free_list)) {
        block->next = g_free_list;
        g_free_list = block;
        coalesce_with_next(block);
        return;
    }

    free_block* current = g_free_list;
    while (current->next != nullptr &&
           reinterpret_cast<unsigned char*>(current->next) <
               reinterpret_cast<unsigned char*>(block)) {
        current = current->next;
    }

    block->next = current->next;
    current->next = block;
    coalesce_with_next(block);
    coalesce_with_next(current);
}

}  // namespace

#if defined(_MSC_VER)
extern "C" void* __dso_handle = nullptr;

extern "C" int atexit(void (__cdecl*)(void))
{
    return 0;
}
#else
void* __dso_handle __attribute__((weak, visibility("hidden"))) = nullptr;
#endif

extern "C" int __cxa_atexit(void (*)(void*), void*, void*) noexcept
{
    return 0;
}

extern "C" void __cxa_finalize(void*) noexcept
{
}

extern "C" int __cxa_guard_acquire(long long* guard) noexcept
{
    return *reinterpret_cast<unsigned char*>(guard) == 1u ? 0 : 1;
}

extern "C" void __cxa_guard_release(long long* guard) noexcept
{
    *reinterpret_cast<unsigned char*>(guard) = 1u;
}

extern "C" void __cxa_guard_abort(long long* guard) noexcept
{
    *reinterpret_cast<unsigned char*>(guard) = 0u;
}

static void* allocate_or_abort(size_t size)
{
    void* ptr = runtime_alloc(size);
    if (ptr == nullptr) {
        runtime_abort();
    }
    return ptr;
}

void* operator new(std::size_t size)
{
    return allocate_or_abort(size);
}

void* operator new[](std::size_t size)
{
    return allocate_or_abort(size);
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept
{
    return runtime_alloc(size);
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept
{
    return runtime_alloc(size);
}

void operator delete(void* ptr) noexcept
{
    runtime_free(ptr);
}

void operator delete[](void* ptr) noexcept
{
    runtime_free(ptr);
}

void operator delete(void* ptr, std::size_t) noexcept
{
    runtime_free(ptr);
}

void operator delete[](void* ptr, std::size_t) noexcept
{
    runtime_free(ptr);
}

extern "C" void __cxa_pure_virtual() noexcept
{
    runtime_abort();
}

namespace std {

const nothrow_t nothrow{};

void terminate() noexcept
{
    runtime_abort();
}

}  // namespace std
