#pragma once

#include "../../../SDK/SDK.h"
#include "../../Configs/Configs.h"
#include <string>

namespace F::NamedPipe
{
    void Initialize();
    void Shutdown();
    void SendStatusUpdate(const std::string& status);
    void ExecuteCommand(const std::string& command);
    void SendHealthUpdate();
    void SendPlayerClassUpdate(int playerClass);
    int GetCurrentPlayerClass();
    std::string GetCurrentLevelName();
    void SendMapUpdate();
    void SendServerInfo();

    // Local bot tracking
    void BroadcastLocalBotId();
    void ProcessLocalBotMessage(const std::string& message);
    bool IsLocalBot(uint32_t friendsID);
    void UpdateLocalBotIgnoreStatus();
    void ClearLocalBots();

    // Main-thread inbound pipe processing
    void QueueIncomingMessage(const std::string& type, const std::string& content);
    void ProcessIncomingQueue();
} 