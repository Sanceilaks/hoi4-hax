#include <charconv>
#include <cstddef>
#include <format>
#include <ios>
#include <iostream>
#include <Windows.h>
#include <minwindef.h>
#include <stdint.h>
#include <string>
#include <string_view>
#include <thread>
#include <ranges>
#include <vadefs.h>
#include <vcruntime.h>
#include <vector>

#include "MinHook.h"
#include "imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "memory.hpp"
#include "kiero.h"
#include "render.hpp"

#include <cr.h>
#include "loader/haxstate.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/fmt/bin_to_hex.h>

struct Manpower;
struct Country;
struct GameState;
struct Application;

class HoiConsoleCommand
{
public:
	int64_t isavil; //0x0000
	char name[64]; //0x0008
	char *desc; //0x0048
	char pad_0050[24]; //0x0050
	void* handler; //0x0068
	char pad_0070[72]; //0x0070
}; //Size: 0x00B8

memory::MemoryModule hoi("hoi4.exe");

memory::Address<__int64(__fastcall*)(__int64 self)> get_current_country;
memory::Address<void(__fastcall*)(__int64, int num, bool unk)> add_manpower;
memory::Address<HoiConsoleCommand*(__fastcall*)(__int64 state, __int64 name)> get_command_by_name;
memory::Address<void(__fastcall*)(__int64, int num)> add_stability;
memory::Address<void(__fastcall*)(__int64, uint32_t num)> add_political_power;

memory::hook_methods::MinHook<bool(__fastcall*)(__int64, __int64)> sub_1402C7830_hook;
bool __fastcall sub_1402C7830_hooked(__int64 self, __int64 a2) {
    sub_1402C7830_hook.original(self, a2);
    return true;
}

auto get_application() -> uintptr_t {
    return *(uintptr_t*)memory::MemoryScanner("48 8B 0D ? ? ? ? 8B 86 ? ? ? ?")
        .scan<uintptr_t>(hoi).absolute(0x3, 0x3 + 0x4).get_pointer();
}

auto get_game_state() {
    auto app = get_application();

    auto state = app + 0xE40;
    if (*(int*)state <= 0) {
        state = app + 0xE44;
    }
    return state;
}

auto get_console_state() {
    return *(uintptr_t*)memory::MemoryScanner("48 8B 0D ? ? ? ? 48 85 C9 75 0C E8 ? ? ? ? 48 8B 0D ? ? ? ? 4C 8D 45 E7")
        .scan<uintptr_t>(hoi).absolute(0x3, 0x3 + 0x4).get_pointer();
}

auto get_political_status(__int64 country) {
    return *(uintptr_t*)(country + 0xF28);
}

namespace hack {
    void manpower(int num) {
        auto game_state = get_game_state();
        auto current_country = get_current_country.invoke_r<__int64>(game_state);
        auto manpowerobj = ((uintptr_t)current_country) + 0x318;

        add_manpower.invoke(manpowerobj, num, 0);

        spdlog::info("Added manpower: {}", num);
    }

    void stability(int num) {
        auto game_state = get_game_state();
        auto current_country = get_current_country.invoke_r<__int64>(game_state);
        add_stability.invoke(current_country, num);

        spdlog::info("Added stability: {}", num);
    }

    void political_power(uint32_t num) {
        auto game_state = get_game_state();
        auto current_country = get_current_country.invoke_r<__int64>(game_state);
        auto political_status = get_political_status(current_country);
        add_political_power.invoke(political_status, num);

        spdlog::info("Added political power: {}", num);
    }

    void command_power(uint32_t num) {
        auto game_state = get_game_state();
        auto current_country = get_current_country.invoke_r<__int64>(game_state);
        *(int*)(current_country + 0x1CC) += num;

        spdlog::info("Added command power: {}", num);
    }

    HoiConsoleCommand* get_command(std::string_view name) {
        struct hoi4_string {
            std::byte data[128];
        };
        
        static memory::Address<__int64(__fastcall*)(__int64, const char*)> construct_hoi4_string 
            = memory::MemoryScanner("E8 ? ? ? ? 8B 55 93").scan<void*>(hoi).absolute(0x1, 0x5);

        auto state = get_console_state();
        
        hoi4_string str;
        construct_hoi4_string.invoke((__int64)&str, name.data());

        return get_command_by_name.invoke_r<HoiConsoleCommand*>(state, (__int64)&str);
    }
}

void draw_menu() {
    ImGui::Begin("AOSHax Hoi");

    static int manpower_to_add = 0;
    ImGui::InputInt("Manpower", &manpower_to_add);
    ImGui::SameLine();
    if (ImGui::Button("Add##MANPOWER")) {
        hack::manpower(manpower_to_add);
    }

    static float stab_to_add = 0;
    ImGui::InputFloat("Stability", &stab_to_add);
    ImGui::SameLine();
    if (ImGui::Button("Add##STAB")) {
        hack::stability(std::round(stab_to_add * 1000.f));
    }

    static float plit_to_add = 0;
    ImGui::InputFloat("PP", &plit_to_add);
    ImGui::SameLine();
    if (ImGui::Button("Add##PP")) {
        hack::political_power(std::round(plit_to_add * 1000.f));
    }

    static float cp_to_add = 0;
    ImGui::InputFloat("Command Power", &cp_to_add);
    ImGui::SameLine();
    if (ImGui::Button("Add##CO")) {
        hack::command_power(std::round(cp_to_add * 1000.f));
    }

    auto get_command_fn = [](std::string_view name) {
        static std::map<std::string, memory::Address<>> addresses;
        if (!addresses.contains(name.data())){
            addresses[name.data()] = hack::get_command(name)->handler;
        }
        return addresses[name.data()];
    };

    ImGui::Checkbox("nocb", (bool*)(hoi.get_base().address + 0x24C7437 + 0x2000));
    ImGui::Checkbox("debug_nuking", (bool*)(hoi.get_base().address + 0x24C943A));
    ImGui::Checkbox("ic", (bool*)(get_command_fn("ic").add(0x1A).absolute(0x2, 0x7).address));
    ImGui::Checkbox("research_on_icon_click", (bool*)(hoi.get_base().address + 0x24C7436 + 0x2000));
    ImGui::Checkbox("yesman", (bool*)(hoi.get_base().address + 0x24C7439 + 0x2000));   

    static std::string command_info, command_name;
    if (ImGui::Button("Get Command By Name") && !command_name.empty()) {
        auto command = hack::get_command(command_name);
        if (command) {
            command_info = std::format("Found command: \n\tname: {}\n\tdesc: {}\n\thandler: {:x}", 
                command->name, command->desc, (uintptr_t)command->handler);
            spdlog::info("{}", command_info);
        }
    }

    ImGui::SameLine();
    ImGui::InputText("Command", &command_name);

    ImGui::Text("%s", command_info.c_str());

    ImGui::End();
}

int on_load(HaxState* state) {
    spdlog::set_default_logger(state->logger);

    spdlog::info("Loading Hax");

    MH_Initialize();

    sub_1402C7830_hook.detour = sub_1402C7830_hooked;

    sub_1402C7830_hook.hook(
        memory::MemoryScanner("48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC 50 48 8B DA 48 8B F1 48 8D 4A 10 E8 ? ? ? ? 48 8B F8 4C 8B 06")
            .scan<uint8_t>(hoi).address
    );
    render::hook();
    MH_EnableHook(MH_ALL_HOOKS);

    get_current_country = memory::MemoryScanner("E8 ? ? ? ? 48 8D 88 ? ? ? ? 45 33 C0 8B D3").scan<void*>(hoi)
        .absolute(0x1, 0x5);
    add_manpower = memory::MemoryScanner("E8 ? ? ? ? 85 DB 7E 61").scan<void*>(hoi)
        .absolute(0x1, 0x5);
    get_command_by_name = memory::MemoryScanner("E8 ? ? ? ? 48 85 C0 40 0F 94 C6 ").scan<void*>(hoi)
        .absolute(0x1, 0x5);
    add_stability = memory::MemoryScanner("E8 ? ? ? ? 90 C7 45 ? ? ? ? ? 48 8B 4D 90 E8 ? ? ? ? 90 48 81 C4 ? ? ? ?").scan<void*>(hoi)
        .absolute(0x1, 0x5);
    add_political_power = memory::MemoryScanner("E8 ? ? ? ? 39 5E 30").scan<void*>(hoi)
        .absolute(0x1, 0x5);

    state->commands["manpower"] = [&](auto args) {
        if (args.empty()) {
            std::cout << "No arguments" << std::endl;
            return;
        }

        auto num = 0;
        auto result = std::from_chars(args[0].data(), args[0].data() + args[0].size(), num);
        if(result.ec == std::errc::invalid_argument || result.ec == std::errc::result_out_of_range) {
            std::cout << "Cannot convert second argument to number" << std::endl;
            return;
        }

        hack::manpower(num);
    };

    state->commands["st"] = [&](auto args) {
        if (args.empty()) {
            std::cout << "No arguments" << std::endl;
            return;
        }

        auto num = 0;
        auto result = std::from_chars(args[0].data(), args[0].data() + args[0].size(), num);
        if(result.ec == std::errc::invalid_argument || result.ec == std::errc::result_out_of_range) {
            std::cout << "Cannot convert second argument to number" << std::endl;
            return;
        }

        hack::stability(num);
    };

    return 0;
}

auto unload_impl() {
    render::unhook();

    MH_DisableHook(MH_ALL_HOOKS);
    MH_RemoveHook(MH_ALL_HOOKS);

    MH_Uninitialize();
} 

int on_unload(HaxState* state) {
    spdlog::info("Unloading Hax");

    unload_impl();

    state->commands.clear();

    return 0;
}

CR_EXPORT int cr_main(struct cr_plugin* ctx, enum cr_op operation) {
    assert(ctx);
    switch (operation) {
        case CR_LOAD: return on_load(reinterpret_cast<HaxState*>(ctx->userdata));
        case CR_UNLOAD: return on_unload(reinterpret_cast<HaxState*>(ctx->userdata));
        case CR_CLOSE: return on_unload(reinterpret_cast<HaxState*>(ctx->userdata));
    }
    return 0;
}