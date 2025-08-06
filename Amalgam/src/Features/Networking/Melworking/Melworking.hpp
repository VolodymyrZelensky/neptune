// Melworking
// Sends periodic reports to the API and downloads ignore list
// mlemlody 06/08/25
// NOTE: this is really easy to abuse xd

#pragma once

#include <cstdint>
#include <string>
#include <unordered_set>
#include <thread>
#include <atomic>

namespace melworking {

constexpr char MELWORKING_KEY[] = "melw0rking"; // useless TODO: remove
constexpr char DEFAULT_API_URL[] = "networking.cathook.org";
constexpr char CHEAT_NAME[] = "NEPTUNE";

struct ReportData {
    uint32_t steamid32 = 0;
    uint32_t play_time_sec = 0;
    uint32_t injections = 0;
    std::string steam_name;
    std::string ip;   // doesnt work
    std::string guid; // doesnt work
    std::string cheat; // cheat label
};

class Client {
public:
    static void IncrementInjection();
    static void Tick();
    static void SetSteamInfo(uint32_t id32, const std::string& name);

public:
    static void Init(const std::string& baseUrl);
    static void SendReport(const ReportData& data);
    static bool IsIgnored(uint32_t steamid32);
    static bool HttpGet(const std::string& path, std::string& out);

private:
    static std::string FetchPublicIP();
    static void FetchIgnoreLoop();
    static void FetchIgnoreOnce();
    static std::string BuildJson(const ReportData&);
    static bool HttpPost(const std::string& path, const std::string& body);

    static inline std::string s_baseUrl;
    static inline std::unordered_set<uint32_t> s_ignore;
    static inline std::atomic<bool> s_running{false};
    static inline std::thread s_thread;
};

}
