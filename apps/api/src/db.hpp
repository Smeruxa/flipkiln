#pragma once

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace flipkiln {
using Params = std::vector<std::string>;
using Row = std::vector<std::optional<std::string>>;
using Rows = std::vector<Row>;

class DatabaseError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class Connection {
public:
    virtual ~Connection() = default;
    virtual Rows Query(std::string_view sql, const Params& params = {}) = 0;
};

class Transaction : public Connection {
public:
    virtual void Commit() = 0;
};

enum class TransactionMode { ReadWrite, ReadSnapshot };

class Database : public Connection {
public:
    virtual std::unique_ptr<Transaction> Begin(TransactionMode mode = TransactionMode::ReadWrite) = 0;
};
}
