#include "HitSounds.h"
#include "HitSoundBytes.h"
#include <Windows.h>
#include <mmsystem.h>
#include <vector>
#include <mutex>
#include <string>
#include <fstream>
#pragma comment(lib, "winmm.lib")

namespace HitSounds
{
    static std::string GetTempSoundPath(int index) {
        char tempPath[MAX_PATH];
        GetTempPathA(MAX_PATH, tempPath);
        return std::string(tempPath) + "cheat_hitsound_" + std::to_string(index) + ".wav";
    }
    
    static bool channelInUse[32] = { false };
    static std::mutex channelMutex;

    static int GetFreeChannel() {
        std::lock_guard<std::mutex> lock(channelMutex);
        for (int i = 0; i < 32; i++) {
            if (!channelInUse[i]) {
                channelInUse[i] = true;
                return i;
            }
        }
        return 0;
    }
    
    static void ReleaseChannel(int channel) {
        std::lock_guard<std::mutex> lock(channelMutex);
        channelInUse[channel] = false;
    }

    void SetVolume(float volume) {
        int volumeValue = static_cast<int>(volume * 500.0f);
        for (int i = 0; i < 32; i++) {
            std::string cmd = "setaudio hitsound_" + std::to_string(i) + " volume to " + std::to_string(volumeValue);
            mciSendStringA(cmd.c_str(), NULL, 0, NULL);
        }
    }
    
    void PlayHitSound(Type type)
    {
        if (!I::EngineClient)
            return;

        if (type == Type::Default)
            return;

        // Sprawdź głośność - jeśli jest 0, nie odtwarzaj dźwięku
        float volume = Vars::Misc::Sound::HitsoundVolume.Value;
        if (volume <= 0.0f) 
            return;

        const unsigned char* soundBytes = nullptr;
        size_t soundSize = 0;

        switch (type)
        {
            case Type::Bonk:
                soundBytes = ::HitSounds::Bytes::BONK;
                soundSize = ::HitSounds::Bytes::BONK_SIZE;
                break;
            case Type::COD:
                soundBytes = ::HitSounds::Bytes::COD;
                soundSize = ::HitSounds::Bytes::COD_SIZE;
                break;
            case Type::Quake:
                soundBytes = ::HitSounds::Bytes::QUAKE;
                soundSize = ::HitSounds::Bytes::QUAKE_SIZE;
                break;
            case Type::Moan:
                soundBytes = ::HitSounds::Bytes::MOAN;
                soundSize = ::HitSounds::Bytes::MOAN_SIZE;
                break;
            default:
                return;
        }

        if (!soundBytes || soundSize == 0) 
            return;

        int channel = GetFreeChannel();
            
        CreateThread(NULL, 0, [](LPVOID param) -> DWORD {
            int channelIndex = (int)(INT_PTR)param;
            
            std::string aliasName = "hitsound_" + std::to_string(channelIndex);
            std::string filePath = GetTempSoundPath(channelIndex);
            
            const unsigned char* soundData = nullptr;
            size_t soundSize = 0;
            
            Type soundType = static_cast<Type>(Vars::Misc::Sound::HitsoundType.Value);
            switch (soundType) {
                case Type::Bonk:
                    soundData = ::HitSounds::Bytes::BONK;
                    soundSize = ::HitSounds::Bytes::BONK_SIZE;
                    break;
                case Type::COD:
                    soundData = ::HitSounds::Bytes::COD;
                    soundSize = ::HitSounds::Bytes::COD_SIZE;
                    break;
                case Type::Quake:
                    soundData = ::HitSounds::Bytes::QUAKE;
                    soundSize = ::HitSounds::Bytes::QUAKE_SIZE;
                    break;
                case Type::Moan:
                    soundData = ::HitSounds::Bytes::MOAN;
                    soundSize = ::HitSounds::Bytes::MOAN_SIZE;
                    break;
                default:
                    return 0;
            }
            
            std::ofstream outFile(filePath, std::ios::binary);
            if (!outFile) {
                ReleaseChannel(channelIndex);
                return 0;
            }
            
            outFile.write(reinterpret_cast<const char*>(soundData), soundSize);
            outFile.close();
            
            std::string closeCmd = "close " + aliasName;
            mciSendStringA(closeCmd.c_str(), NULL, 0, NULL);
            
            std::string openCmd = "open \"" + filePath + "\" type waveaudio alias " + aliasName;
            if (mciSendStringA(openCmd.c_str(), NULL, 0, NULL) != 0) {
                DeleteFileA(filePath.c_str());
                ReleaseChannel(channelIndex);
                return 0;
            }
            
            int volumeValue = static_cast<int>(Vars::Misc::Sound::HitsoundVolume.Value * 1000.0f);
            std::string volumeCmd = "setaudio " + aliasName + " volume to " + std::to_string(volumeValue);
            mciSendStringA(volumeCmd.c_str(), NULL, 0, NULL);
            
            std::string playCmd = "play " + aliasName;
            mciSendStringA(playCmd.c_str(), NULL, 0, NULL);
            
            std::string statusCmd = "status " + aliasName + " mode";
            char returnString[128];
            while (true) {
                mciSendStringA(statusCmd.c_str(), returnString, sizeof(returnString), NULL);
                if (strcmp(returnString, "stopped") == 0) {
                    break;
                }
            }
            
            mciSendStringA(closeCmd.c_str(), NULL, 0, NULL);
            DeleteFileA(filePath.c_str());
            ReleaseChannel(channelIndex);
            
            return 0;
        }, (LPVOID)(INT_PTR)channel, 0, NULL);
    }

    void OnPlayerHurt(IGameEvent* pEvent)
    {
        if (!pEvent)
            return;

        PlayerInfo_t pi{};
        if (!I::EngineClient->GetPlayerInfo(I::EngineClient->GetLocalPlayer(), &pi))
            return;

        int nAttacker = pEvent->GetInt("attacker");
        if (nAttacker != pi.userID)
            return;

        PlayHitSound(static_cast<Type>(Vars::Misc::Sound::HitsoundType.Value));
    }
}