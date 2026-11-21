#pragma once

#include <optional>

#include "domain.hpp"
#include "scope.hpp"

namespace flipkiln::storage {
struct Actor {
    std::string username;
    bool global_admin;
};

enum class Role { Viewer, Editor, Publisher, Admin };
std::optional<Role> ParseRole(std::string_view role);
void CheckControlSchema(Connection& connection);
Actor Authenticate(Connection& connection, std::string_view session);
void RequireRole(Connection& connection, const Actor& actor, const std::string& project, Role minimum,
                 bool lock = false);

std::optional<std::string> PasswordHashFor(Connection& connection, const std::string& username);
std::string StartSession(Database& db, const std::string& username, std::string_view previous);
void EndSession(Connection& connection, std::string_view session);

Json Projects(Connection& connection, const Actor& actor);
Json CreateProject(Database& db, const Actor& actor, const std::string& id, const std::string& name);
Json Members(Connection& connection, const Actor& actor, const std::string& project);
void ReplaceMember(Database& db, const Actor& actor, const std::string& project, const std::string& username,
                   std::optional<Role> role);

Json Tokens(Connection& connection, const Actor& actor, const Scope& scope);
Json CreateToken(Database& db, const Actor& actor, const Scope& scope, const std::string& name);
void RevokeToken(Database& db, const Actor& actor, const Scope& scope, const std::string& id);
}
