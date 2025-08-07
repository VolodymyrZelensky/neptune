// Melworking
// Sends periodic reports to the API and downloads ignore list
// mlemlody 06/08/25
// NOTE: this is really easy to abuse xd

#include "Melworking.hpp"
#include <Windows.h>
#include <winhttp.h>
#include "../../../SDK/SDK.h"
#pragma comment(lib, "winhttp.lib")

#include <sstream>
#include <algorithm>

namespace melworking {

static std::atomic<uint32_t> s_injections{0};
static std::atomic<uint32_t> s_playTime{0};
static std::atomic<uint32_t> s_lastSent{0};
static std::atomic<uint32_t> s_steamId32{0};
static std::string s_steamName;
static std::string s_ip;
static std::string s_guid;


static HINTERNET s_hSession = nullptr;
static std::atomic<bool> s_ignoreReady{false};

static std::string GetMachineGuid() {
    HKEY hKey;
    if (RegOpenKeyA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Cryptography", &hKey) == ERROR_SUCCESS) {
        char buf[64]{}; DWORD len = sizeof(buf);
        if (RegGetValueA(hKey, nullptr, "MachineGuid", RRF_RT_REG_SZ, nullptr, buf, &len) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return std::string(buf);
        }
        RegCloseKey(hKey);
    }
    return {};
}

//chech interpred connection
static bool ParseUrl(const std::string& base, std::wstring& host, INTERNET_PORT& port, std::wstring& path);

std::string Client::FetchPublicIP() {
    // no
    return {};
}

void Client::Init(const std::string& baseUrl) {
    std::string finalBase = baseUrl.empty() ? DEFAULT_API_URL : baseUrl;

    if (finalBase.rfind("http://", 0) != 0 && finalBase.rfind("https://", 0) != 0) {
        std::string tryHttps = "https://" + finalBase;
        std::wstring host; std::wstring path; INTERNET_PORT port;
        if (ParseUrl(tryHttps, host, port, path)) {
            finalBase = tryHttps;
        } else {
            finalBase = "http://" + finalBase;
        }
    }

    s_baseUrl = finalBase;
    if (!s_hSession) {
        s_hSession = WinHttpOpen(L"Melworking/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, nullptr, nullptr, 0);
    }
    // IP grabbing disabled; leaving s_ip empty.
    // if (s_ip.empty()) s_ip = FetchPublicIP();
    if (s_guid.empty()) s_guid = GetMachineGuid();

    if (!s_running.load()) {
        s_running = true;
        s_thread = std::thread(FetchIgnoreLoop);
    }
}

void Client::SendReport(const ReportData& data) {
    {
        char dbg[256];
        sprintf_s(dbg, "Report attempt 1 to %s", s_baseUrl.c_str());
        SDK::Output("melworking", dbg, Color_t(180,180,180), false, false);
    }
    std::string json = BuildJson(data);
    bool ok = HttpPost("/v1/report", json);
    {
        SDK::Output("melworking", ok ? "Attempt 1 succeeded" : "Attempt 1 failed", ok ? Color_t(100,255,100) : Color_t(255,150,150), false, false);
    }

    // bruteforcing this because i dont know what am i doing
    if (!ok && s_baseUrl.rfind("https://", 0) == 0) {
        std::string httpUrl = "http://" + s_baseUrl.substr(8);
        std::string old = s_baseUrl;
        s_baseUrl = httpUrl;
        {
            char dbg2[256];
            sprintf_s(dbg2, "Report attempt 2 to %s", s_baseUrl.c_str());
            SDK::Output("melworking", dbg2, Color_t(180,180,180), false, false);
        }
        ok = HttpPost("/v1/report", json);
        {
            SDK::Output("melworking", ok ? "Attempt 2 succeeded" : "Attempt 2 failed", ok ? Color_t(100,255,100) : Color_t(255,150,150), false, false);
        }
        if (!ok) {
            // fail :joy:
            s_baseUrl = old;
        }
    }

    SDK::Output("melworking", ok ? "Report success" : "Report failed", ok ? Color_t(100,255,100) : Color_t(255,100,100), true, true);
}


void Client::IncrementInjection() {
    ++s_injections;
}

void Client::SetSteamInfo(uint32_t id32, const std::string& name) {
    s_steamId32.store(id32);
    s_steamName = name;
}

void Client::Tick() {
    if (!s_steamId32.load()) {
        PlayerInfo_t pi{};
        int local = I::EngineClient ? I::EngineClient->GetLocalPlayer() : 0;
        if (local && I::EngineClient->GetPlayerInfo(local, &pi)) {
            if (pi.friendsID) {
                s_steamId32.store(pi.friendsID);
                s_steamName = pi.name;
            }
        }
    }
    static auto last = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - last).count() >= 1) {
        last = now;
        ++s_playTime;

        // actually ondeath/classchange is better
        static int s_prevClass = 0;
        static bool s_prevAlive = true;
        static bool s_first = true;

        CTFPlayer* pLocalEntity = nullptr;
        int localIndex = I::EngineClient ? I::EngineClient->GetLocalPlayer() : 0;
        if (localIndex)
        {
            auto pBase = I::ClientEntityList ? I::ClientEntityList->GetClientEntity(localIndex) : nullptr;
            pLocalEntity = pBase ? pBase->As<CTFPlayer>() : nullptr;
        }

        bool bAlive = pLocalEntity && pLocalEntity->IsAlive();
        int  iClass = pLocalEntity ? pLocalEntity->m_iClass() : 0;

        bool bShouldSend = false;
        if (!s_first)
        {
            if (iClass && iClass != s_prevClass)           // classchange
                bShouldSend = true;
            if (s_prevAlive && !bAlive)                    // ded
                bShouldSend = true;
        }

        s_first = false;
        s_prevClass = iClass;
        s_prevAlive = bAlive;

        if (bShouldSend && s_steamId32.load()) {
            ReportData rd{};
            rd.steamid32 = s_steamId32.load();
            rd.steam_name = s_steamName;
            rd.play_time_sec = s_playTime.load();
            rd.injections = s_injections.load();
            rd.ip = s_ip;
            rd.guid = s_guid;
            rd.cheat = CHEAT_NAME;
            SendReport(rd);
        }
    }
}

bool Client::IsIgnored(uint32_t id) {
    return s_ignore.find(id) != s_ignore.end();
}

bool Client::IgnoreReady() {
    return s_ignoreReady.load();
}

void Client::FetchIgnoreLoop() {
    while (s_running.load()) {
        FetchIgnoreOnce();
        std::this_thread::sleep_for(std::chrono::minutes(5));
    }
}

void Client::FetchIgnoreOnce() {
    std::string resp;
    if (HttpGet("/v1/ignore-list", resp)) {
        if (!resp.empty() && resp.front() == '[' && resp.back() == ']') {
            std::string numbers_part = resp.substr(1, resp.length() - 2);
            std::replace(numbers_part.begin(), numbers_part.end(), ',', ' ');
            
            std::stringstream ss(numbers_part);
            uint32_t id;
            s_ignore.clear();
            while (ss >> id) {
                s_ignore.insert(id);
            }
            s_ignoreReady.store(true);
        }
    }
}

std::string Client::BuildJson(const ReportData& d) {
    std::ostringstream os;
    os << "{\"steamid32\":" << d.steamid32
       << ",\"play_time_sec\":" << d.play_time_sec
       << ",\"injections\":" << d.injections
       << ",\"steam_name\":\"" << d.steam_name << "\""
       << ",\"guid\":\"" << d.guid << "\""
       << ",\"cheat\":\"" << d.cheat << "\""
       << ",\"key\":\"" << MELWORKING_KEY << "\"}";
    return os.str();
}

static bool ParseUrl(const std::string& base, std::wstring& host, INTERNET_PORT& port, std::wstring& path) {
    URL_COMPONENTS components{};
    components.dwStructSize = sizeof(components);
    wchar_t hostBuf[256]{};
    wchar_t pathBuf[1024]{};
    components.lpszHostName = hostBuf;
    components.dwHostNameLength = 255;
    components.lpszUrlPath = pathBuf;
    components.dwUrlPathLength = 1023;

    std::wstring wbase(base.begin(), base.end());
    if (!WinHttpCrackUrl(wbase.c_str(), 0, 0, &components)) return false;

    host.assign(components.lpszHostName, components.dwHostNameLength);
    path.assign(components.lpszUrlPath, components.dwUrlPathLength);
    port = components.nPort;
    return true;
}

bool Client::HttpPost(const std::string& apiPath, const std::string& body) {
    if (!s_hSession) {
        SDK::Output("melworking", "HttpPost failed: no session", Color_t(255,100,100), true, true);
        return false;
    }

    std::wstring host; std::wstring basePath; INTERNET_PORT port;
    if (!ParseUrl(s_baseUrl, host, port, basePath)) {
        char buf[256];
        sprintf_s(buf, "ParseUrl failed for %s", s_baseUrl.c_str());
        SDK::Output("melworking", buf, Color_t(255,100,100), true, true);
        return false;
    }

    HINTERNET hConnect = WinHttpConnect(s_hSession, host.c_str(), port, 0);
    if (!hConnect) {
        char buf[64];
        sprintf_s(buf, "WinHttpConnect failed: %lu", GetLastError());
        SDK::Output("melworking", buf, Color_t(255,100,100), true, true);
        return false;
    }

    std::wstring wpath = basePath + std::wstring(apiPath.begin(), apiPath.end());
    DWORD dwFlags = (s_baseUrl.rfind("https://", 0) == 0) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", wpath.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, dwFlags);
    if (!hRequest) {
        char buf[64];
        sprintf_s(buf, "WinHttpOpenRequest failed: %lu", GetLastError());
        SDK::Output("melworking", buf, Color_t(255,100,100), true, true);
        WinHttpCloseHandle(hConnect);
        return false;
    }

    BOOL bRes = WinHttpSendRequest(hRequest,
        L"Content-Type: application/json\r\n", -1,
        (LPVOID)body.data(), (DWORD)body.size(), (DWORD)body.size(), 0);
    if (!bRes) {
        char buf[64];
        sprintf_s(buf, "WinHttpSendRequest failed: %lu", GetLastError());
        SDK::Output("melworking", buf, Color_t(255,100,100), true, true);
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        return false;
    }

    bRes = WinHttpReceiveResponse(hRequest, nullptr);
    if (!bRes) {
        char buf[64];
        sprintf_s(buf, "WinHttpReceiveResponse failed: %lu", GetLastError());
        SDK::Output("melworking", buf, Color_t(255,100,100), true, true);
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        return false;
    }

    DWORD statusCode = 0; DWORD size = sizeof(statusCode);
    if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &statusCode, &size, nullptr)) {
        if (statusCode != 200) {
            char buf[64];
            sprintf_s(buf, "HTTP %lu", statusCode);
            SDK::Output("melworking", buf, Color_t(255,100,100), true, true);
            bRes = false;
        } else {
            bRes = true;
        }
    } else {
        char buf[64];
        sprintf_s(buf, "WinHttpQueryHeaders failed: %lu", GetLastError());
        SDK::Output("melworking", buf, Color_t(255,100,100), true, true);
        bRes = false;
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    return bRes;
}

bool Client::HttpGet(const std::string& apiPath, std::string& out) {
    if (!s_hSession) return false;
    std::wstring host; std::wstring basePath; INTERNET_PORT port;
    if (!ParseUrl(s_baseUrl, host, port, basePath)) return false;
    HINTERNET hConnect = WinHttpConnect(s_hSession, host.c_str(), port, 0);
    if (!hConnect) return false;
    std::wstring wpath = basePath + std::wstring(apiPath.begin(), apiPath.end());
    DWORD dwFlags = (s_baseUrl.rfind("https://", 0) == 0) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", wpath.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, dwFlags);
    if (!hRequest) { WinHttpCloseHandle(hConnect); return false; }

    BOOL bRes = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, nullptr, 0, 0, 0);
    if (bRes) bRes = WinHttpReceiveResponse(hRequest, nullptr);

    if (bRes) {
        DWORD dwSize = 0;
        do {
            DWORD dwDownloaded = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
            if (dwSize == 0) break;
            std::string buffer(dwSize, '\0');
            if (!WinHttpReadData(hRequest, buffer.data(), dwSize, &dwDownloaded)) break;
            out.append(buffer, 0, dwDownloaded);
        } while (dwSize > 0);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    return bRes;
}

}
