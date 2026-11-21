#include "domain.hpp"

namespace flipkiln {
namespace {
bool RuleMatches(const Json& rule, const Json& context, std::string_view project,
                 std::string_view environment, std::string_view key) {
    const auto kind = rule.value("kind", "");
    if (kind == "attribute") {
        const auto name = rule.value("attribute", "");
        const auto attributes = context.value("attributes", Json::object());
        return attributes.contains(name) && attributes.at(name) == rule.at("equals");
    }
    if (!context.contains("id")) {
        return false;
    }
    const auto id = context.at("id").get<std::string_view>();
    if (kind == "ids") {
        for (const auto& item : rule.at("ids")) {
            if (item == id) {
                return true;
            }
        }
        return false;
    }
    return kind == "rollout" && Bucket(project, environment, key, rule.at("id").get<std::string_view>(), id) <
                                    rule.at("percentage").get<unsigned>() * 100;
}
}

std::uint64_t Bucket(std::string_view project, std::string_view environment, std::string_view key,
                     std::string_view rule, std::string_view id) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (const auto part : {project, environment, key, rule}) {
        for (const unsigned char byte : part) {
            hash = (hash ^ byte) * 1099511628211ULL;
        }
        hash = (hash ^ '\n') * 1099511628211ULL;
    }
    for (const unsigned char byte : id) {
        hash = (hash ^ byte) * 1099511628211ULL;
    }
    return hash % 10000;
}

Json Evaluate(const Json& entries, const Json& keys, const Json& context, std::string_view project,
              std::string_view environment) {
    auto results = Json::array();
    for (const auto& requested : keys) {
        const auto key =
            requested.is_string() ? requested.get<std::string>() : requested.at("key").get<std::string>();
        auto result = Json{{"key", key}, {"reason", "unknown_key"}};
        for (const auto& entry : entries) {
            if (entry.at("key") != key) {
                continue;
            }
            if (requested.is_object() && requested.at("type") != entry.at("type")) {
                result["reason"] = "type_mismatch";
                break;
            }
            result["value"] = entry.at("defaultValue");
            result["reason"] = entry.at("enabled") == true ? "default" : "disabled";
            if (entry.at("enabled") == true) {
                for (const auto& rule : entry.at("rules")) {
                    if (!RuleMatches(rule, context, project, environment, key)) {
                        continue;
                    }
                    result["value"] = rule.at("value");
                    result["reason"] = "rule_match";
                    result["ruleId"] = rule.at("id");
                    break;
                }
            }
            break;
        }
        results.push_back(std::move(result));
    }
    return results;
}
}
