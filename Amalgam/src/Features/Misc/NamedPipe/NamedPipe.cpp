//
// melody 3/7/2025
//
#include "NamedPipe.h"
#include "../../../SDK/SDK.h"
#include "../../Players/PlayerUtils.h"
#include "../../Configs/Configs.h"
#include "NamedPipeUtils.h"

#include <windows.h>
#include <thread>
#include <atomic>
#include <sstream>
#include <fstream>
#include <string>
#include <cstdio>
#include <unordered_map>
#include <ShlObj.h>
#include <cstdlib>
#include <deque>
#include <condition_variable>
#include <utility>

namespace F::NamedPipe
{
    // RAII wrapper for Windows HANDLE
    class AutoHandle {
        HANDLE h{ INVALID_HANDLE_VALUE };
    public:
        AutoHandle() = default;
        explicit AutoHandle(HANDLE handle) : h(handle) {}
        AutoHandle(const AutoHandle&) = delete;
        AutoHandle& operator=(const AutoHandle&) = delete;
        AutoHandle(AutoHandle&& other) noexcept { h = std::exchange(other.h, INVALID_HANDLE_VALUE); }
        AutoHandle& operator=(AutoHandle&& other) noexcept {
            if (this != &other) {
                reset();
                h = std::exchange(other.h, INVALID_HANDLE_VALUE);
            }
            return *this;
        }
        ~AutoHandle() { reset(); }

        void reset(HANDLE newHandle = INVALID_HANDLE_VALUE) {
            if (h != INVALID_HANDLE_VALUE) {
                CloseHandle(h);
            }
            h = newHandle;
        }
        [[nodiscard]] HANDLE get() const { return h; }
        [[nodiscard]] bool valid() const { return h != INVALID_HANDLE_VALUE; }
        operator HANDLE() const { return h; }
    };

    AutoHandle hPipe;
    AutoHandle hReadEvent;
    OVERLAPPED ovRead{};
    bool readPending = false;
    std::jthread pipeThread;
    int botId = -1;
    
    // Extract first positive integer appearing in string, -1 if none
    int ParseBotIdFromString(const std::string& src)
    {
        int id = -1;
        for (size_t i = 0; i < src.size(); ++i)
        {
            if (isdigit(static_cast<unsigned char>(src[i])))
            {
                id = 0;
                while (i < src.size() && isdigit(static_cast<unsigned char>(src[i])))
                {
                    id = id * 10 + (src[i] - '0');
                    ++i;
                }
                break;
            }
        }
        return id;
    }

    // Get bot ID detection in priority order: STEAMIPCNAME -> BOTID -> command line
    int DetectBotId()
    {
        char* envVal = nullptr;
        size_t len = 0;

        // 1. STEAMIPCNAME (set by panel when launching TF2)
        if (_dupenv_s(&envVal, &len, "STEAMIPCNAME") == 0 && envVal)
        {
            std::string ipc(envVal);
            free(envVal);
            int id = ParseBotIdFromString(ipc);
            if (id > 0)
            {
                Log("Detected bot ID from STEAMIPCNAME env: " + std::to_string(id));
                return id;
            }
        }

        // 2. BOTID env (legacy fallback)
        if (_dupenv_s(&envVal, &len, "BOTID") == 0 && envVal)
        {
            int id = atoi(envVal);
            free(envVal);
            if (id > 0)
            {
                Log("Detected bot ID from BOTID env: " + std::to_string(id));
                return id;
            }
        }

        // 3. Parse command line for -steamipcname steam_bot_X pattern
        char* cmdLine = GetCommandLineA();
        if (cmdLine)
        {
            std::string cmd(cmdLine);
            int id = ParseBotIdFromString(cmd);
            if (id > 0)
            {
                Log("Detected bot ID from command line: " + std::to_string(id));
                return id;
            }
        }

        return -1;
    }
    
    const int MAX_RECONNECT_ATTEMPTS = 10;
    const int BASE_RECONNECT_DELAY_MS = 500;
    const int MAX_RECONNECT_DELAY_MS = 10000;
    int currentReconnectAttempts = 0;
    DWORD lastConnectAttemptTime = 0;
    
    // Thread-safe FIFO with priority support (simple two-level queues)
    struct PendingMessage {
        std::string type;
        std::string content;
        bool isPriority;
    };

    class MessageQueue
    {
        std::deque<PendingMessage> highPrio;
        std::deque<PendingMessage> normal;
        std::mutex mtx;
        std::condition_variable cv;
    public:
        void push(const PendingMessage &msg)
        {
            {
                std::lock_guard<std::mutex> lk(mtx);
                (msg.isPriority ? highPrio : normal).push_back(msg);
            }
            cv.notify_one();
        }

        // retrieves up to maxCount messages into out vector, waits up to timeoutMs if empty
        size_t popBatch(std::vector<PendingMessage>& out, size_t maxCount, int timeoutMs)
        {
            std::unique_lock<std::mutex> lk(mtx);
            if(highPrio.empty() && normal.empty())
            {
                if(timeoutMs>=0)
                    cv.wait_for(lk, std::chrono::milliseconds(timeoutMs));
            }
            size_t count=0;
            while(count<maxCount && (!highPrio.empty() || !normal.empty()))
            {
                if(!highPrio.empty())
                {
                    out.push_back(highPrio.front());
                    highPrio.pop_front();
                } else {
                    out.push_back(normal.front());
                    normal.pop_front();
                }
                ++count;
            }
            return count;
        }
        void clear()
        {
            std::lock_guard<std::mutex> lk(mtx);
            highPrio.clear();
            normal.clear();
        }
        bool waitForMessage(int timeoutMs)
        {
            std::unique_lock<std::mutex> lk(mtx);
            if(!highPrio.empty() || !normal.empty()) return true;
            return cv.wait_for(lk, std::chrono::milliseconds(timeoutMs), [this]{ return !highPrio.empty() || !normal.empty();});
        }
    } messageQueue;
    
    std::mutex inboundMutex;
    std::vector<PendingMessage> inboundQueue;
    
    std::unordered_map<uint32_t, bool> localBots;
    

    void ConnectAndMaintainPipe(std::stop_token stoken);
    void QueueMessage(const std::string& type, const std::string& content, bool isPriority);
    void ProcessMessageQueue();
    bool SafeWriteToPipe(const std::string& message);
    int GetReconnectDelayMs();
    void QueueIncomingMessage(const std::string& type, const std::string& content);
    void ProcessIncomingQueue();
    
    // (legacy Log / PIPE_NAME / GetErrorMessage helpers removed — see NamedPipeUtils.h)

    // legacy ReadBotIdFromFile removed – DetectBotId is now used directly

    void Initialize()
    {
        Log("NamedPipe::Initialize() called");
        botId = DetectBotId();
        
        std::stringstream ss;
        if (botId == -1)
        {
            ss << "Failed to read bot ID from file";
            Log(ss.str());
        }
        else
        {
            ss << "Bot ID read from file: " << botId;
            Log(ss.str());
        }

        localBots.clear();
        Log("Cleared local bots list on startup");

        pipeThread = std::jthread(ConnectAndMaintainPipe);
        Log("Pipe thread started");
    }

    void Shutdown()
    {
        pipeThread.request_stop();
        if (pipeThread.joinable()) pipeThread.join();
    }

    void SendStatusUpdate(const std::string& status)
    {
        // Queue the status update message with high priority
        QueueMessage("Status", status, true);
        
        // Process immediately if connected
        if (hPipe.valid()) {
            ProcessMessageQueue();
        }
    }

    void ExecuteCommand(const std::string& command)
    {
        Log("ExecuteCommand called with: " + command);
        
        if (command == "debug navbot")
        {
            Log("Debug navbot command received, sending 'kill' to TF2 console");
            if (I::EngineClient)
            {
                I::EngineClient->ClientCmd_Unrestricted("kill");
                Log("'kill' command sent to TF2 console");
                SendStatusUpdate("CommandExecuted:kill");
            }
            else
            {
                Log("Error: EngineClient is not available. 'kill' command not executed");
                SendStatusUpdate("CommandFailed:kill");
            }
            return;
        }
        
        if (command.substr(0, 11) == "loadconfig ")
        {
            std::string configName = command.substr(11);
            Log("Loading config: " + configName);
            
            if (F::Configs.LoadConfig(configName, true))
            {
                Log("Config loaded successfully: " + configName);
                SendStatusUpdate("ConfigLoaded:" + configName);
            }
            else
            {
                Log("Failed to load config: " + configName);
                SendStatusUpdate("ConfigLoadFailed:" + configName);
            }
        }
        else if (I::EngineClient)
        {
            Log("EngineClient is available, sending command to TF2 console");
            I::EngineClient->ClientCmd_Unrestricted(command.c_str());
            Log("Command sent to TF2 console: " + command);
            SendStatusUpdate("CommandExecuted:" + command);
        }
        else
        {
            Log("Error: EngineClient is not available. Command not executed: " + command);
            SendStatusUpdate("CommandFailed:" + command);
        }
    }

    void SendHealthUpdate()
    {
        if (!I::EngineClient || !I::EngineClient->IsInGame())
            return;
        
        auto pLocal = H::Entities.GetLocal();
        if (!pLocal)
            return;
        
        int health = pLocal->m_iHealth();
        QueueMessage("Health", std::to_string(health), false);
    }

    int GetCurrentPlayerClass()
    {
        if (!I::EngineClient || !I::EngineClient->IsInGame())
            return -1;
        
        auto pLocal = H::Entities.GetLocal();
        if (!pLocal)
            return -1;
        
        return pLocal->As<CTFPlayer>()->m_iClass();
    }

    void SendPlayerClassUpdate(int playerClass)
    {
        if (!I::EngineClient || !I::EngineClient->IsInGame())
            return;
        
        if (playerClass == -1)
            playerClass = GetCurrentPlayerClass();
        
        if (playerClass == -1)
            return;
        
        std::string className = "Unknown";
        const char* classStr = SDK::GetClassByIndex(playerClass);
        if (classStr)
            className = classStr;
        QueueMessage("PlayerClass", className, false);
    }

    std::string GetCurrentLevelName()
    {
        if (!I::EngineClient || !I::EngineClient->IsInGame())
            return "Unknown";
        
        return SDK::GetLevelName();
    }

    void SendMapUpdate()
    {
        if (!I::EngineClient || !I::EngineClient->IsInGame())
            return;
        
        static std::string lastSentMap = "";
        std::string currentMap = GetCurrentLevelName();
        
        if (currentMap == lastSentMap || currentMap == "Unknown")
            return;
        
        lastSentMap = currentMap;
        QueueMessage("Map", currentMap, false);
    }

    void SendServerInfo()
    {
        if (!I::EngineClient || !I::EngineClient->IsInGame() || I::EngineClient->IsPlayingDemo())
            return;
        
        std::string serverInfo = "Offline";
        
        INetChannelInfo* netInfo = I::EngineClient->GetNetChannelInfo();
        if (netInfo)
        {
            // Format: IP:Port
            const char* address = netInfo->GetAddress();
            if (address && *address)
            {
                serverInfo = address;
            }
        }
        QueueMessage("ServerInfo", serverInfo, false);
    }

    void UpdateBotInfo()
    {
        if (!I::EngineClient)
            return;
        
        SendHealthUpdate();
        SendPlayerClassUpdate(-1);
        SendMapUpdate();
        SendServerInfo();
    }

    void BroadcastLocalBotId()
    {
        if (!I::EngineClient)
            return;

        // Only proceed if we have a valid steam ID
        PlayerInfo_t pi{};
        int localIdx = I::EngineClient->GetLocalPlayer();
        if (I::EngineClient->GetPlayerInfo(localIdx, &pi) && pi.friendsID != 0)
        {
            // Use the friends ID from player info
            uint32_t friendsID = pi.friendsID;
            
            // Add ourselves to the local bots map
            localBots[friendsID] = true;
            
            // Queue local bot broadcast with high priority
            QueueMessage("LocalBot", std::to_string(friendsID), true);
            Log("Queued local bot ID broadcast: " + std::to_string(friendsID));
            
            // Process immediately if connected
            if (hPipe.valid()) {
                ProcessMessageQueue();
            }
        }
    }

    void ProcessLocalBotMessage(const std::string& message)
    {
        try {
            // Parse message format expected as: "botNumber:LocalBot:friendsID"
            std::istringstream iss(message);
            std::string botNumber, messageType, friendsIDstr;
            std::getline(iss, botNumber, ':');
            std::getline(iss, messageType, ':');
            std::getline(iss, friendsIDstr);
            
            Log("Processing LocalBot message - botNumber: " + botNumber + ", type: " + messageType + ", ID: " + friendsIDstr);
            
            if (messageType == "LocalBot" && !friendsIDstr.empty())
            {
                try
                {
                    uint32_t friendsID = std::stoull(friendsIDstr);
                    
                    // Don't skip messages from our own bot ID - we might have multiple instances
                    // with the same ID running
                    
                    // Add to local bots map if not already there
                    if (localBots.find(friendsID) == localBots.end()) {
                        localBots[friendsID] = true;
                        Log("Added local bot with friendsID: " + friendsIDstr);
                        
                        // Immediately try to find player info and add tags
                        if (I::EngineClient && I::EngineClient->IsInGame()) {
                            bool tagAdded = false;
                            PlayerInfo_t pi{};
                            
                            for (int i = 1; i <= I::EngineClient->GetMaxClients(); i++)
                            {
                                if (I::EngineClient->GetPlayerInfo(i, &pi) && pi.friendsID == friendsID)
                                {
                                    // Add both IGNORED_TAG and FRIEND_TAG
                                    F::PlayerUtils.AddTag(friendsID, F::PlayerUtils.TagToIndex(IGNORED_TAG), true, pi.name);
                                    F::PlayerUtils.AddTag(friendsID, F::PlayerUtils.TagToIndex(FRIEND_TAG), true, pi.name);
                                    Log("Marked local bot as ignored and friend: " + std::string(pi.name));
                                    tagAdded = true;
                                    break;
                                }
                            }
                            
                            if (!tagAdded)
                            {
                                Log("Could not find player info for friendsID: " + friendsIDstr + " to add tags");
                            }
                        } else {
                            Log("EngineClient unavailable or not in game. Will add tags later.");
                        }
                    }
                }
                catch (const std::exception& e)
                {
                    Log("Error processing LocalBot message: " + std::string(e.what()));
                }
            }
        } catch (const std::exception& e) {
            Log("Exception in ProcessLocalBotMessage: " + std::string(e.what()));
        }
    }

    bool IsLocalBot(uint32_t friendsID)
    {
        if (friendsID == 0)
            return false;
            
        return localBots.find(friendsID) != localBots.end();
    }

    void UpdateLocalBotIgnoreStatus()
    {
        // First, process any pending messages to ensure we have the latest bot info
        ProcessIncomingQueue();
        
        // Broadcast our own ID
        BroadcastLocalBotId();
        
        // Make sure all known local bots are properly tagged
        for (const auto& [friendsID, isLocal] : localBots)
        {
            bool needsUpdate = false;
            
            if (!F::PlayerUtils.HasTag(friendsID, F::PlayerUtils.TagToIndex(IGNORED_TAG)))
            {
                needsUpdate = true;
            }
            
            if (!F::PlayerUtils.HasTag(friendsID, F::PlayerUtils.TagToIndex(FRIEND_TAG)))
            {
                needsUpdate = true;
            }
            
            if (needsUpdate)
            {
                PlayerInfo_t pi{};
                for (int i = 1; i <= I::EngineClient->GetMaxClients(); i++)
                {
                    if (I::EngineClient->GetPlayerInfo(i, &pi) && pi.friendsID == friendsID)
                    {
                        F::PlayerUtils.AddTag(friendsID, F::PlayerUtils.TagToIndex(IGNORED_TAG), true, pi.name);
                        F::PlayerUtils.AddTag(friendsID, F::PlayerUtils.TagToIndex(FRIEND_TAG), true, pi.name);
                        Log("Marked local bot as ignored and friend: " + std::string(pi.name));
                        break;
                    }
                }
            }
        }
    }

    void ClearLocalBots()
    {
        localBots.clear();
        Log("Cleared local bots list");
    }

    void QueueMessage(const std::string& type, const std::string& content, bool isPriority = false)
    {
        messageQueue.push({type, content, isPriority});
    }
    
    void ProcessMessageQueue()
    {
        if(!hPipe.valid()) return;
        std::vector<PendingMessage> batch;
        batch.reserve(16);
        size_t fetched = messageQueue.popBatch(batch, 16, 0);
        if(fetched==0) return;

        for(const auto& msg: batch)
        {
            std::string wire;
            wire = std::to_string(botId==-1?0:botId) + ":" + msg.type + ":" + msg.content + "\n";
            DWORD written=0;
            if(!WriteFile(hPipe, wire.c_str(), static_cast<DWORD>(wire.size()), &written, NULL) || written!=wire.size())
            {
                Log("Failed to write queued message: " + std::to_string(GetLastError()));
                // push back remaining messages for retry
                for(auto it=&msg+1; it< batch.data()+fetched; ++it)
                    messageQueue.push(*it);
                break;
            }
        }
    }

    bool SafeWriteToPipe(const std::string& message)
    {
        if (!hPipe.valid()) {
            QueueMessage("Status", "QueuedMessage", false);
            return false;
        }
        
        DWORD bytesWritten = 0;
        BOOL success = WriteFile(hPipe, message.c_str(), static_cast<DWORD>(message.length()), &bytesWritten, NULL);
        
        if (!success || bytesWritten != message.length()) {
            DWORD error = GetLastError();
            Log("WriteFile failed: " + std::to_string(error) + " - " + GetErrorMessage(error));
            
            if (error == ERROR_BROKEN_PIPE || error == ERROR_PIPE_NOT_CONNECTED) {
                hPipe.reset();
            }
            return false;
        }
        return true;
    }
    
    int GetReconnectDelayMs()
    {
        int delay = std::min(
            BASE_RECONNECT_DELAY_MS * (1 << std::min(currentReconnectAttempts, 10)), 
            MAX_RECONNECT_DELAY_MS
        );
        
        int jitter = delay * 0.2 * (static_cast<double>(rand()) / RAND_MAX - 0.5);
        return delay + jitter;
    }

    void QueueIncomingMessage(const std::string& type, const std::string& content)
    {
        std::lock_guard<std::mutex> lock(inboundMutex);
        inboundQueue.push_back({type, content, false});
    }

    void ProcessIncomingQueue()
    {
        std::lock_guard<std::mutex> lock(inboundMutex);
        
        auto it = inboundQueue.begin();
        while (it != inboundQueue.end()) {
            if (it->type == "Command") {
                ExecuteCommand(it->content);
                it = inboundQueue.erase(it);
            } else if (it->type == "LocalBot") {
                ProcessLocalBotMessage(it->content);
                it = inboundQueue.erase(it);
            } else {
                // Unknown message type, just remove it
                Log("Unknown message type in inbound queue: " + it->type);
                it = inboundQueue.erase(it);
            }
        }
    }

    void ConnectAndMaintainPipe(std::stop_token stoken)
    {
        Log("ConnectAndMaintainPipe started");
        srand(static_cast<unsigned int>(time(nullptr)));
        
        DWORD lastBroadcastTime = 0;
        const DWORD BROADCAST_INTERVAL_MS = 5000;
        
        constexpr DWORD HEARTBEAT_MIN_MS = 2000;
        constexpr DWORD HEARTBEAT_MAX_MS = 4000;
        auto scheduleNextHeartbeat = [&]() -> DWORD {
            return GetTickCount() + HEARTBEAT_MIN_MS + rand() % (HEARTBEAT_MAX_MS - HEARTBEAT_MIN_MS + 1);
        };
        DWORD nextHeartbeatTime = scheduleNextHeartbeat();
        
        DWORD lastPlayerUpdateTime = 0;
        const DWORD PLAYER_UPDATE_INTERVAL_MS = 5500;
        
        DWORD lastHealthUpdateTime = 0;
        const DWORD HEALTH_UPDATE_INTERVAL_MS = 5000;
        int lastHealthSent = -1;
        
        while (!stoken.stop_requested())
        {
            DWORD currentTime = GetTickCount();
            
            ProcessIncomingQueue();
            
            if (I::EngineClient && I::EngineClient->IsInGame() && 
                currentTime - lastBroadcastTime > BROADCAST_INTERVAL_MS) {
                BroadcastLocalBotId();
                lastBroadcastTime = currentTime;
            }
            
            if (!hPipe.valid())
            {
                int reconnectDelay = GetReconnectDelayMs();
                if (currentTime - lastConnectAttemptTime > static_cast<DWORD>(reconnectDelay) || lastConnectAttemptTime == 0)
                {
                    lastConnectAttemptTime = currentTime;
                    currentReconnectAttempts++;
                    
                    Log("Attempting to connect to pipe (attempt " + std::to_string(currentReconnectAttempts) + 
                        ", delay: " + std::to_string(reconnectDelay) + "ms)");
                        

                    OVERLAPPED overlapped = {0};
                    overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
                    
                    if (overlapped.hEvent == NULL) {
                        Log("Failed to create connection event: " + std::to_string(GetLastError()));
                        std::this_thread::sleep_for(std::chrono::milliseconds(reconnectDelay));
                        continue;
                    }
                    
                    hPipe.reset(CreateFile(
                        PIPE_NAME,
                        GENERIC_READ | GENERIC_WRITE,
                        0,
                        NULL,
                        OPEN_EXISTING,
                        FILE_FLAG_OVERLAPPED,
                        NULL));
    
                    if (hPipe.valid())
                    {

                        currentReconnectAttempts = 0;
                        Log("Connected to pipe");
                        

                        DWORD pipeMode = PIPE_READMODE_MESSAGE;
                        SetNamedPipeHandleState(hPipe, &pipeMode, NULL, NULL);
                        

                        QueueMessage("Status", "Connected", true);
                        ProcessMessageQueue();

                        if (botId != -1)
                        {
                            Log("Using Bot ID: " + std::to_string(botId));
                        }
                        else
                        {
                            Log("Warning: Bot ID not set");
                        }
                        
                        ClearLocalBots();
                        
                        nextHeartbeatTime = scheduleNextHeartbeat();
                        lastPlayerUpdateTime = currentTime;
                        lastHealthUpdateTime = currentTime;
                        lastHealthSent = -1;
                    }
                    else
                    {
                        DWORD error = GetLastError();
                        Log("Failed to connect to pipe: " + std::to_string(error) + " - " + GetErrorMessage(error));
                    }
                    
                    CloseHandle(overlapped.hEvent);
                }
                else
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }

            if (hPipe.valid())
            {

                DWORD bytesAvail = 0;
                if (!PeekNamedPipe(hPipe, NULL, 0, NULL, &bytesAvail, NULL)) {
                    DWORD error = GetLastError();
                    if (error == ERROR_BROKEN_PIPE || error == ERROR_PIPE_NOT_CONNECTED || error == ERROR_NO_DATA) {
                        Log("Pipe disconnected: " + std::to_string(error) + " - " + GetErrorMessage(error));
                        hPipe.reset();
                        continue;
                    }
                }
                

                ProcessMessageQueue();
                
                if (I::EngineClient && I::EngineClient->IsInGame() && 
                    currentTime - lastHealthUpdateTime >= HEALTH_UPDATE_INTERVAL_MS) 
                {
                    auto pLocal = H::Entities.GetLocal();
                    if (pLocal) {
                        int currentHealth = pLocal->m_iHealth();
                        if (currentHealth != lastHealthSent || lastHealthSent == -1) {
                            QueueMessage("Health", std::to_string(currentHealth), false);
                            lastHealthSent = currentHealth;
                        }
                    }
                    lastHealthUpdateTime = currentTime;
                }
                
                if(currentTime >= nextHeartbeatTime)
                {
                    QueueMessage("Status", "Heartbeat", true);
                    ProcessMessageQueue();
                    nextHeartbeatTime = scheduleNextHeartbeat();
                }
                
                if (currentTime - lastPlayerUpdateTime >= PLAYER_UPDATE_INTERVAL_MS) 
                {
                    if (I::EngineClient && I::EngineClient->IsInGame()) {
                        SendHealthUpdate();
                        QueueMessage("PlayerClass", std::to_string(GetCurrentPlayerClass()), false);
                        QueueMessage("Map", GetCurrentLevelName(), false);
                        QueueMessage("ServerInfo", "Player", false);
                        UpdateLocalBotIgnoreStatus();
                    }
                    ProcessMessageQueue();
                    
                    lastPlayerUpdateTime = currentTime;
                }
                

                char buffer[4096] = {0};
                DWORD bytesRead = 0;
                

                if(!hReadEvent.valid())
                {
                    hReadEvent.reset(CreateEvent(NULL, TRUE, FALSE, NULL));
                    ovRead = {};
                    ovRead.hEvent = hReadEvent;
                }

                if(hReadEvent.valid())
                {
                    if(!readPending && bytesAvail>0)
                    {
                        readPending = ReadFile(hPipe, buffer, sizeof(buffer)-1, &bytesRead, &ovRead)==FALSE && GetLastError()==ERROR_IO_PENDING;
                    }
                    DWORD waitRes = WaitForSingleObject(hReadEvent, 0);
                    if(waitRes==WAIT_OBJECT_0)
                    {
                        if(GetOverlappedResult(hPipe,&ovRead,&bytesRead,FALSE))
                            readPending=false;
                        ResetEvent(hReadEvent);
                    }

                    if(bytesRead>0)
                    {
                        buffer[bytesRead]='\0';
                        std::string message(buffer, bytesRead);
                        Log("Received message: " + message);
                        std::stringstream ss(message);
                        std::string line;
                        while(std::getline(ss,line))
                        {
                            if(line.empty()) continue;
                            std::istringstream iss(line);
                            std::string botNumber, messageType, content;
                            std::getline(iss, botNumber, ':');
                            std::getline(iss, messageType, ':');
                            std::getline(iss, content);
                            if(messageType=="Command")
                                QueueIncomingMessage(messageType,content);
                            else if(messageType=="LocalBot")
                                QueueIncomingMessage(messageType,line);
                        }
                        bytesRead=0;
                    }

                }
            }
            
            // Adaptive wait: if we have messages pending we loop immediately, otherwise block until either
            // a new message arrives or timeout expires (different timeout when pipe connected vs not)
            if(!hPipe.valid())
            {
                messageQueue.waitForMessage(100);
            }
            else
            {
                messageQueue.waitForMessage(500);
            }
        }


        {
            messageQueue.clear();
        }
        
        if (hPipe.valid())
        {

            try {
                if (botId != -1) {
                    std::string message = std::to_string(botId) + ":Status:Disconnecting\n";
                    DWORD bytesWritten = 0;
                    WriteFile(hPipe, message.c_str(), static_cast<DWORD>(message.length()), &bytesWritten, NULL);
                }
            } catch (...) {
                // Ignore any errors during shutdown
            }
            
            hPipe.reset();
            if(hReadEvent.valid()) hReadEvent.reset(); readPending=false;
        }
        Log("ConnectAndMaintainPipe ended");
    }
} 