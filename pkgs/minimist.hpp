#pragma once
#include <map>
#include <string>
#include <optional>
#include <variant>
#include <functional>
#include <type_traits>
#include <nlohmann/json.hpp>

class MinimistOpts {
public:
    std::optional<std::variant<std::string, std::vector<std::string>>> string;
    std::optional<std::variant<bool, std::string, std::vector<std::string>>> boolean;
    std::optional<std::map<std::string, std::variant<std::string, std::vector<std::string>>>> alias;
    std::optional<std::map<std::string, std::variant<std::string, int, bool>>> def;
    bool stopEarly = false;
    bool dashdash = false;
    std::function<bool(std::string)> *unknown = nullptr;
};

nlohmann::json minimist(std::vector<std::string> args, std::optional<MinimistOpts> _opts);