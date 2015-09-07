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

#include "bzfsAPI.h"
#include "plugin_utils.h"

class MatchTrainerAssistant : public bz_Plugin, public bz_CustomSlashCommandHandler
{
public:
    virtual const char* Name () {return "Match Trainer Assistant";}
    virtual void Init (const char* config);
    virtual void Event (bz_EventData *eventData);
    virtual void Cleanup (void);

    virtual bool SlashCommand (int playerID, bz_ApiString, bz_ApiString, bz_APIStringList*);

    float lastDeaths[256][3];
};

BZ_PLUGIN(MatchTrainerAssistant)

void MatchTrainerAssistant::Init (const char* commandLine)
{
    // Register our events with Register()
    Register(bz_ePlayerDieEvent);

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
        case bz_ePlayerDieEvent: // This event is called each time a tank is killed.
        {
            bz_PlayerDieEventData_V1* dieData = (bz_PlayerDieEventData_V1*)eventData;
            std::unique_ptr<bz_BasePlayerRecord> pr(bz_getPlayerByIndex(dieData->playerID));

            lastDeaths[dieData->playerID] = dieData->state.pos;

            // Data
            // ---
            //   (int)                   playerID       - ID of the player who was killed.
            //   (bz_eTeamType)          team           - The team the killed player was on.
            //   (int)                   killerID       - The owner of the shot that killed the player, or BZ_SERVER for server side kills
            //   (bz_eTeamType)          killerTeam     - The team the owner of the shot was on.
            //   (bz_ApiString)          flagKilledWith - The flag name the owner of the shot had when the shot was fired.
            //   (int)                   shotID         - The shot ID that killed the player, if the player was not killed by a shot, the id will be -1.
            //   (bz_PlayerUpdateState)  state          - The state record for the killed player at the time of the event
            //   (double)                eventTime      - Time of the event on the server.
        }
        break;

        default: break;
    }
}


bool MatchTrainerAssistant::SlashCommand(int playerID, bz_ApiString command, bz_ApiString /*message*/, bz_APIStringList *params)
{
    if (command == "spawn")
    {

        return true;
    }
    else if (command == "flag")
    {
        return false;
    }
    else if (command == "die")
    {
        bz_killPlayer(playerID, BZ_SERVER);

        return true;
    }

    return false;
}