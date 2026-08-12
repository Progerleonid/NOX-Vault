#pragma once

#include "nox/api_client.hpp"
#include "nox/crypto_service.hpp"
#include <filesystem>

namespace nox {
class BackupService {
  public:
    BackupService(ApiClient &api, CryptoService &crypto, std::string user_id)
        : api_(api), crypto_(crypto), user_id_(std::move(user_id)) {
    }
    void export_file(const std::filesystem::path &path, std::string master_password) const;
    void import_file(const std::filesystem::path &path, std::string master_password, bool replace) const;

  private:
    ApiClient &api_;
    CryptoService &crypto_;
    std::string user_id_;
};
} // namespace nox
