#include "nox/config_manager.hpp"
#include "nox/errors.hpp"
#include <cstdlib>
#include <fstream>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

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
void replace_file(const std::filesystem::path &temporary, const std::filesystem::path &target) {
#ifdef _WIN32
    if (!MoveFileExW(temporary.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw ConfigurationError("Unable to replace user configuration");
#else
    std::filesystem::rename(temporary, target);
#endif
}
void write_private_file(const std::filesystem::path &path, const std::string &contents) {
#ifdef _WIN32
    std::ofstream stream(path, std::ios::trunc);
    if (!stream || !(stream << contents))
        throw ConfigurationError("Unable to write user configuration");
#else
    const auto descriptor = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (descriptor < 0)
        throw ConfigurationError("Unable to write user configuration");
    std::size_t offset = 0;
    while (offset < contents.size()) {
        const auto written = write(descriptor, contents.data() + offset, contents.size() - offset);
        if (written <= 0) {
            close(descriptor);
            throw ConfigurationError("Unable to write user configuration");
        }
        offset += static_cast<std::size_t>(written);
    }
    if (close(descriptor) != 0)
        throw ConfigurationError("Unable to write user configuration");
#endif
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
std::filesystem::path ConfigManager::accounts_path() const {
    return directory_ / "accounts.json";
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
        config.clipboard_timeout_seconds = j.value("clipboard_timeout_seconds", 30L);
        config.start_locked = j.value("start_locked", false);
        config.language = j.value("language", "en");
    } catch (...) {
        throw ConfigurationError("Configuration contains invalid value types");
    }
    if (config.server_url)
        validate_server_url(*config.server_url);
    if (config.timeout_seconds <= 0 || config.unlock_timeout_seconds <= 0 || config.clipboard_timeout_seconds <= 0)
        throw ConfigurationError("Configuration timeouts must be positive");
    if (config.color != "auto" && config.color != "always" && config.color != "never")
        throw ConfigurationError("color must be auto, always, or never");
    if (config.language != "en" && config.language != "ru" && config.language != "pl" &&
        config.language != "de" && config.language != "cs")
        throw ConfigurationError("language must be en, ru, pl, de, or cs");
    return config;
}
void ConfigManager::write_json(const std::filesystem::path &path, const nlohmann::json &j) const {
    std::filesystem::create_directories(directory_);
#ifndef _WIN32
    std::filesystem::permissions(directory_, std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::replace);
#endif
    const auto temporary = path.string() + ".tmp";
    std::error_code remove_error;
    std::filesystem::remove(temporary, remove_error);
    write_private_file(temporary, j.dump(2) + '\n');
    replace_file(temporary, path);
}
void ConfigManager::set(const std::string &key, const std::string &value) const {
    auto j = read_json(config_path());
    if (key == "server_url") {
        validate_server_url(value);
        j[key] = value;
    } else if (key == "timeout_seconds" || key == "unlock_timeout_seconds" || key == "clipboard_timeout_seconds")
        j[key] = parse_positive(value, key);
    else if (key == "start_locked") {
        if (value != "true" && value != "false")
            throw ConfigurationError("start_locked must be true or false");
        j[key] = value == "true";
    }
    else if (key == "color") {
        if (value != "auto" && value != "always" && value != "never")
            throw ConfigurationError("color must be auto, always, or never");
        j[key] = value;
    } else if (key == "language") {
        if (value != "en" && value != "ru" && value != "pl" && value != "de" && value != "cs")
            throw ConfigurationError("language must be en, ru, pl, de, or cs");
        j[key] = value;
    } else
        throw ConfigurationError("Unknown configuration key: " + key);
    write_json(config_path(), j);
}
void ConfigManager::unset(const std::string &key) const {
    if (key != "server_url" && key != "timeout_seconds" && key != "unlock_timeout_seconds" &&
        key != "clipboard_timeout_seconds" && key != "start_locked" && key != "color" && key != "language")
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
    if (key == "clipboard_timeout_seconds")
        return std::to_string(c.clipboard_timeout_seconds);
    if (key == "start_locked")
        return c.start_locked ? "true" : "false";
    if (key == "color")
        return c.color;
    if (key == "language")
        return c.language;
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
    const auto value = nlohmann::json{{"access_token", s.access_token}, {"user_id", s.user_id},
                                      {"email", s.email}, {"expires_at", s.expires_at}};
    write_json(session_path(), value);
    auto accounts = read_json(accounts_path());
    if (!accounts.is_object())
        accounts = nlohmann::json::object();
    accounts[s.user_id] = value;
    write_json(accounts_path(), accounts);
}
void ConfigManager::clear_session() const {
    const auto active = load_session();
    if (active) {
        auto accounts = read_json(accounts_path());
        if (accounts.is_object()) {
            accounts.erase(active->user_id);
            write_json(accounts_path(), accounts);
        }
    }
    std::error_code error;
    std::filesystem::remove(session_path(), error);
    if (error)
        throw ConfigurationError("Unable to remove authentication session");
}
std::vector<AuthSession> ConfigManager::list_sessions() const {
    std::vector<AuthSession> result;
    const auto accounts = read_json(accounts_path());
    if (!accounts.is_object())
        return result;
    try {
        for (const auto &[id, value] : accounts.items()) {
            (void)id;
            result.push_back(AuthSession{value.at("access_token"), value.at("user_id"), value.at("email"),
                                         value.at("expires_at")});
        }
    } catch (...) {
        throw ConfigurationError("Stored accounts are invalid");
    }
    return result;
}
void ConfigManager::activate_session(const std::string &user_id) const {
    const auto accounts = read_json(accounts_path());
    if (!accounts.is_object() || !accounts.contains(user_id))
        throw ConfigurationError("Stored account not found");
    write_json(session_path(), accounts.at(user_id));
}
} // namespace nox
