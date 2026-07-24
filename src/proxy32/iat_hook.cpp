#include "iat_hook.h"

#include <cstdint>
#include <cstring>

namespace fearvr {
namespace {

bool IsImageRangeValid(std::uint32_t rva, std::size_t bytes,
                       std::uint32_t imageSize) noexcept {
    return rva < imageSize && bytes <= imageSize - rva;
}

} // namespace

BOOL InstallDirect3DCreate9IatHook(void* replacement) noexcept {
    static_assert(sizeof(void*) == sizeof(std::uint32_t),
                  "M2 IAT hook is x86-only");
    if (replacement == nullptr) {
        return FALSE;
    }

    auto* base = reinterpret_cast<std::uint8_t*>(
        GetModuleHandleW(nullptr));
    if (base == nullptr) {
        return FALSE;
    }
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) {
        return FALSE;
    }
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(
        base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        return FALSE;
    }

    const std::uint32_t imageSize = nt->OptionalHeader.SizeOfImage;
    const IMAGE_DATA_DIRECTORY& directory =
        nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (directory.VirtualAddress == 0 ||
        !IsImageRangeValid(directory.VirtualAddress,
                           sizeof(IMAGE_IMPORT_DESCRIPTOR), imageSize)) {
        return FALSE;
    }

    auto* descriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(
        base + directory.VirtualAddress);
    for (; descriptor->Name != 0; ++descriptor) {
        if (!IsImageRangeValid(descriptor->Name, 1, imageSize)) {
            return FALSE;
        }
        const char* moduleName =
            reinterpret_cast<const char*>(base + descriptor->Name);
        if (_stricmp(moduleName, "d3d9.dll") != 0) {
            continue;
        }
        if (descriptor->OriginalFirstThunk == 0 ||
            descriptor->FirstThunk == 0) {
            return FALSE;
        }

        auto* names = reinterpret_cast<IMAGE_THUNK_DATA32*>(
            base + descriptor->OriginalFirstThunk);
        auto* iat = reinterpret_cast<IMAGE_THUNK_DATA32*>(
            base + descriptor->FirstThunk);
        for (; names->u1.AddressOfData != 0; ++names, ++iat) {
            if (IMAGE_SNAP_BY_ORDINAL32(names->u1.Ordinal)) {
                continue;
            }
            const std::uint32_t nameRva = names->u1.AddressOfData;
            if (!IsImageRangeValid(nameRva,
                                   sizeof(IMAGE_IMPORT_BY_NAME),
                                   imageSize)) {
                return FALSE;
            }
            const auto* imported =
                reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(
                    base + nameRva);
            if (std::strcmp(
                    reinterpret_cast<const char*>(imported->Name),
                    "Direct3DCreate9") != 0) {
                continue;
            }

            DWORD oldProtection = 0;
            if (!VirtualProtect(&iat->u1.Function,
                                sizeof(iat->u1.Function),
                                PAGE_READWRITE, &oldProtection)) {
                return FALSE;
            }
            InterlockedExchangePointer(
                reinterpret_cast<void* volatile*>(&iat->u1.Function),
                replacement);
            DWORD ignored = 0;
            VirtualProtect(&iat->u1.Function, sizeof(iat->u1.Function),
                           oldProtection, &ignored);
            FlushInstructionCache(GetCurrentProcess(), &iat->u1.Function,
                                  sizeof(iat->u1.Function));
            return TRUE;
        }
        return FALSE;
    }
    return FALSE;
}

} // namespace fearvr
