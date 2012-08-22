/*
Copyright (c) 2012 Vladimir Jimenez
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.
3. The name of the author may not be used to endorse or promote products
     derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


*** teamSwitch Details ***
Author:
Vladimir Jimenez (allejo)

Description:
Switch players during game play by bypassing the API

Slash Commands:
/switch

License:
BSD

Version:
1.0
*/

#include "allejoian.h"
#include "bzfsAPI.h"
#include <cmath>
#include "../../src/bzfs/GameKeeper.h"
#include "../../src/bzfs/bzfs.h"
#include "../../src/bzfs/CmdLineOptions.h"

class teamSwitch : public bz_Plugin, public bz_CustomSlashCommandHandler
{
public:
    virtual const char* Name (){return "Team Switch";}
    virtual void Init(const char* /*commandLine*/);
    virtual void Cleanup(void);

    virtual void Event(bz_EventData *eventData);
    virtual bool SlashCommand(int playerID, bz_ApiString command, bz_ApiString message, bz_APIStringList *params);
    
    bool teamsUneven;
    double timeFirstUneven;
    bz_eTeamType strongTeam;
};

BZ_PLUGIN(teamSwitch);

void teamSwitch::Init(const char* /*commandLine*/)
{
    Register(bz_eCaptureEvent);
    Register(bz_eTickEvent);

    bz_registerCustomSlashCommand("switch", this);
    
    teamsUneven = false;

    bz_debugMessage(4,"teamSwitch plugin loaded");
}

void teamSwitch::Cleanup()
{
    Flush();
    bz_removeCustomSlashCommand("switch");

    bz_debugMessage(4,"teamSwitch plugin unloaded");
}

void teamSwitch::Event(bz_EventData* eventData)
{
    switch (eventData->eventType)
    {
        case bz_eCaptureEvent: // A flag is captured
        {
            bz_CTFCaptureEventData_V1* ctfdata = (bz_CTFCaptureEventData_V1*)eventData;
            
            if (bz_getTeamCount(eRedTeam) == 0 || bz_getTeamCount(eBlueTeam) == 0) break;
            
            if (bz_getTeamCount(eBlueTeam)/double(bz_getTeamCount(eRedTeam)) > 1.3 || bz_getTeamCount(eRedTeam)/double(bz_getTeamCount(eBlueTeam)) > 1.3)
            {
                if (bz_getTeamCount(eRedTeam) > bz_getTeamCount(eBlueTeam)) strongTeam = eRedTeam;
                else strongTeam = eBlueTeam;
                
                if (strongTeam == eRedTeam)
                {
                    switchPlayer(ctfdata->playerCapping, "blue");
                    bz_killPlayer(ctfdata->playerCapping, 0);
                    bz_sendTextMessage(BZ_SERVER, ctfdata->playerCapping, "You're an asshole for capping unfairly! You've been moved to the blue team.");
                }
                else
                {
                    switchPlayer(ctfdata->playerCapping, "red");
                    bz_killPlayer(ctfdata->playerCapping, 0);
                    bz_sendTextMessage(BZ_SERVER, ctfdata->playerCapping, "You're an asshole for capping unfairly! You've been moved to the red team.");
                }
            }
        }
        break;

        case bz_eTickEvent: // The server's main loop has iterated
        {
            bz_TickEventData_V1* tickdata = (bz_TickEventData_V1*)eventData;
            
            if (bz_getTeamCount(eRedTeam) == 0 || bz_getTeamCount(eBlueTeam) == 0) break;
            
            double redToBlueRatio = bz_getTeamCount(eRedTeam)/double(bz_getTeamCount(eBlueTeam));
            double blueToRedRatio = bz_getTeamCount(eBlueTeam)/double(bz_getTeamCount(eRedTeam));
            
            if ((redToBlueRatio > 1.3 || blueToRedRatio > 1.3) && !teamsUneven)
            {
                if (redToBlueRatio > 1.3) strongTeam = eRedTeam;
                else strongTeam = eBlueTeam;
                
                teamsUneven = true;
                timeFirstUneven = tickdata->eventTime;
                bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "Teams are uneven, please don't be dicks and even them out.");
            }
            else if (teamsUneven)
            {
                if (strongTeam == eRedTeam)
                {
                    if (redToBlueRatio < 1.3)
                    {
                        teamsUneven = false;
                        bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "Thank you for evening out the teams.");
                    }
                }
                else
                {
                    if (blueToRedRatio < 1.3)
                    {
                        teamsUneven = false;
                        bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "Thank you for evening out the teams.");
                    }
                }
            }
            
            if (teamsUneven && (timeFirstUneven + 30 < bz_getCurrentTime()))
            {
                bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "Balancing unfair teams...");
                
                bz_eTeamType strongTeam;
                if (bz_getTeamCount(eRedTeam) > bz_getTeamCount(eBlueTeam)) strongTeam = eRedTeam;
                else strongTeam = eBlueTeam;
                
                int teamDifference = abs(bz_getTeamCount(eRedTeam) - bz_getTeamCount(eBlueTeam));
                if (teamDifference/2 != 0) teamDifference - 1;
                
                int playersToMove = teamDifference/2;
                int playerMoved = 0;
                for (int i = 0; i < playersToMove; i++)
                {
                    playerMoved = bz_randomPlayer(strongTeam);
                    
                    if (strongTeam == eBlueTeam)
                    {
                        switchPlayer(playerMoved, "red");
                        bz_sendTextMessage(BZ_SERVER, playerMoved, "You have been switched to the communist team.");
                    }
                    else if (strongTeam == eRedTeam)
                    {
                        switchPlayer(playerMoved, "blue");
                        bz_sendTextMessage(BZ_SERVER, playerMoved, "You have been switched to the blue team.");
                    }
                }
                
                teamsUneven = false;
            }
        }
        break;

        default:
        break;
    }
}

bool teamSwitch::SlashCommand(int playerID, bz_ApiString command, bz_ApiString message, bz_APIStringList *params)
{
    if (command == "switch" && bz_hasPerm(playerID, "switch"))
    {
        if (params->size() != 1 && params->size() != 2)
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "Syntax: /switch <player slot> rogue|red|green|blue|purple|observer");
            return true;
        }
        
        int myID;
        std::string teamToSwitchTo;
        
        if (params->size() == 2)
        {
            myID = atoi(std::string(params->get(0).c_str()).erase(0,1).c_str());
            teamToSwitchTo = params->get(1).c_str();
        }
        else
        {
            myID = playerID;
            teamToSwitchTo = params->get(0).c_str();
        }
        
        if (isValidPlayerID(myID))
        {
            bz_PlayerRecordV2* pr = (bz_PlayerRecordV2*)bz_getPlayerByIndex(myID);
            
            if (pr->team == eObservers && pr->motto == "bzadmin")
            {
                bz_sendTextMessage(BZ_SERVER, playerID, "Warning: In order to prevent bzadmin clients from crashing, you cannot 'switch' bzadmin clients.");
                bz_freePlayerRecord(pr);
                return true;
            }
            
            bz_freePlayerRecord(pr);
            
            if (strcmp(teamToSwitchTo.c_str(), "rogue") == 0 ||
                strcmp(teamToSwitchTo.c_str(), "red")  == 0 ||
                strcmp(teamToSwitchTo.c_str(), "blue")  == 0 ||
                strcmp(teamToSwitchTo.c_str(), "green")  == 0 ||
                strcmp(teamToSwitchTo.c_str(), "purple")  == 0 ||
                strcmp(teamToSwitchTo.c_str(), "observer") == 0)
            {
                int maxPlayersOnTeam = 0;
                if (strcmp(teamToSwitchTo.c_str(), "rogue") == 0) maxPlayersOnTeam = bz_getTeamPlayerLimit(eRogueTeam);
                if (strcmp(teamToSwitchTo.c_str(), "red") == 0) maxPlayersOnTeam = bz_getTeamPlayerLimit(eRedTeam);
                if (strcmp(teamToSwitchTo.c_str(), "blue") == 0) maxPlayersOnTeam = bz_getTeamPlayerLimit(eBlueTeam);
                if (strcmp(teamToSwitchTo.c_str(), "green") == 0) maxPlayersOnTeam = bz_getTeamPlayerLimit(eGreenTeam);
                if (strcmp(teamToSwitchTo.c_str(), "purple") == 0) maxPlayersOnTeam = bz_getTeamPlayerLimit(ePurpleTeam);
                if (strcmp(teamToSwitchTo.c_str(), "observer") == 0) maxPlayersOnTeam = bz_getTeamPlayerLimit(eObservers);
            
                if (maxPlayersOnTeam > 0)
                {
                    if (playerID != myID)
                    {
                        switchPlayer(myID, teamToSwitchTo);
                        bz_sendTextMessagef(BZ_SERVER, myID, "You've been switched to the %s team by %s", teamToSwitchTo.c_str(), bz_getPlayerByIndex(playerID)->callsign.c_str());
                    }
                    else
                    {
                        switchPlayer(playerID, teamToSwitchTo);
                        bz_sendTextMessagef(BZ_SERVER, myID, "You've been switched to the %s team.", teamToSwitchTo.c_str());
                    }
                }
                else
                {
                    bz_sendTextMessagef(BZ_SERVER, playerID, "The %s team does not exist on this map.", teamToSwitchTo.c_str());
                }
            }
            else
            {
                bz_sendTextMessagef(BZ_SERVER, playerID, "The %s team does not exist.", teamToSwitchTo.c_str());
            }
        }
        else
        {
            bz_sendTextMessagef(BZ_SERVER, playerID, "player \"%s\" not found", params->get(0).c_str());
        }
    }
    else
    {
        bz_sendTextMessage(BZ_SERVER, playerID, "Unknown command [switch]");
    }
}

