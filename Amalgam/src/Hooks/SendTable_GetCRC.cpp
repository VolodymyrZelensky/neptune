#include "../SDK/SDK.h"

MAKE_SIGNATURE(SendTable_GetCRC, "engine.dll", "8B 05 ? ? ? ? C3 CC CC CC CC CC CC CC CC CC 48 83 EC ? 48 8B 41", 0x0);

MAKE_HOOK(SendTable_GetCRC, S::SendTable_GetCRC(), unsigned int)
{
    unsigned int value = CALL_ORIGINAL();
    SDK::Output("SendTable_GetCRC", std::to_string(value).c_str());
    return value;
}