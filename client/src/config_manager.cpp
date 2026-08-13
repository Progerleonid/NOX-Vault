#include "nox/config_manager.hpp"
#include "nox/errors.hpp"
#include <cstdlib>
#include <fstream>

namespace nox {
namespace {
std::optional<std::string> env(const char *name) {
    const char *value = std::getenv(name);
    return value ? std::optional<std::string>(value) : std::nullopt;
}
std::filesystem::path default_directory() {
#ifdef _WIN32
    if (auto value = env("APPDATA"))
        return std::filesystem::path(*value) / "Nox";
    if (auto value = env("USERPROFILE"))
        return std::filesystem::path(*value) / "AppData" / "Roaming" / "Nox";
#else
    if (auto value = env("XDG_CONFIG_HOME"))
        return std::filesystem::path(*value) / "nox";
    if (auto value = env("HOME"))
        return std::filesystem::path(*value) / ".config" / "nox";
#endif
    throw ConfigurationError("Unable to determine the per-user configuration directory");
}
nlohmann::json read_json(const std::filesystem::path &path) {
    if (!std::filesystem::exists(path))
        return nlohmann::json::object();
    try {
        std::ifstream stream(path);
        nlohmann::json result;
        stream >> result;
        return result;
    } catch (...) {
        throw ConfigurationError("Unable to read configuration file");
    }
}
long parse_positive(const std::string &value, const std::string &key) {
    try {
        const auto result = std::stol(value);
        if (result <= 0)
            throw std::invalid_argument("positive");
        return result;
    } catch (...) {
        throw ConfigurationError(key + " must be a positive integer");
    }
}
} // namespace
ConfigManager::ConfigManager(std::optional<std::filesystem::path> directory)
    : directory_(directory.value_or(default_directory())) {
}
std::filesystem::path ConfigManager::config_path() const {
    return directory_ / "config.json";
}
std::filesystem::path ConfigManager::session_path() const {
    return directory_ / "session.json";
}
ClientConfig ConfigManager::load() const {
    const auto j = read_json(config_path());
    ClientConfig config;
    try {
        if (j.contains("server_url"))
            config.server_url = j.at("server_url").get<std::string>();
        config.timeout_seconds = j.value("timeout_seconds", 15L);
        config.color = j.value("color", "auto");
        config.unlock_timeout_seconds = j.value("unlock_timeout_seconds", 900L);
    } catch (...) {
        throw ConfigurationError("Configuration contains invalid value types");
    }
    if (config.server_url)
        validate_server_url(*config.server_url);
    if (config.timeout_seconds <= 0 || config.unlock_timeout_seconds <= 0)
        throw ConfigurationError("Configuration timeouts must be positive");
    if (config.color != "auto" && config.color != "always" && config.color != "never")
        throw ConfigurationError("color must be auto, always, or never");
    return config;
}
void ConfigManager::write_json(const std::filesystem::path &path, const nlohmann::json &j) const {
    std::filesystem::create_directories(directory_);
    const auto temporary = path.string() + ".tmp";
    {
        std::ofstream stream(temporary, std::ios::trunc);
        if (!stream)
            throw ConfigurationError("Unable to write user configuration");
        stream << j.dump(2) << '\n';
    }
    std::error_code replace_error;
    std::filesystem::remove(path, replace_error);
    std::filesystem::rename(temporary, path);
#ifndef _WIN32
    std::filesystem::permissions(path, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
                                 std::filesystem::perm_options::replace);
#endif
}
void ConfigManager::set(const std::string &key, const std::string &value) const {
    auto j = read_json(config_path());
    if (key == "server_url") {
        validate_server_url(value);
        j[key] = value;
    } else if (key == "timeout_seconds" || key == "unlock_timeout_seconds")
        j[key] = parse_positive(value, key);
    else if (key == "color") {
        if (value != "auto" && value != "always" && value != "never")
            throw ConfigurationError("color must be auto, always, or never");
        j[key] = value;
    } else
        throw ConfigurationError("Unknown configuration key: " + key);
    write_json(config_path(), j);
}
void ConfigManager::unset(const std::string &key) const {
    if (key != "server_url" && key != "timeout_seconds" && key != "unlock_timeout_seconds" && key != "color")
        throw ConfigurationError("Unknown configuration key: " + key);
    auto j = read_json(config_path());
    j.erase(key);
    write_json(config_path(), j);
}
std::string ConfigManager::get(const std::string &key) const {
    const auto c = load();
    if (key == "server_url")
        return effective_server_url();
    if (key == "timeout_seconds")
        return std::to_string(c.timeout_seconds);
    if (key == "unlock_timeout_seconds")
        return std::to_string(c.unlock_timeout_seconds);
    if (key == "color")
        return c.color;
    throw ConfigurationError("Unknown configuration key: " + key);
}
std::string ConfigManager::effective_server_url() const {
    auto c = load();
    auto url = c.server_url.value_or(NOX_DEFAULT_SERVER_URL);
    validate_server_url(url);
    return url;
}
bool ConfigManager::has_server_override() const {
    return load().server_url.has_value();
}
void ConfigManager::validate_server_url(const std::string &url) {
    if (url.empty() || url.find_first_of("\r\n") != std::string::npos)
        throw ConfigurationError("Invalid server URL");
    const bool https = url.starts_with("https://");
    const auto scheme_size = https ? 8U : 7U;
    const auto authority_end = url.find_first_of("/?#", scheme_size);
    const auto authority = url.substr(scheme_size,
                                      authority_end == std::string::npos ? std::string::npos
                                                                         : authority_end - scheme_size);
    const bool local = url.starts_with("http://") &&
                       (authority == "localhost" || authority.starts_with("localhost:") || authority == "127.0.0.1" ||
                        authority.starts_with("127.0.0.1:") || authority == "[::1]" || authority.starts_with("[::1]:"));
    if (authority.empty() || authority.find('@') != std::string::npos ||
        authority.find_first_of(" \t") != std::string::npos)
        throw ConfigurationError("Server URL must contain a valid authority without user information");
    if (authority_end != std::string::npos)
        throw ConfigurationError("Server URL must not contain a path, query, or fragment");
    if (!https && !local)
        throw ConfigurationError("Server URL must use HTTPS; HTTP is allowed only for loopback development");
}
std::optional<AuthSession> ConfigManager::load_session() const {
    auto j = read_json(session_path());
    if (j.empty())
        return std::nullopt;
    try {
        return AuthSession{j.at("access_token"), j.at("user_id"), j.at("email"), j.at("expires_at")};
    } catch (...) {
        throw ConfigurationError("Stored authentication session is invalid");
    }
}
void ConfigManager::save_session(const AuthSession &s) const {
    write_json(
        session_path(),
        {{"access_token", s.access_token}, {"user_id", s.user_id}, {"email", s.email}, {"expires_at", s.expires_at}});
}
void ConfigManager::clear_session() const {
    std::error_code error;
    std::filesystem::remove(session_path(), error);
    if (error)
        throw ConfigurationError("Unable to remove authentication session");
}
} // namespace nox
