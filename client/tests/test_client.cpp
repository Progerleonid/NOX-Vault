#include <catch2/catch_test_macros.hpp>
#include "nox/config_manager.hpp"
#include "nox/agent.hpp"
#include "nox/crypto_service.hpp"
#include "nox/errors.hpp"
#include "nox/models.hpp"
#include <filesystem>
#include <chrono>
#include <thread>

using namespace nox;

#ifdef __linux__
TEST_CASE("Linux resolves the running executable through procfs") {
    std::error_code error;
    const auto expected = std::filesystem::canonical("/proc/self/exe", error);
    REQUIRE_FALSE(error);
    REQUIRE(current_executable_path("intentionally-wrong-argv-zero") == expected);
}
#endif
TEST_CASE("encryption roundtrips empty long and Unicode values") {
    CryptoService crypto;
    const auto key = crypto.random_vault_key();
    for (const std::string value : {std::string{}, std::string(100000, 'x'), std::string("секрет-🔐")}) {
        Bytes plain(value.begin(), value.end());
        auto encrypted = crypto.encrypt(plain, key, "aad");
        REQUIRE(crypto.decrypt(encrypted, key, "aad") == plain);
    }
}
TEST_CASE("tampering ciphertext nonce and AAD fails authentication") {
    CryptoService crypto;
    auto key = crypto.random_vault_key();
    Bytes plain{'s', 'e', 'c', 'r', 'e', 't'};
    auto encrypted = crypto.encrypt(plain, key, "aad");
    SECTION("ciphertext") {
        encrypted.ciphertext[0] ^= 1;
        REQUIRE_THROWS_AS(crypto.decrypt(encrypted, key, "aad"), CryptoError);
    }
    SECTION("nonce") {
        encrypted.nonce[0] ^= 1;
        REQUIRE_THROWS_AS(crypto.decrypt(encrypted, key, "aad"), CryptoError);
    }
    SECTION("AAD") {
        REQUIRE_THROWS_AS(crypto.decrypt(encrypted, key, "other"), CryptoError);
    }
}
TEST_CASE("wrong password cannot unwrap a vault key") {
    CryptoService crypto;
    auto p = crypto.default_kdf();
    auto correct = crypto.derive_kek("correct", p);
    auto wrong = crypto.derive_kek("wrong", p);
    auto wrapped = crypto.encrypt(crypto.random_vault_key(), correct, "vault-aad");
    REQUIRE_THROWS_AS(crypto.decrypt(wrapped, wrong, "vault-aad"), CryptoError);
}
TEST_CASE("each encryption gets a unique nonce") {
    CryptoService crypto;
    auto key = crypto.random_vault_key();
    Bytes value{'x'};
    auto a = crypto.encrypt(value, key, "aad"), b = crypto.encrypt(value, key, "aad");
    REQUIRE(a.nonce != b.nonce);
}
TEST_CASE("Base64 validates and roundtrips") {
    Bytes data{0, 1, 2, 253, 254, 255};
    REQUIRE(base64_decode(base64_encode(data)) == data);
    REQUIRE_THROWS_AS(base64_decode("not base64!"), CryptoError);
}
TEST_CASE("secret serialization roundtrips and rejects unknown formats") {
    EncryptedValue value{1, crypto_algorithm, Bytes{1, 2, 3}, Bytes(24, 4)};
    auto json = serialize_secret("", std::string("github"), std::nullopt, value);
    json["id"] = "00000000-0000-0000-0000-000000000001";
    json["record_version"] = 1;
    auto parsed = parse_secret(json);
    REQUIRE(parsed.name == "github");
    REQUIRE(parsed.value.ciphertext == value.ciphertext);
    json["version"] = 2;
    REQUIRE_THROWS_AS(parse_secret(json), ApiCompatibilityError);
}
TEST_CASE("encrypted private name format roundtrips and binds to record") {
    CryptoService crypto;
    auto key = crypto.random_vault_key();
    Bytes name{'g', 'i', 't', 'h', 'u', 'b'};
    auto encrypted = crypto.encrypt(name, key, CryptoService::private_name_aad("vault", "record"));
    REQUIRE(unpack_encrypted_name(pack_encrypted_name(encrypted)).ciphertext == encrypted.ciphertext);
    REQUIRE_THROWS_AS(crypto.decrypt(encrypted, key, CryptoService::private_name_aad("vault", "other")), CryptoError);
    auto packed = pack_encrypted_name(encrypted);
    packed[0] ^= 1;
    REQUIRE_THROWS_AS(unpack_encrypted_name(packed), CryptoError);
    REQUIRE_THROWS_AS(unpack_encrypted_name(Bytes(10, 0)), CryptoError);
}
TEST_CASE("local agent rejects malformed keys and enforces absolute timeout") {
    AgentClient client(std::filesystem::path("unused"));
    if (client.available()) {
        (void)client.request({{"op", "lock"}});
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    int server_result = -1;
    std::thread server([&] { server_result = run_agent(10, 1); });
    bool agent_available = false;
    for (int i = 0; i < 250 && !(agent_available = client.available()); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    if (!agent_available) {
        server.detach();
        FAIL("Local agent did not become available within 5 seconds");
    }
    bool malformed_rejected = false;
    bool expired = false;
    bool communication_ok = true;
    CryptoService crypto;
    auto key = crypto.random_vault_key();
    try {
        try {
            (void)client.request({{"op", "unlock"}, {"key", base64_encode(Bytes{1, 2})}});
        } catch (const NoxError &) {
            malformed_rejected = true;
        }
        (void)client.request({{"op", "unlock"}, {"key", base64_encode(key)}});
        for (int i = 0; i < 50 && !expired; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            expired = !client.request({{"op", "status"}}).at("unlocked").get<bool>();
        }
    } catch (...) {
        communication_ok = false;
    }
    for (int i = 0; i < 20; ++i) {
        try {
            (void)client.request({{"op", "lock"}});
            break;
        } catch (...) {
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }
    }
    server.join();
    for (int i = 0; i < 100 && client.available(); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    CryptoService::wipe(key);
    REQUIRE(malformed_rejected);
    REQUIRE(expired);
    REQUIRE(communication_ok);
    REQUIRE(server_result == 0);
}
TEST_CASE("password rewrap preserves the vault key and secret ciphertext") {
    CryptoService crypto;
    auto p1 = crypto.default_kdf();
    auto p2 = crypto.default_kdf();
    auto key = crypto.random_vault_key();
    auto old_kek = crypto.derive_kek("old", p1);
    auto wrapped = crypto.encrypt(key, old_kek, "vault");
    Bytes plain{'s'};
    auto secret = crypto.encrypt(plain, key, "secret");
    auto unwrapped = crypto.decrypt(wrapped, old_kek, "vault");
    auto new_kek = crypto.derive_kek("new", p2);
    auto rewrapped = crypto.encrypt(unwrapped, new_kek, "vault");
    auto final_key = crypto.decrypt(rewrapped, new_kek, "vault");
    REQUIRE(final_key == key);
    REQUIRE(crypto.decrypt(secret, final_key, "secret") == plain);
}
TEST_CASE("local agent supports cross-client unlock timeout and explicit lock") {
    AgentClient first(std::filesystem::path("unused"));
    if (first.available()) {
        (void)first.request({{"op", "lock"}});
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    int server_result = -1;
    std::thread server([&] { server_result = run_agent(1, 5); });
    bool agent_available = false;
    for (int i = 0; i < 250 && !(agent_available = first.available()); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    if (!agent_available) {
        server.detach();
        FAIL("Local agent did not become available within 5 seconds");
    }
    CryptoService crypto;
    auto key = crypto.random_vault_key();
    AgentClient second(std::filesystem::path("unused"));
    bool second_available = false, second_unlocked = false, expired = false, communication_ok = true;
    try {
        (void)first.request({{"op", "unlock"}, {"key", base64_encode(key)}});
        for (int i = 0; i < 50 && !(second_available = second.available()); ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        for (int i = 0; i < 50 && !second_unlocked; ++i) {
            try {
                second_unlocked = second.request({{"op", "status"}}).at("unlocked").get<bool>();
            } catch (const NoxError &) {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
        }
        for (int i = 0; i < 50 && !expired; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            expired = !second.request({{"op", "status"}}).at("unlocked").get<bool>();
        }
    } catch (...) {
        communication_ok = false;
    }
    for (int i = 0; i < 20; ++i) {
        try {
            (void)second.request({{"op", "lock"}});
            break;
        } catch (...) {
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }
    }
    server.join();
    for (int i = 0; i < 100 && second.available(); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    CryptoService::wipe(key);
    REQUIRE(second_available);
    REQUIRE(second_unlocked);
    REQUIRE(expired);
    REQUIRE(communication_ok);
    REQUIRE(server_result == 0);
}
TEST_CASE("config defaults override reset and URL policy") {
    auto directory = std::filesystem::temp_directory_path() / "nox-client-test-config";
    std::filesystem::remove_all(directory);
    ConfigManager config(directory);
    REQUIRE(config.effective_server_url() == NOX_DEFAULT_SERVER_URL);
    REQUIRE_FALSE(config.has_server_override());
    config.set("server_url", "https://vault.example");
    REQUIRE(config.effective_server_url() == "https://vault.example");
    config.unset("server_url");
    REQUIRE_FALSE(config.has_server_override());
    REQUIRE_NOTHROW(ConfigManager::validate_server_url("http://localhost:8000"));
    REQUIRE_THROWS_AS(ConfigManager::validate_server_url("http://example.com"), ConfigurationError);
    REQUIRE_THROWS_AS(ConfigManager::validate_server_url("http://localhost.evil"), ConfigurationError);
    REQUIRE_THROWS_AS(ConfigManager::validate_server_url("https://user@example.com"), ConfigurationError);
    REQUIRE_THROWS_AS(ConfigManager::validate_server_url("https://example.com/path"), ConfigurationError);
    REQUIRE_THROWS_AS(ConfigManager::validate_server_url("https://example.com?query"), ConfigurationError);
    std::filesystem::remove_all(directory);
}
