#include "postgres.hpp"

#include <drogon/orm/DbClient.h>
#include <drogon/orm/Exception.h>

#include <chrono>
#include <future>

namespace flipkiln {
namespace {
namespace orm = drogon::orm;

Rows Run(orm::DbClient& client, std::string_view sql, const Params& params) {
    Rows rows;
    try {
        auto query = client << sql;
        for (const auto& arg : params) {
            query << arg;
        }
        query << orm::Mode::Blocking;
        query >> [&rows](const orm::Result& result) {
            rows.reserve(result.size());
            for (const auto& source : result) {
                auto& target = rows.emplace_back();
                target.reserve(result.columns());
                for (orm::Result::RowSizeType i = 0; i < result.columns(); ++i) {
                    const auto field = source[i];
                    if (field.isNull()) {
                        target.emplace_back();
                    } else {
                        target.emplace_back(field.as<std::string>());
                    }
                }
            }
        };
        query.exec();
    } catch (const orm::DrogonDbException&) {
        throw DatabaseError("PostgreSQL query failed");
    }
    return rows;
}

class PgTransaction final : public Transaction {
public:
    explicit PgTransaction(std::shared_ptr<orm::Transaction> connection)
        : connection_(std::move(connection)) {}

    ~PgTransaction() override {
        if (connection_) {
            connection_->rollback();
        }
    }

    Rows Query(std::string_view sql, const Params& params) override { return Run(*connection_, sql, params); }

    void Commit() override {
        const auto result = std::make_shared<std::promise<bool>>();
        auto committed = result->get_future();
        connection_->setCommitCallback([result](bool success) { result->set_value(success); });
        connection_.reset();
        if (committed.wait_for(std::chrono::seconds(6)) != std::future_status::ready || !committed.get()) {
            throw DatabaseError("PostgreSQL commit could not be confirmed");
        }
    }

private:
    std::shared_ptr<orm::Transaction> connection_;
};

class PgDatabase final : public Database {
public:
    explicit PgDatabase(const std::string& dsn) : client_(orm::DbClient::newPgClient(dsn, 8)) {
        if (!client_) {
            throw DatabaseError("Cannot create PostgreSQL client");
        }
        client_->setTimeout(2.5);
    }

    Rows Query(std::string_view sql, const Params& params) override { return Run(*client_, sql, params); }

    std::unique_ptr<Transaction> Begin(TransactionMode mode) override {
        try {
            auto connection = client_->newTransaction();
            if (!connection) {
                throw DatabaseError("PostgreSQL transaction is unavailable");
            }
            connection->setTimeout(2.5);
            auto transaction = std::make_unique<PgTransaction>(std::move(connection));
            if (mode == TransactionMode::ReadSnapshot) {
                transaction->Query("SET TRANSACTION ISOLATION LEVEL REPEATABLE READ, READ ONLY", {});
            }
            return transaction;
        } catch (const orm::DrogonDbException&) {
            throw DatabaseError("PostgreSQL transaction is unavailable");
        }
    }

private:
    orm::DbClientPtr client_;
};
}

std::unique_ptr<Database> ConnectPostgres(const std::string& dsn) {
    return std::make_unique<PgDatabase>(dsn);
}
}
