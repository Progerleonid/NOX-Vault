#pragma once

#include <optional>
#include <string>
#include <nlohmann/json.hpp>

namespace nox {
class ApiClient {
public:
    ApiClient(std::string server_url, long timeout_seconds, bool verbose = false);
    void set_token(std::optional<std::string> token);
    [[nodiscard]] nlohmann::json get(const std::string& path) const;
    [[nodiscard]] nlohmann::json post(const std::string& path, const nlohmann::json& body) const;
    [[nodiscard]] nlohmann::json put(const std::string& path, const nlohmann::json& body) const;
    void remove(const std::string& path) const;
    void check_compatibility() const;
    [[nodiscard]] const std::string& server_url() const noexcept { return server_url_; }
private:
    std::string server_url_; long timeout_seconds_; bool verbose_; std::optional<std::string> token_;
    [[nodiscard]] nlohmann::json request(const std::string& method, const std::string& path,
                                         const std::optional<nlohmann::json>& body) const;
};
}
