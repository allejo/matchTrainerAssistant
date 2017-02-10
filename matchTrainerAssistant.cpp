/*
 Copyright (C) 2016-2017 Vladimir "allejo" Jimenez

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

    virtual void reloadAliases (void);

    struct SpawnAlias
    {
        std::string name;
        std::string desc;
        float position[3];
        float rotation;
    };

    bool  handleSpawn[256];
    float nextSpawnLocation[256][4];
    std::string aliasesFile;
    std::map<std::string, SpawnAlias> aliases;
};

BZ_PLUGIN(MatchTrainerAssistant)

const char* MatchTrainerAssistant::Name ()
{
    static std::string pluginName;

    if (pluginName.empty())
        pluginName = bztk_pluginName(PLUGIN_NAME, MAJOR, MINOR, REV, BUILD);

    return pluginName.c_str();
}

void MatchTrainerAssistant::Init (const char* commandLine)
{
    // Register our events with Register()
    Register(bz_eAllowCTFCaptureEvent);
    Register(bz_eGetPlayerSpawnPosEvent);
    Register(bz_ePlayerDieEvent);
    Register(bz_ePlayerJoinEvent);
    Register(bz_ePlayerPartEvent);

    // Register our custom slash commands
    bz_registerCustomSlashCommand("reload", this);
    bz_registerCustomSlashCommand("spawn", this);
    bz_registerCustomSlashCommand("flag", this);
    bz_registerCustomSlashCommand("die", this);

    aliasesFile = commandLine;
}

void MatchTrainerAssistant::Cleanup (void)
{
    Flush(); // Clean up all the events

    // Clean up our custom slash commands
    bz_removeCustomSlashCommand("reload");
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
                if (isnan(nextSpawnLocation[spawnData->playerID][i]))
                {
                    return;
                }
            }

            if (handleSpawn[spawnData->playerID])
            {
                spawnData->handled = true;
                spawnData->pos[0]  = nextSpawnLocation[spawnData->playerID][0];
                spawnData->pos[1]  = nextSpawnLocation[spawnData->playerID][1];
                spawnData->pos[2]  = nextSpawnLocation[spawnData->playerID][2];
                spawnData->rot     = nextSpawnLocation[spawnData->playerID][3];

                handleSpawn[spawnData->playerID] = false;
            }
        }
        break;

        case bz_ePlayerDieEvent: // This event is called each time a tank is killed.
        {
            bz_PlayerDieEventData_V1* dieData = (bz_PlayerDieEventData_V1*)eventData;

            // If they aren't killed by the server, save their last position. This is done because whenever someone does a /spawn, they are killed in order to
            // be respawned elsewhere
            if (dieData->killerID != ServerPlayer)
            {
                nextSpawnLocation[dieData->playerID][0] = dieData->state.pos[0];
                nextSpawnLocation[dieData->playerID][1] = dieData->state.pos[1];
                nextSpawnLocation[dieData->playerID][2] = dieData->state.pos[2];
                nextSpawnLocation[dieData->playerID][3] = dieData->state.rotation;
            }
        }
        break;

        case bz_ePlayerJoinEvent:
        case bz_ePlayerPartEvent:
        {
            bz_PlayerJoinPartEventData_V1* joinPartData = (bz_PlayerJoinPartEventData_V1*)eventData;

            handleSpawn[joinPartData->playerID] = false;

            nextSpawnLocation[joinPartData->playerID][0] = NAN;
            nextSpawnLocation[joinPartData->playerID][1] = NAN;
            nextSpawnLocation[joinPartData->playerID][2] = NAN;
            nextSpawnLocation[joinPartData->playerID][3] = NAN;
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

    if (command == "reload" && params->get(0) == "aliases")
    {
        if (bz_hasPerm(playerID, "setAll"))
        {
            reloadAliases();
            bz_sendTextMessage(BZ_SERVER, playerID, "spawn aliases reloaded");
        }
        else
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "permission denied");
        }

        return true;
    }
    else if (command == "spawn")
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
                bz_sendTextMessage(BZ_SERVER, playerID, "");
                bz_sendTextMessage(BZ_SERVER, playerID, "/spawn <alias>");

                for (auto alias : aliases)
                {
                    bz_sendTextMessagef(BZ_SERVER, playerID, "     %s - %s", alias.first.c_str(), alias.second.desc.c_str());
                }

                return true;
            }
            else if (params->size() == 1)
            {
                std::string targetStr = params->get(0);

                if (aliases.find(targetStr) != aliases.end())
                {
                    nextSpawnLocation[playerID][0] = aliases[targetStr].position[0];
                    nextSpawnLocation[playerID][1] = aliases[targetStr].position[1];
                    nextSpawnLocation[playerID][2] = aliases[targetStr].position[1];
                    nextSpawnLocation[playerID][3] = aliases[targetStr].rotation;
                }
                else
                {
                    std::unique_ptr<bz_BasePlayerRecord> target(bz_getPlayerBySlotOrCallsign(targetStr.c_str()));

                    if (!target)
                    {
                        bz_sendTextMessagef(BZ_SERVER, playerID, "player %s not found", targetStr.c_str());
                        return true;
                    }

                    nextSpawnLocation[playerID][0] = target->lastKnownState.pos[0];
                    nextSpawnLocation[playerID][1] = target->lastKnownState.pos[1];
                    nextSpawnLocation[playerID][2] = target->lastKnownState.pos[2];
                    nextSpawnLocation[playerID][3] = target->lastKnownState.rotation;
                }
            }
            else
            {
                float posX = std::atof(params->get(0).c_str());
                float posY = std::atof(params->get(1).c_str());
                float posZ = std::atof(params->get(2).c_str());
                float rot  = std::atof(params->get(3).c_str());

                if (std::abs(posX) > maxXY || std::abs(posY) > maxXY)
                {
                    bz_sendTextMessagef(BZ_SERVER, playerID, "The following %s value (%.2f) is outside of this world",
                                        (std::abs(posX) > maxXY) ? "X" : "Y",
                                        (std::abs(posX) > maxXY) ? posX : posY);

                    return true;
                }

                float maxZ = bz_getWorldMaxHeight();

                if (posZ < 0 || (maxZ > 0 && posZ > maxZ))
                {
                    bz_sendTextMessagef(BZ_SERVER, playerID, "The following Z value (%.2f) is outside of this world", posZ);

                    return true;
                }

                nextSpawnLocation[playerID][0] = posX;
                nextSpawnLocation[playerID][1] = posY;
                nextSpawnLocation[playerID][2] = posZ;
                nextSpawnLocation[playerID][3] = rot;
            }
        }

        if (pr->spawned)
        {
            bz_killPlayer(playerID, ServerPlayer);
        }

        handleSpawn[playerID] = true;
        bztk_forcePlayerSpawn(playerID);

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
        bz_killPlayer(playerID, false, playerID);

        return true;
    }

    return false;
}

void MatchTrainerAssistant::reloadAliases()
{
    if (!aliasesFile.empty())
    {
        aliases.clear();

        std::vector<std::string> fileContents;
        bztk_fileToVector(aliasesFile.c_str(), fileContents, true, false);

        for (auto line : fileContents)
        {
            bz_APIStringList nodes;
            nodes.tokenize(line.c_str(), ",");

            std::string alias = bz_tolower(nodes.get(0).c_str());

            SpawnAlias a;
            a.name = alias;
            a.desc = nodes.get(1).c_str();
            a.position[0] = std::stof(nodes.get(2).c_str());
            a.position[1] = std::stof(nodes.get(3).c_str());
            a.position[2] = std::stof(nodes.get(4).c_str());
            a.rotation = std::stof(nodes.get(5).c_str());

            aliases[alias] = a;
        }
    }
}
