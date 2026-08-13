#include "nox/agent.hpp"
#include "nox/api_client.hpp"
#include "nox/crypto_service.hpp"
#include "nox/errors.hpp"
#include "nox/vault_service.hpp"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cctype>
#include <mutex>
#include <thread>
#include <sodium.h>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <sddl.h>
#else
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <cerrno>
#endif

namespace nox {
std::filesystem::path current_executable_path(const char *argv0) {
#ifdef __linux__
    (void)argv0;
    std::error_code error;
    auto path = std::filesystem::canonical("/proc/self/exe", error);
    if (error || path.empty())
        throw NoxError("Unable to resolve the current executable through /proc/self/exe");
    return path;
#else
    return std::filesystem::absolute(argv0);
#endif
}

namespace {
constexpr std::uint32_t max_frame = 1024 * 1024;
#ifdef _WIN32
std::string endpoint() {
    char name[256]{};
    DWORD n = sizeof(name);
    GetUserNameA(name, &n);
    std::string s(name);
    for (auto &c : s)
        if (!std::isalnum(static_cast<unsigned char>(c)))
            c = '_';
    return "\\\\.\\pipe\\nox-vault-" + s;
}
using Channel = HANDLE;
void close_channel(Channel h) {
    CloseHandle(h);
}
bool write_all(Channel h, const void *p, std::size_t n) {
    DWORD done = 0;
    return WriteFile(h, p, static_cast<DWORD>(n), &done, nullptr) && done == n;
}
bool read_all(Channel h, void *p, std::size_t n) {
    DWORD done = 0;
    return ReadFile(h, p, static_cast<DWORD>(n), &done, nullptr) && done == n;
}
Channel connect_channel() {
    return CreateFileA(endpoint().c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
}
bool same_user_peer(Channel h) {
    ULONG client_pid = 0;
    if (!GetNamedPipeClientProcessId(h, &client_pid))
        return false;
    HANDLE client_process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, client_pid);
    if (!client_process)
        return false;
    HANDLE client_token = nullptr;
    const bool opened = OpenProcessToken(client_process, TOKEN_QUERY, &client_token) != 0;
    CloseHandle(client_process);
    if (!opened)
        return false;
    HANDLE process_token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &process_token)) {
        CloseHandle(client_token);
        return false;
    }
    DWORD cn = 0, pn = 0;
    GetTokenInformation(client_token, TokenUser, nullptr, 0, &cn);
    GetTokenInformation(process_token, TokenUser, nullptr, 0, &pn);
    std::vector<unsigned char> ci(cn), pi(pn);
    const bool ok = GetTokenInformation(client_token, TokenUser, ci.data(), cn, &cn) &&
                    GetTokenInformation(process_token, TokenUser, pi.data(), pn, &pn) &&
                    EqualSid(reinterpret_cast<TOKEN_USER *>(ci.data())->User.Sid,
                             reinterpret_cast<TOKEN_USER *>(pi.data())->User.Sid);
    CloseHandle(client_token);
    CloseHandle(process_token);
    return ok;
}
Channel accept_channel() {
    HANDLE token = nullptr;
    DWORD needed = 0;
    std::string descriptor;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
        return INVALID_HANDLE_VALUE;
    GetTokenInformation(token, TokenUser, nullptr, 0, &needed);
    std::vector<unsigned char> info(needed);
    if (!GetTokenInformation(token, TokenUser, info.data(), needed, &needed)) {
        CloseHandle(token);
        return INVALID_HANDLE_VALUE;
    }
    CloseHandle(token);
    LPSTR sid_text = nullptr;
    if (!ConvertSidToStringSidA(reinterpret_cast<TOKEN_USER *>(info.data())->User.Sid, &sid_text))
        return INVALID_HANDLE_VALUE;
    descriptor = "D:P(A;;GA;;;SY)(A;;GA;;;" + std::string(sid_text) + ")";
    LocalFree(sid_text);
    PSECURITY_DESCRIPTOR sd = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorA(descriptor.c_str(), SDDL_REVISION_1, &sd, nullptr))
        return INVALID_HANDLE_VALUE;
    SECURITY_ATTRIBUTES sa{sizeof(sa), sd, FALSE};
    auto h = CreateNamedPipeA(endpoint().c_str(), PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                              1, max_frame, max_frame, 0, &sa);
    LocalFree(sd);
    if (h == INVALID_HANDLE_VALUE)
        return h;
    if (!ConnectNamedPipe(h, nullptr) && GetLastError() != ERROR_PIPE_CONNECTED) {
        CloseHandle(h);
        return INVALID_HANDLE_VALUE;
    }
    if (!same_user_peer(h)) {
        DisconnectNamedPipe(h);
        CloseHandle(h);
        return INVALID_HANDLE_VALUE;
    }
    return h;
}
#else
std::filesystem::path runtime_dir() {
    const char *x = std::getenv("XDG_RUNTIME_DIR");
    return x && *x ? std::filesystem::path(x) / "nox"
                   : std::filesystem::temp_directory_path() / ("nox-" + std::to_string(getuid()));
}
std::string endpoint() {
    return (runtime_dir() / "agent.sock").string();
}
using Channel = int;
void close_channel(Channel h) {
    close(h);
}
bool write_all(Channel h, const void *p, std::size_t n) {
    auto *c = static_cast<const char *>(p);
    while (n) {
        auto k = send(h, c, n, 0);
        if (k <= 0)
            return false;
        c += k;
        n -= static_cast<std::size_t>(k);
    }
    return true;
}
bool read_all(Channel h, void *p, std::size_t n) {
    auto *c = static_cast<char *>(p);
    while (n) {
        auto k = recv(h, c, n, 0);
        if (k <= 0)
            return false;
        c += k;
        n -= static_cast<std::size_t>(k);
    }
    return true;
}
Channel connect_channel() {
    auto h = socket(AF_UNIX, SOCK_STREAM, 0);
    sockaddr_un a{};
    a.sun_family = AF_UNIX;
    auto e = endpoint();
    std::snprintf(a.sun_path, sizeof(a.sun_path), "%s", e.c_str());
    if (connect(h, reinterpret_cast<sockaddr *>(&a), sizeof(a)) < 0) {
        close(h);
        return -1;
    }
    return h;
}
int listen_channel() {
    std::filesystem::create_directories(runtime_dir());
    chmod(runtime_dir().c_str(), 0700);
    unlink(endpoint().c_str());
    auto h = socket(AF_UNIX, SOCK_STREAM, 0);
    sockaddr_un a{};
    a.sun_family = AF_UNIX;
    auto e = endpoint();
    std::snprintf(a.sun_path, sizeof(a.sun_path), "%s", e.c_str());
    if (bind(h, reinterpret_cast<sockaddr *>(&a), sizeof(a)) < 0 || listen(h, 8) < 0) {
        close(h);
        return -1;
    }
    chmod(e.c_str(), 0600);
    return h;
}
Channel accept_channel(int listener) {
    auto h = accept(listener, nullptr, nullptr);
    if (h < 0)
        return -1;
#ifdef __linux__
    struct ucred cred {};
    socklen_t n = sizeof(cred);
    if (getsockopt(h, SOL_SOCKET, SO_PEERCRED, &cred, &n) < 0 || cred.uid != getuid()) {
        close(h);
        return -1;
    }
#endif
    return h;
}
#endif
bool valid(Channel h) {
#ifdef _WIN32
    return h != INVALID_HANDLE_VALUE;
#else
    return h >= 0;
#endif
}
nlohmann::json exchange(Channel h, const nlohmann::json &j) {
    auto s = j.dump();
    std::uint32_t n = static_cast<std::uint32_t>(s.size());
    if (n > max_frame || !write_all(h, &n, sizeof(n)) || !write_all(h, s.data(), s.size()))
        throw NoxError("Unable to communicate with local vault agent");
    std::uint32_t rn = 0;
    if (!read_all(h, &rn, sizeof(rn)) || rn > max_frame)
        throw NoxError("Invalid local agent response");
    std::string r(rn, '\0');
    if (!read_all(h, r.data(), r.size()))
        throw NoxError("Incomplete local agent response");
    return nlohmann::json::parse(r);
}
void send_response(Channel h, const nlohmann::json &j) {
    auto s = j.dump();
    auto n = static_cast<std::uint32_t>(s.size());
    (void)write_all(h, &n, sizeof(n));
    (void)write_all(h, s.data(), s.size());
}
} // namespace
AgentClient::AgentClient(std::filesystem::path executable) : executable_(std::move(executable)) {
}
bool AgentClient::available() const noexcept {
    try {
        auto h = connect_channel();
        if (!valid(h))
            return false;
        auto r = exchange(h, {{"op", "status"}});
        close_channel(h);
        return r.value("ok", false);
    } catch (...) {
        return false;
    }
}
void AgentClient::ensure_running() const {
    if (available())
        return;
#ifdef _WIN32
    std::string cmd = '"' + executable_.string() + "\" agent --serve";
    STARTUPINFOA si{sizeof(si)};
    PROCESS_INFORMATION pi{};
    if (!CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW | DETACHED_PROCESS, nullptr,
                        nullptr, &si, &pi))
        throw NoxError("Unable to start local vault agent");
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
#else
    auto pid = fork();
    if (pid < 0)
        throw NoxError("Unable to start local vault agent");
    if (pid == 0) {
        setsid();
        execl(executable_.c_str(), executable_.c_str(), "agent", "--serve", nullptr);
        _exit(127);
    }
#endif
    for (int i = 0; i < 50 && !available(); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    if (!available())
        throw NoxError("Local vault agent did not start");
}
nlohmann::json AgentClient::request(const nlohmann::json &value) const {
    auto h = connect_channel();
    if (!valid(h))
        throw NoxError("Vault is locked. Run 'nox unlock'.");
    try {
        auto r = exchange(h, value);
        close_channel(h);
        if (!r.value("ok", false))
            throw NoxError(r.value("error", std::string("Local agent request failed")));
        return r;
    } catch (...) {
        close_channel(h);
        throw;
    }
}
int run_agent(long idle, long absolute) {
    CryptoService crypto;
    Bytes key;
    std::mutex mutex;
    std::atomic<bool> stop = false;
    auto started = std::chrono::steady_clock::now(), last = started;
    std::thread watchdog([&] {
        while (!stop) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            std::scoped_lock l(mutex);
            auto now = std::chrono::steady_clock::now();
            if (!key.empty() &&
                (now - last > std::chrono::seconds(idle) || now - started > std::chrono::seconds(absolute)))
                CryptoService::wipe(key);
        }
    });
#ifndef _WIN32
    int listener = listen_channel();
    if (listener < 0) {
        stop = true;
        watchdog.join();
        return 1;
    }
#endif
    while (!stop) {
#ifdef _WIN32
        auto h = accept_channel();
#else
        auto h = accept_channel(listener);
#endif
        if (!valid(h))
            continue;
        std::uint32_t n = 0;
        if (!read_all(h, &n, sizeof(n)) || n > max_frame) {
            close_channel(h);
            continue;
        }
        std::string raw(n, '\0');
        if (!read_all(h, raw.data(), raw.size())) {
            close_channel(h);
            continue;
        }
        try {
            auto q = nlohmann::json::parse(raw);
            auto op = q.at("op").get<std::string>();
            std::scoped_lock l(mutex);
            auto now = std::chrono::steady_clock::now();
            if (op == "unlock") {
                auto candidate = base64_decode(q.at("key").get<std::string>());
                if (candidate.size() != crypto_aead_xchacha20poly1305_ietf_KEYBYTES) {
                    CryptoService::wipe(candidate);
                    throw NoxError("Invalid Vault Key supplied to agent");
                }
                CryptoService::wipe(key);
                key = std::move(candidate);
                started = last = now;
                send_response(h, {{"ok", true}});
            } else if (op == "status") {
                const auto idle_elapsed =
                    static_cast<long>(std::chrono::duration_cast<std::chrono::seconds>(now - last).count());
                const auto absolute_elapsed =
                    static_cast<long>(std::chrono::duration_cast<std::chrono::seconds>(now - started).count());
                const auto idle_left = key.empty() ? 0L : std::max(0L, idle - idle_elapsed);
                const auto absolute_left = key.empty() ? 0L : std::max(0L, absolute - absolute_elapsed);
                send_response(h, {{"ok", true},
                                  {"unlocked", !key.empty()},
                                  {"idle_seconds", idle_left},
                                  {"absolute_seconds", absolute_left}});
            } else if (op == "lock") {
                CryptoService::wipe(key);
                send_response(h, {{"ok", true}});
                stop = true;
            } else {
                if (key.empty())
                    throw NoxError("Vault session expired. Run 'nox unlock'.");
                last = now;
                ApiClient api(q.at("server_url").get<std::string>(), q.value("timeout", 15L), false);
                api.set_token(q.at("token").get<std::string>());
                VaultService vault(api, crypto, q.at("user_id").get<std::string>());
                if (op == "list") {
                    send_response(h, {{"ok", true}, {"names", vault.list_unlocked(key)}});
                } else if (op == "get") {
                    auto value = vault.get_unlocked(q.at("name").get<std::string>(), key);
                    send_response(h, {{"ok", true}, {"value", value}});
                    CryptoService::wipe(value);
                } else if (op == "add") {
                    auto value = q.at("value").get<std::string>();
                    vault.add_unlocked(q.at("name").get<std::string>(), value, key);
                    CryptoService::wipe(value);
                    send_response(h, {{"ok", true}});
                } else if (op == "update") {
                    auto value = q.at("value").get<std::string>();
                    vault.update_unlocked(q.at("name").get<std::string>(), value, key);
                    CryptoService::wipe(value);
                    send_response(h, {{"ok", true}});
                } else if (op == "remove") {
                    vault.remove_unlocked(q.at("name").get<std::string>(), key);
                    send_response(h, {{"ok", true}});
                } else
                    throw NoxError("Unknown local agent operation");
            }
        } catch (const std::exception &e) {
            send_response(h, {{"ok", false}, {"error", e.what()}});
        }
        close_channel(h);
    }
#ifndef _WIN32
    close(listener);
    unlink(endpoint().c_str());
#endif
    {
        std::scoped_lock l(mutex);
        CryptoService::wipe(key);
    }
    stop = true;
    watchdog.join();
    return 0;
}
} // namespace nox
