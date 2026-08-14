#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include "nox/models.hpp"

namespace nox {
struct ClientConfig {
    std::optional<std::string> server_url;
    long timeout_seconds{15};
    std::string color{"auto"};
    long unlock_timeout_seconds{900};
    long clipboard_timeout_seconds{30};
    bool start_locked{false};
    std::string language{"en"};
};

class ConfigManager {
  public:
    explicit ConfigManager(std::optional<std::filesystem::path> directory = std::nullopt);
    [[nodiscard]] ClientConfig load() const;
    void set(const std::string &key, const std::string &value) const;
    void unset(const std::string &key) const;
    [[nodiscard]] std::string get(const std::string &key) const;
    [[nodiscard]] std::string effective_server_url() const;
    [[nodiscard]] bool has_server_override() const;
    [[nodiscard]] std::optional<AuthSession> load_session() const;
    void save_session(const AuthSession &session) const;
    void clear_session() const;
    [[nodiscard]] std::vector<AuthSession> list_sessions() const;
    void activate_session(const std::string &user_id) const;
    [[nodiscard]] const std::filesystem::path &directory() const noexcept {
        return directory_;
    }
    static void validate_server_url(const std::string &url);

  private:
    std::filesystem::path directory_;
    [[nodiscard]] std::filesystem::path config_path() const;
    [[nodiscard]] std::filesystem::path session_path() const;
    [[nodiscard]] std::filesystem::path accounts_path() const;
    void write_json(const std::filesystem::path &path, const nlohmann::json &json) const;
};
} // namespace nox
