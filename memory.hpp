#pragma once

#include <format>
#include <memory>
#include <string>
#include <ranges>
#include <functional>
#include <MinHook.h>

#include <Windows.h>
#include <Psapi.h>
#include <minwindef.h>
#include <stdint.h>
#include <vadefs.h>
#include <vcruntime.h>
#include <exception>
#include <stdexcept>
#include <vector>
#include <assert.h>

#pragma comment (lib,"psapi.lib")

namespace memory {
    struct MemoryScanner;
    struct MemoryModule;

    template<class T = uint8_t>
    struct Address {
        uintptr_t address;

        Address() {}

        template<class U = T> requires (!std::is_pointer<T>::value)
        Address(U* addr) : address((uintptr_t)addr) {}

        template<class U = T> requires (std::is_pointer<T>::value)
        Address(U addr) : address((uintptr_t)addr) {}

        Address(uintptr_t addr) : address(addr) {}
        Address(uintptr_t addr, size_t offset, size_t instruction_size) 
            : address(convert_to_absolute(addr, offset, instruction_size)) {}

        template<class U>
        Address(const Address<U>& addr) : address(addr.address) {}

        template<class U>
        Address(Address<U>&& addr) : address(addr.address) {} 

        Address& operator=(const Address& mod) {
            this->address = mod.address;
            return *this;
        }

        template<class U = T> requires ( not std::is_pointer<T>::value )
        T* get_pointer() const { 
            return reinterpret_cast<T*>(address); 
        }

        template<class U>
        Address<U> as() const {
            return Address<U>(*this);
        }

        template<class U = T> requires ( std::is_pointer<T>::value )
        T get_pointer() const { 
            return reinterpret_cast<T>(address); 
        }

        template<class U = T> requires ( std::is_pointer<T>::value )
        U& get_ref() const {
            return *get_pointer();
        }

        template<class ...Args> requires ( std::is_invocable_v<T, Args...> )
        auto invoke(Args&& ...args) {
            auto pointer = get_pointer();
            return std::invoke(pointer, args...);
        }

        template<class ReturnType, class ...Args> requires ( std::is_invocable_r_v<ReturnType, T, Args...> )
        auto invoke_r(Args&& ...args) {
            auto pointer = get_pointer();
            return std::invoke_r<ReturnType>(pointer, args...);
        }

        #ifdef _WIN64
        Address<T>& absolute(ptrdiff_t offset, size_t instruction_size) {
            address = convert_to_absolute(address, offset, instruction_size);
            return *this;
        }
        #else
        Address<T>& absolute(ptrdiff_t offset) {
            address = convert_to_absolute(address, offset, 0x0);
            return *this;
        }
        #endif

        Address<T> add(ptrdiff_t offset) {
            return address + offset;
        }

        static auto convert_to_absolute(uintptr_t address, ptrdiff_t offset, size_t instruction_size) {
		#ifdef _WIN64
            return address + instruction_size + (ptrdiff_t)(*(int*)(address + offset));
		#else
            auto rel = *(ptrdiff_t*)(address + offset);
            return address + offset + sizeof(std::uintptr_t) + rel;
		#endif
        }

        template<class ModuleType>
        static auto get_symbol(std::string_view sym, const ModuleType& mod) {
            auto ptr = GetProcAddress(static_cast<HMODULE>(mod), sym.data());
            return Address<T>(ptr);
        }
    };

    
    struct MemoryScanner {
        MemoryScanner(std::string_view pattern) : pattern(pattern) {}

        template <class T>
        Address<T*> scan(const auto& range) const {
            static_assert(std::ranges::range<decltype(range)>);
            return scan_impl(range.begin(), range.end());
        }

        template <class T>
        Address<T*> scan(uint8_t* begin, uint8_t* end) const {
            return scan_impl(begin, end);
        }
    private:
        std::string pattern;

        Address<> scan_impl(uint8_t* begin, uint8_t* end) const;
    };

    struct MemoryModule {
        HMODULE handle;

        MemoryModule(std::string_view module_name) {
            handle = get_module_by_name(module_name).handle;
        }
        MemoryModule(uintptr_t addr) : handle((HMODULE)addr) {}
        MemoryModule(HMODULE addr) : handle(addr) {}
        MemoryModule(MemoryModule&& mod) : handle(mod.handle) {}
        MemoryModule(const MemoryModule& mod) : handle(mod.handle) {}

        MemoryModule& operator=(const MemoryModule& mod) {
            this->handle = mod.handle;
            return *this;
        }

        operator HMODULE() const {
            return handle;
        }

        template<class T>
        Address<T> get_symbol(std::string_view name) {
            auto handle = GetProcAddress(*this, name.data());
            return Address<T>(reinterpret_cast<uintptr_t>(handle)); 
        }

        auto get_dos_headers() const {
            return reinterpret_cast<PIMAGE_DOS_HEADER>(get_base().get_pointer());
        }

        auto get_nt_headers() const {
            return reinterpret_cast<PIMAGE_NT_HEADERS>(
                (uint8_t*)(get_base().address) + get_dos_headers()->e_lfanew
            );
        }

        auto get_module_info() const {
            MODULEINFO info;
            GetModuleInformation(GetCurrentProcess(), handle, &info, sizeof(MODULEINFO));
            return info;
        }

        Address<> get_base() const {
            return {reinterpret_cast<uintptr_t>(get_module_info().lpBaseOfDll)};
        }

        std::uint8_t* begin() const {
            return get_base().get_pointer();
        }

        std::uint8_t* end() const {
            return reinterpret_cast<std::uint8_t*>(
                get_base().address + (ptrdiff_t)(get_nt_headers()->OptionalHeader.SizeOfImage)
            );
        }

        static MemoryModule get_module_by_name(std::string_view name);
    };

    namespace hook_methods {
        template<typename T, size_t index>
        struct VMTHook {
            T detour;
            T original = nullptr;

            auto hook(uintptr_t* vmt) {
                original = reinterpret_cast<T>(vmt[index]);

                DWORD protection;
                VirtualProtect((LPVOID)&vmt[index], sizeof(uintptr_t), PAGE_EXECUTE_READWRITE, &protection);
                vmt[index] = reinterpret_cast<uintptr_t>(detour);
                VirtualProtect((LPVOID)&vmt[index], sizeof(uintptr_t), protection, &protection);
            }

            auto unhook(uintptr_t* vmt) {
                vmt[index] = reinterpret_cast<uintptr_t>(original);
            }
        };

        template<typename T>
        struct MinHook {
            LPVOID detour;
            T original = nullptr;

            auto hook(uintptr_t target) {
                auto status = MH_CreateHook((LPVOID)target, (LPVOID)detour, (LPVOID*)&original);
                if (status != MH_OK) {
                    throw std::runtime_error("Failed to create hook");
                }
                MH_EnableHook((LPVOID)target);
            }

            auto unhook() {
                assert(original != nullptr);
                MH_DisableHook((LPVOID)original);
                MH_RemoveHook((LPVOID)original);
            }
        };
    }
}