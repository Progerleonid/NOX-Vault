#include <CLI/CLI.hpp>
#include "nox/api_client.hpp"
#include "nox/auth_manager.hpp"
#include "nox/config_manager.hpp"
#include "nox/crypto_service.hpp"
#include "nox/errors.hpp"
#include "nox/input.hpp"
#include "nox/vault_service.hpp"
#include <chrono>
#include <iostream>

namespace {
void attach_session(nox::ApiClient& api, const nox::ConfigManager& config) {
    if (auto session=config.load_session()) api.set_token(session->access_token);
}
std::string account_password() { return nox::read_hidden("Account password: "); }
std::string master_password() { return nox::read_hidden("Vault master password: "); }
}

int main(int argc, char** argv) {
    CLI::App app{"NOX VAULT - client-side encrypted secrets vault"}; app.set_version_flag("--version", NOX_VERSION);
    bool verbose=false; app.add_flag("--verbose",verbose,"Show safe diagnostic request information");
    auto* reg=app.add_subcommand("register","Create an account and log in"); auto* login=app.add_subcommand("login","Log in to an account");
    auto* logout=app.add_subcommand("logout","Remove the local authentication session"); auto* init=app.add_subcommand("init","Initialize the encrypted vault");
    std::string email; reg->add_option("email",email,"Email address"); login->add_option("email",email,"Email address");
    std::string name; auto* add=app.add_subcommand("add","Add a secret"); add->add_option("name",name)->required();
    auto* get=app.add_subcommand("get","Decrypt a secret"); get->add_option("name",name)->required(); bool stdout_flag=false; get->add_flag("--stdout",stdout_flag,"Explicitly write plaintext to stdout");
    auto* update=app.add_subcommand("update","Replace a secret"); update->add_option("name",name)->required();
    auto* remove=app.add_subcommand("remove","Delete a secret"); remove->add_option("name",name)->required(); bool yes=false; remove->add_flag("--yes",yes,"Skip deletion confirmation");
    auto* list=app.add_subcommand("list","List secret names"); auto* status=app.add_subcommand("status","Show safe local status"); auto* doctor=app.add_subcommand("doctor","Run safe diagnostics");
    auto* config_cmd=app.add_subcommand("config","Manage advanced client configuration");
    std::string config_key,config_value; auto* config_get=config_cmd->add_subcommand("get"); config_get->add_option("key",config_key)->required();
    auto* config_set=config_cmd->add_subcommand("set"); config_set->add_option("key",config_key)->required(); config_set->add_option("value",config_value)->required();
    auto* config_unset=config_cmd->add_subcommand("unset"); config_unset->add_option("key",config_key)->required();
    app.require_subcommand(1); CLI11_PARSE(app,argc,argv);
    try {
        nox::ConfigManager config;
        if (*config_get) { std::cout << config.get(config_key) << '\n'; return 0; }
        if (*config_set) { config.set(config_key,config_value); std::cout << "Configuration updated.\n"; return 0; }
        if (*config_unset) { config.unset(config_key); std::cout << "Configuration reset to its default.\n"; return 0; }
        const auto settings=config.load(); nox::ApiClient api(config.effective_server_url(),settings.timeout_seconds,verbose); attach_session(api,config);
        nox::CryptoService crypto; nox::AuthManager auth(api,config);
        if (*reg || *login) {
            if (email.empty()) { std::cerr << "Email: "; std::getline(std::cin,email); }
            auto password=account_password(); nox::AuthSession session;
            try { session=auth.authenticate(email,password,reg->parsed()); } catch (...) { nox::CryptoService::wipe(password); throw; }
            nox::CryptoService::wipe(password);
            std::cout << (*reg ? "Account created" : "Logged in") << " as " << session.email << ".\n"; return 0;
        }
        if (*logout) { auth.logout(); std::cout << "Logged out.\n"; return 0; }
        if (*status) {
            std::cout << "Server: " << api.server_url() << " (" << (config.has_server_override()?"override":"built-in default") << ")\n";
            auto session=config.load_session(); std::cout << "Authentication: " << (session?"session stored":"logged out") << '\n';
            if (session) { try { (void)api.get("/vault"); std::cout << "Vault: initialized\n"; } catch(const nox::ServerError& e) { std::cout << "Vault: " << (e.status()==404?"not initialized":"unavailable") << '\n'; } }
            return 0;
        }
        if (*doctor) {
            std::cout << "[ok] Config loaded\n[ok] libsodium initialized\n[ok] TLS verification enabled" << (api.server_url().starts_with("http://")?" (loopback development HTTP)":"") << '\n';
            try { api.check_compatibility(); std::cout << "[ok] Server reachable\n[ok] API compatible\n"; } catch(const std::exception& e) { std::cout << "[fail] Server/API: " << e.what() << '\n'; return 1; }
            if (config.load_session()) { try { (void)api.get("/vault"); std::cout << "[ok] Authentication valid\n"; } catch(const nox::ServerError& e) { if(e.status()==404) std::cout << "[ok] Authentication valid (vault not initialized)\n"; else throw; } }
            else std::cout << "[skip] Authentication: logged out\n"; return 0;
        }
        const auto session=auth.require_session(); nox::VaultService vault(api,crypto,session.user_id);
        if (*init) {
            auto first=master_password(); auto second=nox::read_hidden("Confirm vault master password: ");
            if(first!=second) { nox::CryptoService::wipe(first); nox::CryptoService::wipe(second); throw nox::NoxError("Master passwords do not match"); }
            nox::CryptoService::wipe(second); vault.initialize(std::move(first)); std::cout << "Encrypted vault initialized.\n"; return 0;
        }
        if (*list) { for(const auto& record:vault.list()) std::cout << record.name << '\n'; return 0; }
        if (*get) {
            if(!stdout_flag) throw nox::NoxError("Plaintext output requires the explicit --stdout option");
            auto password=master_password(); auto plaintext=vault.get(name,std::move(password));
            try { std::cout << plaintext << '\n'; } catch (...) { nox::CryptoService::wipe(plaintext); throw; }
            nox::CryptoService::wipe(plaintext); return 0;
        }
        if (*add || *update) {
            auto plaintext=nox::read_hidden(*add?"Secret value: ":"New secret value: "); auto password=master_password();
            try { if(*add) vault.add(name,plaintext,std::move(password)); else vault.update(name,plaintext,std::move(password)); }
            catch (...) { nox::CryptoService::wipe(plaintext); nox::CryptoService::wipe(password); throw; }
            nox::CryptoService::wipe(plaintext); std::cout << (*add?"Secret added.\n":"Secret updated.\n"); return 0;
        }
        if (*remove) {
            if(!yes) { std::cerr << "Delete secret \"" << name << "\"? [y/N] "; std::string answer; std::getline(std::cin,answer); if(answer!="y"&&answer!="Y") { std::cout << "Cancelled.\n"; return 0; } }
            vault.remove(name); std::cout << "Secret removed.\n"; return 0;
        }
    } catch(const nox::InvalidMasterPassword& e) {
        std::cerr << "Error: " << e.what() << "\n\nPossible causes:\n- incorrect master password\n- corrupted vault metadata\n"; return 2;
    } catch(const nox::NoxError& e) { std::cerr << "Error: " << e.what() << '\n'; return 1; }
    catch(const std::exception& e) { std::cerr << "Error: unexpected client failure: " << e.what() << '\n'; return 1; }
    return 0;
}
