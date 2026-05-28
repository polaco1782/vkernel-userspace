#include "dynamic_plugin_loader.h"

#include "console_log.h"
#include "vkgui_common.h"
#include "window_manager.h"

#include "vkernel/elf.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <utility>

namespace vkgui {

namespace {

using init_array_fn = void (*)();

constexpr auto k_plugin_init_symbol = "vkgui_plugin_init";
constexpr auto k_min_image_alignment = 4096ULL;

PluginHost* g_active_plugin_host = nullptr;

struct dynamic_info {
    vk_u64 rela_vaddr = 0;
    vk_u64 rela_size = 0;
    vk_u64 rela_ent = sizeof(vk::elf::Elf64_Rela);
    vk_u64 init_array_vaddr = 0;
    vk_u64 init_array_size = 0;
    vk_u64 fini_array_vaddr = 0;
    vk_u64 fini_array_size = 0;
};

struct dynsym_info {
    const vk::elf::Elf64_Sym* symbols = nullptr;
    const char* strings = nullptr;
    vk_usize count = 0;
};

constexpr auto align_up(vk_u64 value, vk_u64 alignment) -> vk_u64
{
    return alignment == 0 ? value : (value + alignment - 1ULL) & ~(alignment - 1ULL);
}

auto range_ok(vk_usize file_size, vk_u64 offset, vk_u64 length) -> bool
{
    return offset <= file_size && length <= file_size - offset;
}

void set_active_plugin_host(PluginHost* host)
{
    g_active_plugin_host = host;
}

void clear_active_plugin_host()
{
    g_active_plugin_host = nullptr;
}

auto active_plugin_host() -> PluginHost*
{
    return g_active_plugin_host;
}

void host_set_next_window_pos_first_use(float x, float y)
{
    ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_FirstUseEver);
}

void host_set_next_window_size_first_use(float width, float height)
{
    ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_FirstUseEver);
}

auto host_begin_window(const char* title, vk_u32* open) -> vk_u32
{
    bool open_value = open == nullptr || *open != 0;
    const bool began = imgui_begin_window_readable_caption(title, open != nullptr ? &open_value : nullptr);
    if (open != nullptr) {
        *open = open_value ? 1u : 0u;
    }
    return began ? 1u : 0u;
}

void host_end_window()
{
    ImGui::End();
}

void host_clear_focus_if_host_window_focused()
{
    if (PluginHost* host = active_plugin_host(); host != nullptr) {
        host->window_manager.clear_focus_if_host_window_focused();
    }
}

void host_separator_text(const char* text)
{
    ImGui::SeparatorText(text != nullptr ? text : "");
}

void host_text(const char* text)
{
    ImGui::TextUnformatted(text != nullptr ? text : "");
}

void host_textf(const char* fmt, ...)
{
    if (fmt == nullptr) {
        return;
    }

    std::array<char, 256> buffer {};
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer.data(), buffer.size(), fmt, args);
    va_end(args);
    ImGui::TextUnformatted(buffer.data());
}

void host_same_line()
{
    ImGui::SameLine();
}

auto host_button(const char* label, float width, float height) -> vk_u32
{
    return ImGui::Button(label != nullptr ? label : "", ImVec2(width, height)) ? 1u : 0u;
}

void host_spacing()
{
    ImGui::Spacing();
}

void host_log(const char* text)
{
    if (PluginHost* host = active_plugin_host(); host != nullptr && text != nullptr) {
        host->log.add(text);
    }
}

void host_logf(const char* fmt, ...)
{
    if (fmt == nullptr) {
        return;
    }

    PluginHost* host = active_plugin_host();
    if (host == nullptr) {
        return;
    }

    std::array<char, 256> buffer {};
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer.data(), buffer.size(), fmt, args);
    va_end(args);
    host->log.add(buffer.data());
}

auto plugin_host_api() -> const vkgui_plugin_host_api_t&
{
    static const vkgui_plugin_host_api_t api = {
        VKGUI_PLUGIN_HOST_API_VERSION,
        vk_get_api(),
        host_set_next_window_pos_first_use,
        host_set_next_window_size_first_use,
        host_begin_window,
        host_end_window,
        host_clear_focus_if_host_window_focused,
        host_separator_text,
        host_text,
        host_textf,
        host_same_line,
        host_button,
        host_spacing,
        host_log,
        host_logf,
    };
    return api;
}

auto read_file_bytes(vk::string_view path, std::string& bytes) -> bool
{
    bytes.clear();

    const std::string path_string = string_from_view(path);
    const vk_usize expected_size = VK_CALL(file_size, path_string.c_str());
    const vk_file_handle_t handle = VK_CALL(file_open, path_string.c_str(), "r");
    if (handle == static_cast<vk_file_handle_t>(0)) {
        return false;
    }

    if (expected_size != 0) {
        bytes.resize(expected_size);
        vk_usize total = 0;
        while (total < expected_size) {
            const vk_usize count = VK_CALL(file_read_handle,
                                           handle,
                                           bytes.data() + total,
                                           expected_size - total);
            if (count == 0) {
                break;
            }
            total += count;
        }
        bytes.resize(total);
    } else {
        std::array<char, 512> chunk {};
        for (;;) {
            const vk_usize count = VK_CALL(file_read_handle, handle, chunk.data(), chunk.size());
            if (count == 0) {
                break;
            }
            bytes.append(chunk.data(), count);
        }
    }

    VK_CALL(file_close, handle);
    return !bytes.empty();
}

auto collect_dynsym_info(const unsigned char* file_data,
                         vk_usize file_size,
                         const vk::elf::Elf64_Ehdr& ehdr,
                         dynsym_info& out) -> bool
{
    if (ehdr.e_shoff == 0
        || ehdr.e_shentsize != sizeof(vk::elf::Elf64_Shdr)
        || ehdr.e_shnum == 0
        || !range_ok(file_size,
                     ehdr.e_shoff,
                     static_cast<vk_u64>(ehdr.e_shnum) * sizeof(vk::elf::Elf64_Shdr))) {
        return false;
    }

    const auto* shdrs = reinterpret_cast<const vk::elf::Elf64_Shdr*>(file_data + ehdr.e_shoff);
    for (u16 index = 0; index < ehdr.e_shnum; ++index) {
        const auto& shdr = shdrs[index];
        if (shdr.sh_type != vk::elf::SHT_DYNSYM || shdr.sh_entsize != sizeof(vk::elf::Elf64_Sym)) {
            continue;
        }
        if (shdr.sh_link >= ehdr.e_shnum) {
            return false;
        }

        const auto& strtab = shdrs[shdr.sh_link];
        if (strtab.sh_type != vk::elf::SHT_STRTAB) {
            return false;
        }
        if (!range_ok(file_size, shdr.sh_offset, shdr.sh_size)
            || !range_ok(file_size, strtab.sh_offset, strtab.sh_size)) {
            return false;
        }

        out.symbols = reinterpret_cast<const vk::elf::Elf64_Sym*>(file_data + shdr.sh_offset);
        out.strings = reinterpret_cast<const char*>(file_data + strtab.sh_offset);
        out.count = static_cast<vk_usize>(shdr.sh_size / shdr.sh_entsize);
        return out.count != 0;
    }

    return false;
}

auto resolve_dynamic_info(const vk::elf::Elf64_Ehdr& ehdr,
                          const vk::elf::Elf64_Phdr* phdrs,
                          unsigned char* /*image_base*/,
                          long long load_bias,
                          dynamic_info& out) -> bool
{
    for (u16 index = 0; index < ehdr.e_phnum; ++index) {
        const auto& phdr = phdrs[index];
        if (phdr.p_type != vk::elf::PT_DYNAMIC) {
            continue;
        }

        const auto* dyn = reinterpret_cast<const vk::elf::Elf64_Dyn*>(
            reinterpret_cast<unsigned char*>(static_cast<long long>(phdr.p_vaddr) + load_bias));
        for (; dyn->d_tag != vk::elf::DT_NULL; ++dyn) {
            switch (dyn->d_tag) {
            case vk::elf::DT_RELA:
                out.rela_vaddr = dyn->d_val;
                break;
            case vk::elf::DT_RELASZ:
                out.rela_size = dyn->d_val;
                break;
            case vk::elf::DT_RELAENT:
                out.rela_ent = dyn->d_val;
                break;
            case vk::elf::DT_INIT_ARRAY:
                out.init_array_vaddr = dyn->d_val;
                break;
            case vk::elf::DT_INIT_ARRAYSZ:
                out.init_array_size = dyn->d_val;
                break;
            case vk::elf::DT_FINI_ARRAY:
                out.fini_array_vaddr = dyn->d_val;
                break;
            case vk::elf::DT_FINI_ARRAYSZ:
                out.fini_array_size = dyn->d_val;
                break;
            default:
                break;
            }
        }
        return true;
    }

    return true;
}

auto apply_relocations(unsigned char* /*image_base*/,
                       long long load_bias,
                       const dynamic_info& info) -> bool
{
    if (info.rela_vaddr == 0 || info.rela_size == 0) {
        return true;
    }
    if (info.rela_ent != sizeof(vk::elf::Elf64_Rela) || info.rela_size % info.rela_ent != 0) {
        return false;
    }

    const auto* rela = reinterpret_cast<const vk::elf::Elf64_Rela*>(
        reinterpret_cast<unsigned char*>(static_cast<long long>(info.rela_vaddr) + load_bias));
    const vk_usize count = static_cast<vk_usize>(info.rela_size / info.rela_ent);

    for (vk_usize index = 0; index < count; ++index) {
        if (vk::elf::elf64_r_type(rela[index].r_info) != vk::elf::R_X86_64_RELATIVE) {
            return false;
        }

        auto* location = reinterpret_cast<vk_u64*>(
            static_cast<long long>(rela[index].r_offset) + load_bias);
        *location = static_cast<vk_u64>(load_bias + rela[index].r_addend);
    }

    return true;
}

auto find_symbol_address(const dynsym_info& dynsym,
                         const char* symbol_name,
                         long long load_bias) -> vk_u64
{
    if (dynsym.symbols == nullptr || dynsym.strings == nullptr || symbol_name == nullptr) {
        return 0;
    }

    for (vk_usize index = 0; index < dynsym.count; ++index) {
        const auto& symbol = dynsym.symbols[index];
        if (symbol.st_name == 0 || symbol.st_shndx == vk::elf::SHN_UNDEF) {
            continue;
        }

        const char* candidate = dynsym.strings + symbol.st_name;
        if (strcmp(candidate, symbol_name) == 0) {
            return static_cast<vk_u64>(load_bias + static_cast<long long>(symbol.st_value));
        }
    }

    return 0;
}

void run_init_array(init_array_fn* functions, vk_usize count)
{
    if (functions == nullptr) {
        return;
    }

    for (vk_usize index = 0; index < count; ++index) {
        if (functions[index] != nullptr) {
            functions[index]();
        }
    }
}

void run_fini_array(init_array_fn* functions, vk_usize count)
{
    if (functions == nullptr) {
        return;
    }

    for (vk_usize index = count; index > 0; --index) {
        if (functions[index - 1] != nullptr) {
            functions[index - 1]();
        }
    }
}

} // namespace

DynamicPluginModule::~DynamicPluginModule()
{
    reset();
}

DynamicPluginModule::DynamicPluginModule(DynamicPluginModule&& other) noexcept
{
    *this = std::move(other);
}

auto DynamicPluginModule::operator=(DynamicPluginModule&& other) noexcept -> DynamicPluginModule&
{
    if (this == &other) {
        return *this;
    }

    reset();
    raw_allocation_ = other.raw_allocation_;
    image_base_ = other.image_base_;
    image_size_ = other.image_size_;
    fini_array_ = other.fini_array_;
    fini_count_ = other.fini_count_;
    descriptor_ = other.descriptor_;
    valid_ = other.valid_;

    other.raw_allocation_ = nullptr;
    other.image_base_ = nullptr;
    other.image_size_ = 0;
    other.fini_array_ = nullptr;
    other.fini_count_ = 0;
    other.descriptor_ = {};
    other.valid_ = false;
    return *this;
}

void DynamicPluginModule::reset() noexcept
{
    if (raw_allocation_ != nullptr) {
        VK_CALL(free, raw_allocation_);
    }

    raw_allocation_ = nullptr;
    image_base_ = nullptr;
    image_size_ = 0;
    fini_array_ = nullptr;
    fini_count_ = 0;
    descriptor_ = {};
    valid_ = false;
}

void DynamicPluginModule::run_fini_array() const
{
    ::vkgui::run_fini_array(fini_array_, fini_count_);
}

auto DynamicPluginModule::load(vk::string_view path, PluginHost& host) -> bool
{
    reset();

    std::string file_bytes;
    if (!read_file_bytes(path, file_bytes)) {
        host.log.addf("vkGUI plugin loader: failed to read %s.", string_from_view(path).c_str());
        return false;
    }

    if (file_bytes.size() < sizeof(vk::elf::Elf64_Ehdr)) {
        host.log.addf("vkGUI plugin loader: %s is too small.", string_from_view(path).c_str());
        return false;
    }

    const auto* file_data = reinterpret_cast<const unsigned char*>(file_bytes.data());
    const auto& ehdr = *reinterpret_cast<const vk::elf::Elf64_Ehdr*>(file_data);
    if (ehdr.e_ident[vk::elf::EI_MAG0] != vk::elf::ELFMAG0
        || ehdr.e_ident[vk::elf::EI_MAG1] != vk::elf::ELFMAG1
        || ehdr.e_ident[vk::elf::EI_MAG2] != vk::elf::ELFMAG2
        || ehdr.e_ident[vk::elf::EI_MAG3] != vk::elf::ELFMAG3
        || ehdr.e_ident[vk::elf::EI_CLASS] != vk::elf::ELFCLASS64
        || ehdr.e_ident[vk::elf::EI_DATA] != vk::elf::ELFDATA2LSB
        || ehdr.e_machine != vk::elf::EM_X86_64
        || ehdr.e_type != vk::elf::ET_DYN) {
        host.log.addf("vkGUI plugin loader: %s is not a supported x86_64 ET_DYN plugin.",
                      string_from_view(path).c_str());
        return false;
    }

    if (ehdr.e_phentsize != sizeof(vk::elf::Elf64_Phdr)
        || !range_ok(file_bytes.size(),
                     ehdr.e_phoff,
                     static_cast<vk_u64>(ehdr.e_phnum) * sizeof(vk::elf::Elf64_Phdr))) {
        host.log.addf("vkGUI plugin loader: %s has an invalid program header table.",
                      string_from_view(path).c_str());
        return false;
    }

    const auto* phdrs = reinterpret_cast<const vk::elf::Elf64_Phdr*>(file_data + ehdr.e_phoff);
    vk_u64 vaddr_min = ~0ULL;
    vk_u64 vaddr_max = 0;
    vk_u64 max_align = k_min_image_alignment;
    vk_u32 load_count = 0;

    for (u16 index = 0; index < ehdr.e_phnum; ++index) {
        const auto& phdr = phdrs[index];
        if (phdr.p_type != vk::elf::PT_LOAD) {
            continue;
        }
        if (!range_ok(file_bytes.size(), phdr.p_offset, phdr.p_filesz)) {
            host.log.addf("vkGUI plugin loader: %s has an out-of-range load segment.",
                          string_from_view(path).c_str());
            return false;
        }

        const vk_u64 seg_end = phdr.p_vaddr + phdr.p_memsz;
        if (phdr.p_vaddr < vaddr_min) {
            vaddr_min = phdr.p_vaddr;
        }
        if (seg_end > vaddr_max) {
            vaddr_max = seg_end;
        }
        if (phdr.p_align > max_align) {
            max_align = phdr.p_align;
        }
        ++load_count;
    }

    if (load_count == 0 || vaddr_min == ~0ULL || vaddr_max <= vaddr_min) {
        host.log.addf("vkGUI plugin loader: %s does not contain loadable segments.",
                      string_from_view(path).c_str());
        return false;
    }

    const vk_u64 image_bytes = align_up(vaddr_max - vaddr_min, k_min_image_alignment);
    raw_allocation_ = vk_malloc_executable(static_cast<vk_usize>(image_bytes + max_align));
    if (raw_allocation_ == nullptr) {
        host.log.addf("vkGUI plugin loader: executable allocation failed for %s.",
                      string_from_view(path).c_str());
        return false;
    }

    const auto raw_base = reinterpret_cast<vk_u64>(raw_allocation_);
    image_base_ = reinterpret_cast<unsigned char*>(align_up(raw_base, max_align));
    image_size_ = static_cast<vk_usize>(image_bytes);
    memset(image_base_, 0, image_size_);

    const long long load_bias =
        static_cast<long long>(reinterpret_cast<vk_u64>(image_base_))
        - static_cast<long long>(vaddr_min);

    for (u16 index = 0; index < ehdr.e_phnum; ++index) {
        const auto& phdr = phdrs[index];
        if (phdr.p_type != vk::elf::PT_LOAD) {
            continue;
        }

        auto* dest = reinterpret_cast<unsigned char*>(static_cast<long long>(phdr.p_vaddr) + load_bias);
        if (phdr.p_filesz != 0) {
            memcpy(dest, file_data + phdr.p_offset, static_cast<size_t>(phdr.p_filesz));
        }
        if (phdr.p_memsz > phdr.p_filesz) {
            memset(dest + phdr.p_filesz, 0, static_cast<size_t>(phdr.p_memsz - phdr.p_filesz));
        }
    }

    dynamic_info dyn {};
    if (!resolve_dynamic_info(ehdr, phdrs, image_base_, load_bias, dyn) || !apply_relocations(image_base_, load_bias, dyn)) {
        host.log.addf("vkGUI plugin loader: unsupported relocations in %s.",
                      string_from_view(path).c_str());
        reset();
        return false;
    }

    dynsym_info dynsym {};
    if (!collect_dynsym_info(file_data, file_bytes.size(), ehdr, dynsym)) {
        host.log.addf("vkGUI plugin loader: %s is missing a usable dynamic symbol table.",
                      string_from_view(path).c_str());
        reset();
        return false;
    }

    if (dyn.init_array_vaddr != 0 && dyn.init_array_size != 0) {
        auto* init_array = reinterpret_cast<init_array_fn*>(
            reinterpret_cast<unsigned char*>(static_cast<long long>(dyn.init_array_vaddr) + load_bias));
        run_init_array(init_array, static_cast<vk_usize>(dyn.init_array_size / sizeof(init_array_fn)));
    }

    const vk_u64 init_address = find_symbol_address(dynsym, k_plugin_init_symbol, load_bias);
    if (init_address == 0) {
        host.log.addf("vkGUI plugin loader: %s does not export %s.",
                      string_from_view(path).c_str(),
                      k_plugin_init_symbol);
        reset();
        return false;
    }

    const auto init = reinterpret_cast<vkgui_plugin_init_fn>(init_address);
    set_active_plugin_host(&host);
    const int init_ok = init(&plugin_host_api(), &descriptor_);
    clear_active_plugin_host();

    if (init_ok == 0
        || descriptor_.abi_version != VKGUI_PLUGIN_ABI_VERSION
        || descriptor_.id == nullptr
        || descriptor_.menu_label == nullptr
        || descriptor_.draw_window == nullptr) {
        host.log.addf("vkGUI plugin loader: %s rejected the plugin ABI handshake.",
                      string_from_view(path).c_str());
        reset();
        return false;
    }

    if (dyn.fini_array_vaddr != 0 && dyn.fini_array_size != 0) {
        fini_array_ = reinterpret_cast<init_array_fn*>(
            reinterpret_cast<unsigned char*>(static_cast<long long>(dyn.fini_array_vaddr) + load_bias));
        fini_count_ = static_cast<vk_usize>(dyn.fini_array_size / sizeof(init_array_fn));
    }

    valid_ = true;
    host.log.addf("vkGUI plugin loader: loaded %s from %s.",
                  descriptor_.menu_label,
                  string_from_view(path).c_str());
    return true;
}

void DynamicPluginModule::unload(PluginHost& host)
{
    if (!valid_) {
        reset();
        return;
    }

    set_active_plugin_host(&host);
    if (descriptor_.shutdown != nullptr) {
        descriptor_.shutdown(descriptor_.user_data);
    }
    clear_active_plugin_host();

    run_fini_array();
    reset();
}

void DynamicPluginModule::draw_window(PluginHost& host, bool& visible)
{
    if (!valid_) {
        visible = false;
        return;
    }

    vk_u32 visible_value = visible ? 1u : 0u;
    set_active_plugin_host(&host);
    descriptor_.draw_window(&plugin_host_api(), &visible_value, descriptor_.user_data);
    clear_active_plugin_host();
    visible = visible_value != 0u;
}

} // namespace vkgui
