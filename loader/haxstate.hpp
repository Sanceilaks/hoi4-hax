#pragma once

#include <functional>
#include <map>
#include <memory>
#include <spdlog/logger.h>
#include <string_view>
#include <vector>
#include <string>
#include <span>

#include <spdlog/spdlog.h>

struct HaxState {
    using CommandHandler = std::function<void(std::span<std::string_view> args)>;
    using Commands = std::map<std::string, CommandHandler>;

    Commands commands;
    std::shared_ptr<spdlog::logger> logger;
};