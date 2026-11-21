#include <cmath>
#include <set>
#include <string>

#include "domain.hpp"

namespace flipkiln {
namespace {
bool String(const Json& value, std::size_t minimum, std::size_t maximum) {
    return value.is_string() && value.get_ref<const std::string&>().size() >= minimum &&
           value.get_ref<const std::string&>().size() <= maximum &&
           value.get_ref<const std::string&>().find('\0') == std::string::npos;
}

bool Scalar(const Json& value) {
    return value.is_boolean() || (value.is_number() && std::isfinite(value.get<double>())) ||
           String(value, 0, 512) || value.is_null();
}

bool MatchesType(const Json& value, std::string_view type) {
    if (type == "boolean") {
        return value.is_boolean();
    }
    if (type == "string") {
        return String(value, 0, 8192);
    }
    if (type == "number") {
        return value.is_number() && std::isfinite(value.get<double>());
    }
    if (type == "integer") {
        return value.is_number() && std::isfinite(value.get<double>()) &&
               std::trunc(value.get<double>()) == value.get<double>();
    }
    if (type == "null") {
        return value.is_null();
    }
    if (type == "array") {
        return value.is_array();
    }
    if (type == "object") {
        return value.is_object();
    }
    return type == "json";
}

void Issue(Json& issues, const std::string& path, std::string_view message) {
    if (issues.size() < 100) {
        issues.push_back({{"path", path}, {"message", message}});
    }
}

std::string PointerToken(std::string_view token) {
    std::string escaped;
    for (char c : token) {
        if (c == '~') {
            escaped += "~0";
        } else if (c == '/') {
            escaped += "~1";
        } else {
            escaped += c;
        }
    }
    return escaped;
}

void CheckJsonStorage(const Json& data, const std::string& path, Json& issues) {
    if (issues.size() >= 100) {
        return;
    }
    if (data.is_string() && data.get_ref<const std::string&>().find('\0') != std::string::npos) {
        Issue(issues, path, "U+0000 cannot be stored in a configuration.");
    } else if (data.is_number()) {
        constexpr double kMaxExactInteger = 9007199254740991.0;
        const auto value = data.get<double>();
        if (!std::isfinite(value) || (std::trunc(value) == value && std::abs(value) > kMaxExactInteger)) {
            Issue(issues, path,
                  "Use a finite number with integer values between -9007199254740991 and 9007199254740991.");
        }
    } else if (data.is_object()) {
        for (const auto& [key, value] : data.items()) {
            if (key.find('\0') != std::string::npos) {
                Issue(issues, path, "A property name contains U+0000.");
            } else {
                CheckJsonStorage(value, path + "/" + PointerToken(key), issues);
            }
        }
    } else if (data.is_array()) {
        for (std::size_t i = 0; i < data.size(); ++i) {
            CheckJsonStorage(data.at(i), path + "/" + std::to_string(i), issues);
        }
    }
}

void CheckSchema(const Json& schema, const std::string& path, Json& issues, int depth) {
    if (depth > 12 || !schema.is_object()) {
        Issue(issues, path, "Expected a schema object with at most 12 levels of nesting.");
        return;
    }
    const std::set<std::string_view> types{"boolean", "string", "integer", "number",
                                           "null",    "array",  "object"};
    for (const auto& [key, value] : schema.items()) {
        const auto field = path + "/" + PointerToken(key);
        if (key == "$schema") {
            if (depth != 0 || value != "http://json-schema.org/draft-07/schema#") {
                Issue(issues, field, "Use the Draft 7 dialect URI at the schema root.");
            }
        } else if (key == "type") {
            if (!value.is_string() || !types.contains(value.get_ref<const std::string&>())) {
                Issue(issues, field, "Expected a single Draft 7 type.");
            }
        } else if (key == "properties") {
            if (!value.is_object()) {
                Issue(issues, field, "Expected a property-to-schema map.");
            } else {
                for (const auto& [property, child] : value.items()) {
                    CheckSchema(child, field + "/" + PointerToken(property), issues, depth + 1);
                }
            }
        } else if (key == "required") {
            std::set<std::string> names;
            if (!value.is_array()) {
                Issue(issues, field, "Expected an array of property names.");
            } else {
                for (const auto& name : value) {
                    if (!String(name, 0, 512) || !names.insert(name.get<std::string>()).second) {
                        Issue(issues, field, "Property names must be unique strings.");
                    }
                }
            }
        } else if (key == "additionalProperties") {
            if (!value.is_boolean()) {
                Issue(issues, field, "Expected a boolean.");
            }
        } else if (key == "items") {
            CheckSchema(value, field, issues, depth + 1);
        } else if (key == "minimum" || key == "maximum") {
            if (!MatchesType(value, "number")) {
                Issue(issues, field, "Expected a finite number.");
            }
        } else if (key == "minLength" || key == "maxLength" || key == "minItems" || key == "maxItems") {
            if (!MatchesType(value, "integer") || value.get<double>() < 0) {
                Issue(issues, field, "Expected a nonnegative integer.");
            }
        } else if (key == "enum") {
            if (!value.is_array() || value.empty()) {
                Issue(issues, field, "Expected a nonempty array.");
            }
        } else if (key != "const") {
            Issue(issues, field, "This JSON Schema keyword is not supported.");
        }
    }
}

std::size_t UnicodeLength(const std::string& text) {
    std::size_t length = 0;
    for (unsigned char byte : text) {
        if ((byte & 0xc0) != 0x80) {
            ++length;
        }
    }
    return length;
}

void CheckValue(const Json& value, const Json& schema, const std::string& path, Json& issues) {
    if (schema.contains("type") && !MatchesType(value, schema.at("type").get<std::string_view>())) {
        Issue(issues, path, "Value does not match the schema type.");
        return;
    }
    if (schema.contains("const") && value != schema.at("const")) {
        Issue(issues, path, "Value differs from the schema constant.");
    }
    if (schema.contains("enum")) {
        bool found = false;
        for (const auto& choice : schema.at("enum")) {
            found |= choice == value;
        }
        if (!found) {
            Issue(issues, path, "Value is outside the schema's allowed choices.");
        }
    }
    if (value.is_object()) {
        if (schema.contains("required")) {
            for (const auto& key : schema.at("required")) {
                const auto name = key.get<std::string>();
                if (!value.contains(name)) {
                    Issue(issues, path + "/" + PointerToken(name), "Required property is missing.");
                }
            }
        }
        const auto properties = schema.value("properties", Json::object());
        for (const auto& [name, property] : value.items()) {
            const auto child = path + "/" + PointerToken(name);
            if (properties.contains(name)) {
                CheckValue(property, properties.at(name), child, issues);
            } else if (!schema.value("additionalProperties", true)) {
                Issue(issues, child, "Unexpected property.");
            }
        }
    }
    if (value.is_number()) {
        if (schema.contains("minimum") && value < schema.at("minimum")) {
            Issue(issues, path, "Value is below the minimum.");
        }
        if (schema.contains("maximum") && value > schema.at("maximum")) {
            Issue(issues, path, "Value is above the maximum.");
        }
    }
    if (value.is_string()) {
        const auto length = UnicodeLength(value.get_ref<const std::string&>());
        if (schema.contains("minLength") && length < schema.at("minLength").get<double>()) {
            Issue(issues, path, "String is too short.");
        }
        if (schema.contains("maxLength") && length > schema.at("maxLength").get<double>()) {
            Issue(issues, path, "String is too long.");
        }
    }
    if (value.is_array()) {
        if (schema.contains("minItems") && value.size() < schema.at("minItems").get<double>()) {
            Issue(issues, path, "Array has too few items.");
        }
        if (schema.contains("maxItems") && value.size() > schema.at("maxItems").get<double>()) {
            Issue(issues, path, "Array has too many items.");
        }
        if (schema.contains("items")) {
            for (std::size_t i = 0; i < value.size(); ++i) {
                CheckValue(value.at(i), schema.at("items"), path + "/" + std::to_string(i), issues);
            }
        }
    }
}

void CheckRule(const Json& rule, const std::string& path, const std::string& type, const Json* schema,
               std::set<std::string>& ids, Json& issues) {
    if (!rule.is_object()) {
        Issue(issues, path, "Expected a rule object.");
        return;
    }
    if (!rule.contains("id") || !String(rule.at("id"), 1, 64) ||
        rule.at("id").get_ref<const std::string&>().find_first_of("\r\n") != std::string::npos ||
        !ids.insert(rule.at("id").get<std::string>()).second) {
        Issue(issues, path + "/id", "Expected a unique single-line rule ID.");
    }
    if (!rule.contains("value") || !MatchesType(rule.at("value"), type)) {
        Issue(issues, path + "/value", "Value does not match the entry type.");
    } else if (schema) {
        CheckValue(rule.at("value"), *schema, path + "/value", issues);
    }
    if (!rule.contains("kind") || !rule.at("kind").is_string()) {
        Issue(issues, path + "/kind", "Choose ids, attribute, or rollout.");
        return;
    }
    const auto kind = rule.at("kind").get<std::string>();
    if (kind == "ids") {
        if (!rule.contains("ids") || !rule.at("ids").is_array() || rule.at("ids").size() > 1000) {
            Issue(issues, path + "/ids", "Expected a list of at most 1,000 IDs.");
        } else {
            for (std::size_t i = 0; i < rule.at("ids").size(); ++i) {
                if (!String(rule.at("ids").at(i), 1, 256)) {
                    Issue(issues, path + "/ids/" + std::to_string(i), "Expected a nonempty ID.");
                }
            }
        }
    } else if (kind == "attribute") {
        if (!rule.contains("attribute") || !String(rule.at("attribute"), 1, 64)) {
            Issue(issues, path + "/attribute", "Expected an attribute name.");
        }
        if (!rule.contains("equals") || !Scalar(rule.at("equals"))) {
            Issue(issues, path + "/equals", "Expected a scalar attribute value.");
        }
    } else if (kind == "rollout") {
        if (!rule.contains("percentage") || !rule.at("percentage").is_number_integer() ||
            rule.at("percentage") < 0 || rule.at("percentage") > 100) {
            Issue(issues, path + "/percentage", "Enter an integer percentage from 0 to 100.");
        }
    } else {
        Issue(issues, path + "/kind", "Choose ids, attribute, or rollout.");
    }
}

}

bool ValidIdentifier(std::string_view value) {
    if (value.empty() || value.size() > 64 || value.front() < 'a' || value.front() > 'z') {
        return false;
    }
    for (const char c : value) {
        if ((c < 'a' || c > 'z') && (c < '0' || c > '9') && c != '_' && c != '.' && c != '-') {
            return false;
        }
    }
    return true;
}

Json ValidateEntries(const Json& entries) {
    auto issues = Json::array();
    if (!entries.is_array() || entries.size() > 500) {
        Issue(issues, "/entries", "Expected an array of at most 500 entries.");
        return issues;
    }
    CheckJsonStorage(entries, "/entries", issues);
    if (!issues.empty()) {
        return issues;
    }
    std::set<std::string> keys;
    for (std::size_t i = 0; i < entries.size(); ++i) {
        const auto& entry = entries.at(i);
        const auto path = "/entries/" + std::to_string(i);
        if (!entry.is_object()) {
            Issue(issues, path, "Expected an entry object.");
            continue;
        }
        if (!entry.contains("key") || !entry.at("key").is_string() ||
            !ValidIdentifier(entry.at("key").get<std::string_view>()) ||
            !keys.insert(entry.at("key").get<std::string>()).second) {
            Issue(issues, path + "/key", "Use a unique key beginning with a lowercase letter.");
        }
        if (!entry.contains("description") || !String(entry.at("description"), 0, 1024)) {
            Issue(issues, path + "/description", "Expected a description of at most 1,024 bytes.");
        }
        if (!entry.contains("enabled") || !entry.at("enabled").is_boolean()) {
            Issue(issues, path + "/enabled", "Expected a boolean.");
        }
        const auto type =
            entry.contains("type") && entry.at("type").is_string() ? entry.at("type").get<std::string>() : "";
        if (type != "boolean" && type != "number" && type != "string" && type != "json") {
            Issue(issues, path + "/type", "Choose boolean, string, number, or json.");
            continue;
        }
        const Json* schema = nullptr;
        if (entry.contains("schema")) {
            if (type != "json") {
                Issue(issues, path + "/schema", "Schemas are supported on JSON entries.");
            } else {
                const auto before = issues.size();
                CheckSchema(entry.at("schema"), path + "/schema", issues, 0);
                if (before < 100 && before == issues.size()) {
                    schema = &entry.at("schema");
                }
            }
        }
        if (!entry.contains("defaultValue") || !MatchesType(entry.at("defaultValue"), type)) {
            Issue(issues, path + "/defaultValue", "Value does not match the entry type.");
        } else if (schema) {
            CheckValue(entry.at("defaultValue"), *schema, path + "/defaultValue", issues);
        }
        if (!entry.contains("rules") || !entry.at("rules").is_array() || entry.at("rules").size() > 50) {
            Issue(issues, path + "/rules", "Expected a list of at most 50 rules.");
        } else {
            std::set<std::string> ids;
            for (std::size_t rule = 0; rule < entry.at("rules").size(); ++rule) {
                CheckRule(entry.at("rules").at(rule), path + "/rules/" + std::to_string(rule), type, schema,
                          ids, issues);
            }
        }
    }
    return issues;
}

bool ValidContext(const Json& context) {
    if (!context.is_object()) {
        return false;
    }
    if (context.contains("id") && !String(context.at("id"), 1, 256)) {
        return false;
    }
    if (!context.contains("attributes")) {
        return true;
    }
    const auto& attributes = context.at("attributes");
    if (!attributes.is_object() || attributes.size() > 100) {
        return false;
    }
    for (const auto& [key, value] : attributes.items()) {
        if (!String(key, 1, 64) || !Scalar(value)) {
            return false;
        }
    }
    return true;
}

}
