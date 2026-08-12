#pragma once

#include <string>
#include <vector>
#include "nox/api_client.hpp"
#include "nox/crypto_service.hpp"

namespace nox {
class VaultService {
public:
    VaultService(ApiClient& api, CryptoService& crypto, std::string user_id)
        : api_(api), crypto_(crypto), user_id_(std::move(user_id)) {}
    void initialize(std::string master_password);
    [[nodiscard]] VaultMetadata metadata() const;
    [[nodiscard]] std::vector<SecretRecord> list() const;
    void add(const std::string& name, const std::string& plaintext, std::string master_password);
    [[nodiscard]] std::string get(const std::string& name, std::string master_password);
    void update(const std::string& name, const std::string& plaintext, std::string master_password);
    void remove(const std::string& name);
private:
    ApiClient& api_; CryptoService& crypto_; std::string user_id_;
    [[nodiscard]] Bytes unlock(const VaultMetadata& vault, std::string& password) const;
    [[nodiscard]] SecretRecord find(const std::string& name) const;
};
}
