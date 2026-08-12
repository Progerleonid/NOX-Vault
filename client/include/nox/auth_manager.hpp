#pragma once

#include <string>
#include "nox/api_client.hpp"
#include "nox/config_manager.hpp"

namespace nox {
class AuthManager {
public:
    AuthManager(ApiClient& api, ConfigManager& config) : api_(api), config_(config) {}
    AuthSession authenticate(const std::string& email, const std::string& password, bool registration);
    void logout();
    [[nodiscard]] AuthSession require_session() const;
private: ApiClient& api_; ConfigManager& config_;
};
}
