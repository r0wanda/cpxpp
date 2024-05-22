#pragma once
#include <regex>
#include <string>
#include <vector>
#include <variant>
#include <functional>
#include <nlohmann/json.hpp>

void initToken();
typedef std::variant<nlohmann::json, std::function<std::string(std::string)>, std::function<nlohmann::json(std::string)>> env_t;
std::variant<nlohmann::json, std::vector<std::smatch>> parseShq(std::string s, env_t *env, std::optional<nlohmann::json> opts);