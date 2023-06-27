#include "MinHook.h"
#include <Windows.h>
#include <chrono>
#include <string_view>
#include <thread>
#include <vector>
#include <iostream>
#include <ranges>
#include <winuser.h>
#include <xutility>

#define CR_HOST
#include <cr.h>

#include "haxstate.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "../kiero.h"

static HaxState haxState;
static cr_plugin ctx;

void entry_point(HMODULE _mod) {
    AllocConsole();
	freopen("CONOUT$", "wb", stdout);
	freopen("CONOUT$", "wb", stderr);
	freopen("CONIN$", "rb", stdin);

    haxState.logger = spdlog::stderr_color_mt("console");
    spdlog::set_default_logger(haxState.logger);

    std::thread([&]() {
        while (true) {
            std::string input;
            std::getline(std::cin, input);

            auto args = std::views::split(input, ' ') 
                    | std::views::transform([](auto&& arg) { return std::string_view(arg.begin(), arg.end()); })
                    | std::ranges::to<std::vector>();

            if (haxState.commands.contains(args[0].data())) {
                haxState.commands[args[0].data()](args | std::views::drop(1));
            }
        } 
    }).detach();

    ctx.userdata = &haxState;
    bool flag = cr_plugin_open(ctx, BINARY_PATH "/hoi4_hax.dll");
    while (!flag) {
        std::cout << "Failed to load HAX DLL" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        flag = cr_plugin_open(ctx, BINARY_PATH "/hoi4_hax.dll");
    }
    
    while (!cr_plugin_update(ctx)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }

    cr_plugin_close(ctx);
}

bool __stdcall DllMain(
    HMODULE mod,
    DWORD ul_reason_for_call,
    LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        std::thread(entry_point, mod).detach();
    }
    return TRUE;
}