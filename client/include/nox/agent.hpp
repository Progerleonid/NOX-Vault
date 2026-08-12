#pragma once

#include <chrono>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>

namespace nox {
class AgentClient {
  public:
    explicit AgentClient(std::filesystem::path executable);
    void ensure_running() const;
    [[nodiscard]] nlohmann::json request(const nlohmann::json &value) const;
    [[nodiscard]] bool available() const noexcept;

  private:
    std::filesystem::path executable_;
};

int run_agent(long idle_timeout_seconds, long absolute_timeout_seconds);
} // namespace nox
