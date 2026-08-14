#include "nox/agent.hpp"
#include "nox/crypto_service.hpp"
#include "nox/models.hpp"
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

int main(int argc, char **argv) {
    if (argc != 3) {
        std::cerr << "usage: test_agent_entrypoint <first-client-exe> <second-client-exe>\n";
        return 2;
    }
    nox::AgentClient first(argv[1]);
    nox::AgentClient second(argv[2]);
    const auto stop_agent = [&] {
        try {
            if (first.available())
                (void)first.request({{"op", "lock"}});
        } catch (...) {
        }
        for (int i = 0; i < 100 && first.available(); ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
    };
    try {
        if (first.available()) {
            (void)first.request({{"op", "lock"}});
            for (int i = 0; i < 100 && first.available(); ++i)
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        std::atomic<int> failures{0};
        std::thread a([&] { try { first.ensure_running(); } catch (...) { ++failures; } });
        std::thread b([&] { try { second.ensure_running(); } catch (...) { ++failures; } });
        a.join();
        b.join();
        if (failures != 0 || !first.available() || !second.available()) {
            std::cerr << "concurrent ensure_running did not leave one reachable agent\n";
            stop_agent();
            return 3;
        }

        nox::CryptoService crypto;
        auto key = crypto.random_vault_key();
        (void)first.request({{"op", "unlock"}, {"key", nox::base64_encode(key)}});
        nox::CryptoService::wipe(key);
        const auto status = second.request({{"op", "status"}});
        if (status.size() != 4 || !status.at("ok").get<bool>() ||
            !status.at("unlocked").get<bool>() || !status.contains("idle_seconds") ||
            !status.contains("absolute_seconds")) {
            std::cerr << "status response schema or unlock visibility changed: " << status.dump() << '\n';
            stop_agent();
            return 4;
        }
        stop_agent();
        if (first.available()) {
            std::cerr << "explicit lock did not stop the shared agent\n";
            return 5;
        }
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        stop_agent();
        return 6;
    }
}
