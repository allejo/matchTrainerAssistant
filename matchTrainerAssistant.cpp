/*
 Copyright (C) 2015 Vladimir "allejo" Jimenez

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 */

#include <cmath>
#include <sstream>

#include "bzfsAPI.h"
#include "bztoolkit/bzToolkitAPI.h"

// Define plug-in name
const std::string PLUGIN_NAME = "Match Trainer Assistant";

// Define plug-in version numbering
const int MAJOR = 1;
const int MINOR = 0;
const int REV = 1;
const int BUILD = 5;

class MatchTrainerAssistant : public bz_Plugin, public bz_CustomSlashCommandHandler
{
public:
    virtual const char* Name ();
    virtual void Init (const char* config);
    virtual void Event (bz_EventData *eventData);
    virtual void Cleanup (void);

    virtual bool SlashCommand (int playerID, bz_ApiString, bz_ApiString, bz_APIStringList*);

    bool  protectedPos[256];
    bool  handleSpawn[256];
    float lastDeaths[256][4];
};

BZ_PLUGIN(MatchTrainerAssistant)

const char* MatchTrainerAssistant::Name (void)
{
    static std::string pluginBuild = "";

    if (!pluginBuild.size())
    {
        std::ostringstream pluginBuildStream;

        pluginBuildStream << PLUGIN_NAME << " " << MAJOR << "." << MINOR << "." << REV << " (" << BUILD << ")";
        pluginBuild = pluginBuildStream.str();
    }

    return pluginBuild.c_str();
}

void MatchTrainerAssistant::Init (const char* /*commandLine*/)
{
    // Register our events with Register()
    Register(bz_eAllowCTFCaptureEvent);
    Register(bz_eGetPlayerSpawnPosEvent);
    Register(bz_ePlayerDieEvent);
    Register(bz_ePlayerJoinEvent);
    Register(bz_ePlayerPartEvent);

    // Register our custom slash commands
    bz_registerCustomSlashCommand("spawn", this);
    bz_registerCustomSlashCommand("flag", this);
    bz_registerCustomSlashCommand("die", this);
}

void MatchTrainerAssistant::Cleanup (void)
{
    Flush(); // Clean up all the events

    // Clean up our custom slash commands
    bz_removeCustomSlashCommand("spawn");
    bz_removeCustomSlashCommand("flag");
    bz_removeCustomSlashCommand("die");
}

void MatchTrainerAssistant::Event (bz_EventData *eventData)
{
    switch (eventData->eventType)
    {
        case bz_eAllowCTFCaptureEvent:
        {
            bz_AllowCTFCaptureEventData_V1* allowCtfData = (bz_AllowCTFCaptureEventData_V1*)eventData;

            allowCtfData->allow = false;
        }
        break;

        case bz_eGetPlayerSpawnPosEvent:
        {
            bz_GetPlayerSpawnPosEventData_V1* spawnData = (bz_GetPlayerSpawnPosEventData_V1*)eventData;

            for (int i = 0; i < 4; i++)
            {
                if (isnan(lastDeaths[spawnData->playerID][i]))
                {
                    return;
                }
            }

            if (handleSpawn[spawnData->playerID])
            {
                spawnData->handled = handleSpawn[spawnData->playerID];
                spawnData->pos[0]  = lastDeaths[spawnData->playerID][0];
                spawnData->pos[1]  = lastDeaths[spawnData->playerID][1];
                spawnData->pos[2]  = lastDeaths[spawnData->playerID][2];
                spawnData->rot     = lastDeaths[spawnData->playerID][3];

                protectedPos[spawnData->playerID] = false;
                handleSpawn[spawnData->playerID] = false;
            }
        }
        break;

        case bz_ePlayerDieEvent: // This event is called each time a tank is killed.
        {
            bz_PlayerDieEventData_V1* dieData = (bz_PlayerDieEventData_V1*)eventData;

            if (!protectedPos[dieData->playerID])
            {
                lastDeaths[dieData->playerID][0] = dieData->state.pos[0];
                lastDeaths[dieData->playerID][1] = dieData->state.pos[1];
                lastDeaths[dieData->playerID][2] = dieData->state.pos[2];
                lastDeaths[dieData->playerID][3] = dieData->state.rotation;
            }

            protectedPos[dieData->playerID] = false;
        }
        break;

        case bz_ePlayerJoinEvent:
        case bz_ePlayerPartEvent:
        {
            bz_PlayerJoinPartEventData_V1* joinPartData = (bz_PlayerJoinPartEventData_V1*)eventData;

            protectedPos[joinPartData->playerID] = false;
            handleSpawn[joinPartData->playerID] = false;

            lastDeaths[joinPartData->playerID][0] = NAN;
            lastDeaths[joinPartData->playerID][1] = NAN;
            lastDeaths[joinPartData->playerID][2] = NAN;
            lastDeaths[joinPartData->playerID][3] = NAN;
        }
        break;

        default: break;
    }
}


bool MatchTrainerAssistant::SlashCommand(int playerID, bz_ApiString command, bz_ApiString /*message*/, bz_APIStringList *params)
{
    std::unique_ptr<bz_BasePlayerRecord> pr(bz_getPlayerByIndex(playerID));

    if (pr->team == eObservers && command != "flag")
    {
        bz_sendTextMessage(BZ_SERVER, playerID, "These commands are intended for players only.");

        return true;
    }

    if (command == "spawn")
    {
        int maxXY = bz_getBZDBInt("_worldSize") / 2;

        if (params->size() > 0)
        {
            if (params->size() != 1 && params->size() != 4)
            {
                bz_sendTextMessage(BZ_SERVER, playerID, "Spawn Command Usage");
                bz_sendTextMessage(BZ_SERVER, playerID, "---");
                bz_sendTextMessage(BZ_SERVER, playerID, "/spawn");
                bz_sendTextMessage(BZ_SERVER, playerID, "   - Respawn at the location of your last death");
                bz_sendTextMessage(BZ_SERVER, playerID, "/spawn <id|callsign>");
                bz_sendTextMessage(BZ_SERVER, playerID, "   - Respawn at the location of another player's location or location of death");
                bz_sendTextMessage(BZ_SERVER, playerID, "/spawn <x> <y> <z> <rotation>");
                bz_sendTextMessage(BZ_SERVER, playerID, "   - Respawn at specified location; don't spawn inside a building!");

                return true;
            }
            else if (params->size() == 1)
            {
                std::unique_ptr<bz_BasePlayerRecord> target(bz_getPlayerBySlotOrCallsign(params->get(0).c_str()));

                if (target == NULL)
                {
                    bz_sendTextMessagef(BZ_SERVER, playerID, "player %s not found", params->get(0).c_str());
                    return true;
                }

                lastDeaths[playerID][0] = target->lastKnownState.pos[0];
                lastDeaths[playerID][1] = target->lastKnownState.pos[1];
                lastDeaths[playerID][2] = target->lastKnownState.pos[2];
                lastDeaths[playerID][3] = target->lastKnownState.rotation;
            }
            else
            {
                float posX = std::atof(params->get(0).c_str());
                float posY = std::atof(params->get(1).c_str());
                float posZ = std::atof(params->get(2).c_str());

                if (abs(posX) > maxXY || abs(posY) > maxXY)
                {
                    bz_sendTextMessagef(BZ_SERVER, playerID, "The following %s value (%.2f) is outside of this world",
                                        (abs(posX) > maxXY) ? "X" : "Y",
                                        (abs(posX) > maxXY) ? posX : posY);

                    return true;
                }

                float maxZ = bz_getWorldMaxHeight();

                if (maxZ > 0 && posZ > maxZ)
                {
                    bz_sendTextMessagef(BZ_SERVER, playerID, "The following Z value (%.2f) is outside of this world", posZ);

                    return true;
                }

                lastDeaths[playerID][0] = std::atof(params->get(0).c_str());
                lastDeaths[playerID][1] = std::atof(params->get(1).c_str());
                lastDeaths[playerID][2] = std::atof(params->get(2).c_str());
                lastDeaths[playerID][3] = std::atof(params->get(3).c_str());
            }
        }

        if (pr->spawned)
        {
            bz_killPlayer(playerID, BZ_SERVER);
        }

        bztk_forcePlayerSpawn(playerID);
        handleSpawn[playerID] = true;

        return true;
    }
    else if (command == "flag")
    {
        if (params->size() == 0 && pr->team != eObservers)
        {
            bz_givePlayerFlag(playerID, bztk_getFlagFromTeam(pr->team), false);

            return true;
        }

        return false;
    }
    else if (command == "die")
    {
        protectedPos[playerID] = true;
        bz_killPlayer(playerID, BZ_SERVER);

        return true;
    }

    return false;
}