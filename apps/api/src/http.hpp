#pragma once

#include <map>
#include <string>
#include <vector>

#include "problem.hpp"

namespace flipkiln {
using Headers = std::map<std::string, std::string>;

struct Request {
    std::string method;
    std::string path;
    std::string body;
    Headers headers;
    std::string peer;
    std::string session;
};

struct Response {
    int status;
    std::string body;
    Headers headers{};
};

namespace http {
Response Send(int status, const Json& json);
Response Error(const Problem& problem);
std::string_view Header(const Request& request, const std::string& name);
std::string_view Trim(std::string_view value);
bool EqualIgnoreCase(std::string_view left, std::string_view right);
std::vector<std::string> SplitPath(std::string_view path);
void RequireSameSite(const Request& request);
Json Body(const Request& request);
const Json& Field(const Json& document, const char* name);
std::string ReadString(const Json& value, std::size_t low, std::size_t high);
int ReadInt(const Json& value);
std::string ReadId(const Json& value);
bool ValidProject(std::string_view project);
Json SnapshotDocument(const Snapshot& snapshot);
EvaluationInput ReadEvaluation(const Json& body);
}
}
