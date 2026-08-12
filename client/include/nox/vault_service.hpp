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
    void initialize(std::string master_password, bool private_metadata = false);
    [[nodiscard]] VaultMetadata metadata() const;
    [[nodiscard]] std::vector<SecretRecord> list() const;
    void add(const std::string& name, const std::string& plaintext, std::string master_password);
    [[nodiscard]] std::string get(const std::string& name, std::string master_password);
    void update(const std::string& name, const std::string& plaintext, std::string master_password);
    void remove(const std::string& name);
    [[nodiscard]] Bytes unlock_with_password(std::string master_password) const;
    void rotate_password(std::string old_password, std::string new_password);
    void add_unlocked(const std::string& name, const std::string& plaintext, const Bytes& key);
    [[nodiscard]] std::string get_unlocked(const std::string& name, const Bytes& key);
    void update_unlocked(const std::string& name, const std::string& plaintext, const Bytes& key);
    void remove_unlocked(const std::string& name, const Bytes& key);
    [[nodiscard]] std::vector<std::string> list_unlocked(const Bytes& key) const;
private:
    ApiClient& api_; CryptoService& crypto_; std::string user_id_;
    [[nodiscard]] Bytes unlock(const VaultMetadata& vault, std::string& password) const;
    [[nodiscard]] SecretRecord find(const std::string& name, const VaultMetadata& vault, const Bytes* key = nullptr) const;
};
}
