#include "domain.hpp"

#include <gtest/gtest.h>

namespace flipkiln {
namespace {
Json Flag(const Json& rules = Json::array(), bool enabled = true) {
    return {{"key", "checkout.fast_lane"}, {"type", "boolean"},  {"description", "Express checkout"},
            {"defaultValue", false},       {"enabled", enabled}, {"rules", rules}};
}

TEST(Evaluation, FirstMatchingRuleWinsAndDisableUsesDefault) {
    const Json rules = Json::parse(R"([
      {"id":"staff","kind":"ids","ids":["customer-42"],"value":true},
      {"id":"plan","kind":"attribute","attribute":"plan","equals":"pro","value":false},
      {"id":"rollout","kind":"rollout","percentage":100,"value":true}
    ])");
    const auto keys = Json::array({"checkout.fast_lane"});
    const Json matching = {{"id", "customer-42"}, {"attributes", {{"plan", "pro"}}}};
    const auto enabled = Json::array({Flag(rules)});
    const auto disabled = Json::array({Flag(rules, false)});
    ASSERT_TRUE(ValidateEntries(enabled).empty());
    const auto matched = Evaluate(enabled, keys, matching, "workshop", "development").at(0);
    EXPECT_EQ(matched.at("value"), true);
    EXPECT_EQ(matched.at("ruleId"), "staff");
    EXPECT_EQ(matched.at("reason"), "rule_match");
    const auto off = Evaluate(disabled, keys, matching, "workshop", "development").at(0);
    EXPECT_EQ(off.at("value"), false);
    EXPECT_EQ(off.at("reason"), "disabled");
    EXPECT_FALSE(off.contains("ruleId"));
}

TEST(Evaluation, RolloutIsStableMonotonicAndRequiresIdentity) {
    const auto keys = Json::array({"checkout.fast_lane"});
    Json percentage = {{"id", "staged"}, {"kind", "rollout"}, {"percentage", 0}, {"value", true}};
    const auto evaluate = [&](int threshold, const Json& context) {
        percentage["percentage"] = threshold;
        return Evaluate(Json::array({Flag(Json::array({percentage}))}), keys, context, "workshop",
                        "development")
            .at(0)
            .at("value")
            .get<bool>();
    };
    const Json context = {{"id", "customer-42"}};
    const auto bucket = Bucket("workshop", "development", "checkout.fast_lane", "staged", "customer-42");
    EXPECT_EQ(bucket, 3087);
    EXPECT_FALSE(evaluate(0, context));
    EXPECT_FALSE(evaluate(bucket / 100, context));
    EXPECT_TRUE(evaluate(bucket / 100 + 1, context));
    EXPECT_TRUE(evaluate(100, context));
    EXPECT_FALSE(evaluate(100, Json::object()));
    EXPECT_NE(bucket, Bucket("workshop", "production", "checkout.fast_lane", "staged", "customer-42"));
}

TEST(Evaluation, UnknownAndTypedRequestsOmitValue) {
    const auto keys = Json::array({"missing.key", Json{{"key", "checkout.fast_lane"}, {"type", "number"}}});
    const auto result = Evaluate(Json::array({Flag()}), keys, Json::object(), "workshop", "development");
    EXPECT_EQ(result.at(0).at("reason"), "unknown_key");
    EXPECT_EQ(result.at(1).at("reason"), "type_mismatch");
    EXPECT_FALSE(result.at(0).contains("value"));
    EXPECT_FALSE(result.at(1).contains("value"));
}

TEST(Validation, SchemaPathsUseJsonPointerAndRulesUseSameSchema) {
    const Json entries = Json::parse(R"([{
       "key": "checkout.copy", "description": "", "type": "json", "enabled": true,
       "defaultValue": {"line/limit":0}, "rules": [
          {"id":"all","kind":"rollout","percentage":1,"value":{}}
       ],
       "schema":{"type":"object","properties":{"line/limit":{"type":"integer","minimum":1}},
          "additionalProperties":false,"required":["line/limit"]}
    }])");
    const Json issues = ValidateEntries(entries);
    ASSERT_EQ(issues.size(), 2);
    EXPECT_EQ(issues.at(0).at("path"), "/entries/0/defaultValue/line~1limit");
    EXPECT_EQ(issues.at(1).at("path"), "/entries/0/rules/0/value/line~1limit");
}

TEST(Validation, RejectsUnsupportedSchemasAndNonPortableJsonValues) {
    auto entries = Json::parse(R"([{
      "key": "checkout.copy", "description": "", "type":"json", "defaultValue":{"k":"\u0000"},
      "enabled":true,"rules":[],"schema":{"$ref":"#/other"}
    }])");
    const auto invalid_text = ValidateEntries(entries);
    ASSERT_EQ(invalid_text.size(), 1);
    EXPECT_EQ(invalid_text.at(0).at("path"), "/entries/0/defaultValue/k");
    entries.at(0)["defaultValue"]["k"] = 9007199254740992ULL;
    const auto invalid_integer = ValidateEntries(entries);
    ASSERT_EQ(invalid_integer.size(), 1);
    EXPECT_EQ(invalid_integer.at(0).at("path"), "/entries/0/defaultValue/k");
    entries.at(0)["defaultValue"]["k"] = 9007199254740991ULL;
    const auto invalid_schema = ValidateEntries(entries);
    ASSERT_EQ(invalid_schema.size(), 1);
    EXPECT_EQ(invalid_schema.at(0).at("path"), "/entries/0/schema/$ref");
}

TEST(Validation, ContextMayOmitIdAndPreservesScalarTypes) {
    EXPECT_TRUE(ValidContext(Json::object()));
    EXPECT_TRUE(
        ValidContext(Json::parse(R"({"id":"customer-42","attributes":{"plan":"pro","input":null}})")));
    EXPECT_FALSE(ValidContext(Json::parse(R"({"id":42})")));
    EXPECT_FALSE(ValidContext(Json::parse(R"({"attributes":{"plan":{"nested":true}}})")));
}
}
}
