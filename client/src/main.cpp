#include <CLI/CLI.hpp>
#include "nox/agent.hpp"
#include "nox/api_client.hpp"
#include "nox/auth_manager.hpp"
#include "nox/backup_service.hpp"
#include "nox/clipboard.hpp"
#include "nox/config_manager.hpp"
#include "nox/crypto_service.hpp"
#include "nox/errors.hpp"
#include "nox/input.hpp"
#include "nox/vault_service.hpp"
#include <filesystem>
#include <iostream>
#include <sstream>

namespace {
std::string account_password() {
    return nox::read_hidden("Account password: ");
}
std::string master_password(const std::string &prompt = "Vault master password: ") {
    return nox::read_hidden(prompt);
}
void attach_session(nox::ApiClient &api, const nox::ConfigManager &config) {
    if (auto s = config.load_session())
        api.set_token(s->access_token);
}
nlohmann::json agent_request(const std::string &op, const nox::AuthSession &s, const nox::ConfigManager &c) {
    auto cfg = c.load();
    return {{"op", op},
            {"server_url", c.effective_server_url()},
            {"timeout", cfg.timeout_seconds},
            {"token", s.access_token},
            {"user_id", s.user_id}};
}
void print_agent_status(const nox::AgentClient &agent) {
    if (!agent.available()) {
        std::cout << "Vault session: locked\n";
        return;
    }
    auto r = agent.request({{"op", "status"}});
    if (!r.at("unlocked").get<bool>())
        std::cout << "Vault session: locked\n";
    else
        std::cout << "Vault session: unlocked (idle " << r.at("idle_seconds") << "s, absolute "
                  << r.at("absolute_seconds") << "s remaining)\n";
}
void run_shell(const nox::AuthSession &s, const nox::ConfigManager &c, const nox::AgentClient &agent) {
    std::cout << "Nox Vault\n─────────\n";
    print_agent_status(agent);
    std::string line;
    while (std::cout << "nox> " && std::getline(std::cin, line)) {
        std::istringstream in(line);
        std::string cmd, name;
        in >> cmd >> name;
        try {
            if (cmd.empty())
                continue;
            if (cmd == "exit" || cmd == "quit")
                break;
            if (cmd == "status") {
                print_agent_status(agent);
                continue;
            }
            if (cmd == "lock") {
                (void)agent.request({{"op", "lock"}});
                std::cout << "Vault locked.\n";
                continue;
            }
            if (cmd == "unlock") {
                std::cout << "Use 'nox unlock' outside the shell.\n";
                continue;
            }
            if (cmd == "list") {
                auto r = agent.request(agent_request("list", s, c));
                for (auto &n : r.at("names"))
                    std::cout << n.get<std::string>() << '\n';
                continue;
            }
            if (name.empty())
                throw nox::NoxError("A secret name is required");
            auto q = agent_request(cmd, s, c);
            q["name"] = name;
            if (cmd == "add" || cmd == "update")
                q["value"] = nox::read_hidden(cmd == "add" ? "Secret value: " : "New secret value: ");
            auto r = agent.request(q);
            if (cmd == "get") {
                auto value = r.at("value").get<std::string>();
                std::cout << value << '\n';
                nox::CryptoService::wipe(value);
            } else
                std::cout << "Done.\n";
        } catch (const std::exception &e) {
            std::cerr << "Error: " << e.what() << '\n';
        }
    }
}
} // namespace

int main(int argc, char **argv) {
    CLI::App app{"NOX VAULT - client-side encrypted secrets vault"};
    app.set_version_flag("--version", NOX_VERSION);
    bool verbose = false, no_color = false;
    app.add_flag("--verbose", verbose, "Show safe diagnostic request information");
    app.add_flag("--no-color", no_color, "Disable ANSI colors");
    auto *agent_cmd = app.add_subcommand("agent");
    agent_cmd->group("");
    bool serve = false;
    agent_cmd->add_flag("--serve", serve);
    auto *reg = app.add_subcommand("register", "Create an account and log in"),
         *login = app.add_subcommand("login", "Log in"),
         *logout = app.add_subcommand("logout", "Log out and lock the vault"),
         *init = app.add_subcommand("init", "Initialize the encrypted vault");
    bool private_metadata = false;
    init->add_flag("--private-metadata", private_metadata, "Encrypt secret names locally");
    std::string email, name, path;
    reg->add_option("email", email);
    login->add_option("email", email);
    auto *add = app.add_subcommand("add", "Add a secret"), *get = app.add_subcommand("get", "Get a secret"),
         *update = app.add_subcommand("update", "Update a secret"),
         *remove = app.add_subcommand("remove", "Remove a secret"), *list = app.add_subcommand("list", "List secrets");
    for (auto *c : {add, get, update, remove})
        c->add_option("name", name)->required();
    bool yes = false, copy = false;
    remove->add_flag("--yes", yes);
    get->add_flag("--copy", copy, "Copy to the system clipboard");
    auto *unlock = app.add_subcommand("unlock", "Unlock vault for this user"),
         *lock = app.add_subcommand("lock", "Lock local vault session"),
         *passwd = app.add_subcommand("passwd", "Rotate master password"),
         *status = app.add_subcommand("status", "Show safe status"),
         *doctor = app.add_subcommand("doctor", "Run diagnostics"),
         *shell = app.add_subcommand("shell", "Open interactive shell");
    auto *export_cmd = app.add_subcommand("export", "Write encrypted backup"),
         *import_cmd = app.add_subcommand("import", "Restore encrypted backup");
    export_cmd->add_option("file", path)->required();
    import_cmd->add_option("file", path)->required();
    bool replace = false;
    import_cmd->add_flag("--replace", replace);
    import_cmd->add_flag("--yes", yes);
    auto *config_cmd = app.add_subcommand("config", "Manage advanced configuration");
    std::string key, value;
    auto *config_get = config_cmd->add_subcommand("get"), *config_set = config_cmd->add_subcommand("set"),
         *config_unset = config_cmd->add_subcommand("unset");
    config_get->add_option("key", key)->required();
    config_set->add_option("key", key)->required();
    config_set->add_option("value", value)->required();
    config_unset->add_option("key", key)->required();
    app.require_subcommand(1);
    CLI11_PARSE(app, argc, argv);
    try {
        nox::ConfigManager config;
        if (*agent_cmd) {
            auto cfg = config.load();
            return nox::run_agent(cfg.unlock_timeout_seconds, 8 * 60 * 60);
        }
        if (*config_get) {
            std::cout << config.get(key) << '\n';
            return 0;
        }
        if (*config_set) {
            config.set(key, value);
            std::cout << "Configuration updated.\n";
            return 0;
        }
        if (*config_unset) {
            config.unset(key);
            std::cout << "Configuration reset.\n";
            return 0;
        }
        auto cfg = config.load();
        nox::AgentClient agent(std::filesystem::absolute(argv[0]));
        if (*lock) {
            if (agent.available())
                (void)agent.request({{"op", "lock"}});
            std::cout << "Vault locked.\n";
            return 0;
        }
        if (*logout) {
            if (agent.available())
                (void)agent.request({{"op", "lock"}});
            nox::ApiClient api(config.effective_server_url(), cfg.timeout_seconds, verbose);
            nox::AuthManager auth(api, config);
            auth.logout();
            std::cout << "Logged out.\n";
            return 0;
        }
        nox::ApiClient api(config.effective_server_url(), cfg.timeout_seconds, verbose);
        attach_session(api, config);
        nox::CryptoService crypto;
        nox::AuthManager auth(api, config);
        api.check_compatibility();
        if (*reg || *login) {
            if (email.empty()) {
                std::cerr << "Email: ";
                std::getline(std::cin, email);
            }
            auto password = account_password();
            auto session = auth.authenticate(email, password, reg->parsed());
            nox::CryptoService::wipe(password);
            std::cout << (*reg ? "Account created" : "Logged in") << " as " << session.email << ".\n";
            return 0;
        }
        if (*doctor) {
            std::cout << "[ok] Config loaded\n[ok] libsodium initialized\n[ok] TLS verification enabled\n[ok] Server "
                         "reachable and API v1 compatible\n";
            print_agent_status(agent);
            return 0;
        }
        if (*status) {
            std::cout << "Server: " << api.server_url() << " ("
                      << (config.has_server_override() ? "override" : "built-in default")
                      << ")\nAuthentication: " << (config.load_session() ? "session stored" : "logged out") << '\n';
            print_agent_status(agent);
            return 0;
        }
        auto session = auth.require_session();
        nox::VaultService vault(api, crypto, session.user_id);
        if (*init) {
            auto a = master_password(), b = master_password("Confirm vault master password: ");
            if (a != b) {
                nox::CryptoService::wipe(a);
                nox::CryptoService::wipe(b);
                throw nox::NoxError("Master passwords do not match");
            }
            nox::CryptoService::wipe(b);
            vault.initialize(std::move(a), private_metadata);
            std::cout << "Encrypted vault initialized.\n";
            return 0;
        }
        if (*unlock) {
            auto password = master_password();
            auto key_bytes = vault.unlock_with_password(std::move(password));
            agent.ensure_running();
            (void)agent.request({{"op", "unlock"}, {"key", nox::base64_encode(key_bytes)}});
            nox::CryptoService::wipe(key_bytes);
            std::cout << "Vault unlocked for 15 minutes of inactivity.\n";
            return 0;
        }
        if (*passwd) {
            auto old = master_password("Old vault master password: "),
                 next = master_password("New vault master password: "),
                 confirm = master_password("Confirm new master password: ");
            if (next != confirm) {
                nox::CryptoService::wipe(old);
                nox::CryptoService::wipe(next);
                nox::CryptoService::wipe(confirm);
                throw nox::NoxError("New master passwords do not match");
            }
            nox::CryptoService::wipe(confirm);
            vault.rotate_password(std::move(old), std::move(next));
            if (agent.available())
                (void)agent.request({{"op", "lock"}});
            std::cout << "Master password changed; vault locked.\n";
            return 0;
        }
        if (*export_cmd || *import_cmd) {
            if (*import_cmd && replace && !yes)
                throw nox::NoxError("Replacing a vault requires both --replace and --yes");
            auto password = master_password();
            nox::BackupService backup(api, crypto, session.user_id);
            if (*export_cmd)
                backup.export_file(path, std::move(password));
            else
                backup.import_file(path, std::move(password), replace);
            std::cout << (*export_cmd ? "Encrypted backup written.\n" : "Encrypted backup restored.\n");
            return 0;
        }
        if (*shell) {
            run_shell(session, config, agent);
            return 0;
        }
        auto q = agent_request(*list     ? "list"
                               : *get    ? "get"
                               : *add    ? "add"
                               : *update ? "update"
                                         : "remove",
                               session, config);
        if (!*list)
            q["name"] = name;
        if (*remove && !yes) {
            std::cerr << "Delete secret \"" << name << "\"? [y/N] ";
            std::string answer;
            std::getline(std::cin, answer);
            if (answer != "y" && answer != "Y") {
                std::cout << "Cancelled.\n";
                return 0;
            }
        }
        if (*add || *update)
            q["value"] = nox::read_hidden(*add ? "Secret value: " : "New secret value: ");
        auto response = agent.request(q);
        if (*list) {
            for (auto &n : response.at("names"))
                std::cout << n.get<std::string>() << '\n';
        } else if (*get) {
            auto secret = response.at("value").get<std::string>();
            if (copy) {
                nox::copy_to_clipboard(secret);
                std::cout << "Secret copied. Clipboard managers may retain its contents.\n";
            } else
                std::cout << secret << '\n';
            nox::CryptoService::wipe(secret);
        } else
            std::cout << "Done.\n";
    } catch (const nox::VersionConflict &) {
        std::cerr << "Error: secret was modified by another client. Fetch the latest version before retrying.\n";
        return 3;
    } catch (const nox::InvalidMasterPassword &e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 2;
    } catch (const nox::NoxError &e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    } catch (const std::exception &e) {
        std::cerr << "Error: unexpected client failure: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
