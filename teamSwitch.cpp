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
/switch <player slot> <color>
/balance

License:
BSD

Version:
1.2
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
    virtual void Init(const char* commandLine);
    virtual void Cleanup(void);

    virtual void Event(bz_EventData *eventData);
    virtual bool SlashCommand(int playerID, bz_ApiString command, bz_ApiString message, bz_APIStringList *params);
    
    virtual void balanceTeams(void);
    
    bool alwaysBalanceTeams;
    bool balanceTeamsOnCap;
    
    bool teamsUneven;
    double timeFirstUneven;
    bz_eTeamType strongTeam, teamOne, teamTwo;
    std::string teamOneColor, teamTwoColor;
};

BZ_PLUGIN(teamSwitch);

void teamSwitch::Init(const char* commandLine)
{
    bz_debugMessage(1,"teamSwitch plugin loaded");
    
    Register(bz_eBZDBChange);
    Register(bz_eCaptureEvent);
    Register(bz_eTickEvent);

    bz_registerCustomSlashCommand("switch", this);
    bz_registerCustomSlashCommand("balance", this);
    
    teamOne = eNoTeam;
    teamTwo = eNoTeam;
    
    while (teamOne == eNoTeam || teamTwo == eNoTeam)
    {
        if (bz_getTeamPlayerLimit(eRedTeam) > 0 && teamOne == eNoTeam)
        {
            teamOne = eRedTeam;
            teamOneColor = "red";
            continue;
        }
        if (bz_getTeamPlayerLimit(eGreenTeam) > 0 && teamOne == eNoTeam)
        {
            teamOne = eGreenTeam;
            teamOneColor = "green";
            continue;
        }
        if (bz_getTeamPlayerLimit(eBlueTeam) > 0 && teamOne == eNoTeam)
        {
            teamOne = eBlueTeam;
            teamOneColor = "blue";
            continue;
        }
        if (bz_getTeamPlayerLimit(ePurpleTeam) > 0 && teamOne == eNoTeam)
        {
            teamOne = ePurpleTeam;
            teamOneColor = "purple";
            continue;
        }
        
        //Figure out the other team
        if (bz_getTeamPlayerLimit(eRedTeam) > 0 && teamOne != eRedTeam && teamTwo == eNoTeam)
        {
            teamTwo = eRedTeam;
            teamTwoColor = "red";
            continue;
        }
        if (bz_getTeamPlayerLimit(eGreenTeam) > 0 && teamOne != eGreenTeam && teamTwo == eNoTeam)
        {
            teamTwo = eGreenTeam;
            teamTwoColor = "green";
            continue;
        }
        if (bz_getTeamPlayerLimit(eBlueTeam) > 0 && teamOne != eBlueTeam && teamTwo == eNoTeam)
        {
            teamTwo = eBlueTeam;
            teamTwoColor = "blue";
            continue;
        }
        if (bz_getTeamPlayerLimit(ePurpleTeam) > 0 && teamOne != ePurpleTeam && teamTwo == eNoTeam)
        {
            teamTwo = ePurpleTeam;
            teamTwoColor = "purple";
            continue;
        }
    }
    
    int count = 0;
    std::string params = commandLine;
    std::string myParameters[2] = {"0"};
    const char* myParam;
    char* parameter = new char[params.size() + 1];
    std::copy(params.begin(), params.end(), parameter);
    parameter[params.size()] = '\0';

    myParam = strtok (parameter,",");
    
    while (myParam != NULL)
    {
        myParameters[count] = myParam;
        count++;
        myParam = strtok (NULL, ",");
    }
    
    if (strcmp(myParameters[0].c_str(), "1") == 0)
    {
        alwaysBalanceTeams = true;
        bz_setBZDBBool("_alwaysBalanceTeams", true, 0, false);
        bz_debugMessage(1," -> alwaysBalanceTeams enabled");
    }
    else
    {
        alwaysBalanceTeams = false;
        bz_setBZDBBool("_alwaysBalanceTeams", false, 0, false);
        bz_debugMessage(1," -> alwaysBalanceTeams disabled");
    }
    if (strcmp(myParameters[1].c_str(), "1") == 0)
    {
        
        balanceTeamsOnCap = true;
        bz_setBZDBBool("_balanceTeamsOnCap", true, 0, false);
        bz_debugMessage(1," -> balanceTeamsOnCap enabled");
    }
    else
    {
        alwaysBalanceTeams = false;
        bz_setBZDBBool("_balanceTeamsOnCap", false, 0, false);
        bz_debugMessage(1," -> balanceTeamsOnCap disabled");
    }
    
    teamsUneven = false;
}

void teamSwitch::Cleanup()
{
    Flush();
    bz_removeCustomSlashCommand("switch");
    bz_removeCustomSlashCommand("balance");

    bz_debugMessage(4,"teamSwitch plugin unloaded");
}

void teamSwitch::balanceTeams(void)
{
    if (bz_getTeamCount(teamOne) == 0 || bz_getTeamCount(teamTwo) == 0) return;
    
    //Break if it's 2-1, 2-3, or 3-4
    if (((bz_getTeamCount(teamOne) == 1 || bz_getTeamCount(teamTwo) == 1) && (bz_getTeamCount(teamOne) == 2 || bz_getTeamCount(teamTwo) == 2)) ||
        ((bz_getTeamCount(teamOne) == 2 || bz_getTeamCount(teamTwo) == 2) && (bz_getTeamCount(teamOne) == 3 || bz_getTeamCount(teamTwo) == 3)) ||
        ((bz_getTeamCount(teamOne) == 3 || bz_getTeamCount(teamTwo) == 3) && (bz_getTeamCount(teamOne) == 4 || bz_getTeamCount(teamTwo) == 4)))
        return;
    
    bz_eTeamType strongTeam;
    if (bz_getTeamCount(teamOne) > bz_getTeamCount(teamTwo)) strongTeam = teamOne;
    else strongTeam = teamTwo;
    
    int teamDifference = abs(bz_getTeamCount(teamOne) - bz_getTeamCount(teamTwo));
    if (teamDifference/2 != 0) teamDifference - 1;
    
    int playersToMove = teamDifference/2;
    int playerMoved = 0;
    for (int i = 0; i < playersToMove; i++)
    {
        playerMoved = bz_randomPlayer(strongTeam);
        
        if (strongTeam == teamTwo)
        {
            switchPlayer(playerMoved, teamOneColor);
            bz_sendTextMessage(playerMoved, playerMoved, "-_-__-___-___++ ###################################################################### ++____-___-__-_-");
            bz_sendTextMessagef(playerMoved, playerMoved, "-_-__-___-___++ {{{ YOU SWITCHED TO THE %s TEAM | SEE '/HELP SWITCH' FOR MORE INFO }}} ++____-___-__-_-", bz_toupper(teamOneColor.c_str()));
            bz_sendTextMessage(playerMoved, playerMoved, "-_-__-___-___++ ###################################################################### ++____-___-__-_-");
        }
        else if (strongTeam == teamOne)
        {
            switchPlayer(playerMoved, teamTwoColor);
            bz_sendTextMessage(playerMoved, playerMoved, "-_-__-___-___++ ##################################################################### ++____-___-__-_-");
            bz_sendTextMessagef(playerMoved, playerMoved, "-_-__-___-___++ {{{ YOU SWITCHED TO THE %s TEAM | SEE '/HELP SWITCH' FOR MORE INFO }}} ++____-___-__-_-", bz_toupper(teamTwoColor.c_str()));
            bz_sendTextMessage(playerMoved, playerMoved, "-_-__-___-___++ ###################################################################### ++____-___-__-_-");
        }
        
        bz_BasePlayerRecord *pr = bz_getPlayerByIndex(playerMoved);
        pr->spawned = true;
        bz_freePlayerRecord(pr);
    }
    
    teamsUneven = false;
}

void teamSwitch::Event(bz_EventData* eventData)
{
    switch (eventData->eventType)
    {
        case bz_eBZDBChange:
        {
            bz_BZDBChangeData_V1* bzdbChange = (bz_BZDBChangeData_V1*)eventData;
            
            if (bzdbChange->key == "_alwaysBalanceTeams")
            {
                if (bzdbChange->value == "1" || bzdbChange->value == "true")
                {
                    alwaysBalanceTeams = true;
                }
                else
                {
                    alwaysBalanceTeams = false;
                }
                
            }
            if (bzdbChange->key == "_balanceTeamsOnCap")
            {
                if (bzdbChange->value == "1" || bzdbChange->value == "true")
                {
                    balanceTeamsOnCap = true;
                }
                else
                {
                    balanceTeamsOnCap = false;
                }
            }
        }
        break;
        
        case bz_eCaptureEvent: // A flag is captured
        {
            bz_CTFCaptureEventData_V1* ctfdata = (bz_CTFCaptureEventData_V1*)eventData;
            
            if (balanceTeamsOnCap)
            {
                if (bz_getTeamCount(teamOne) == 0 || bz_getTeamCount(teamTwo) == 0) break;
            
                if (bz_getTeamCount(teamTwo)/double(bz_getTeamCount(teamOne)) > 1.3 || bz_getTeamCount(teamOne)/double(bz_getTeamCount(teamTwo)) > 1.3)
                {
                    if (bz_getTeamCount(teamOne) > bz_getTeamCount(teamTwo)) strongTeam = teamOne;
                    else strongTeam = teamTwo;
                
                    if (strongTeam == teamOne)
                    {
                        bz_BasePlayerRecord *pr = bz_getPlayerByIndex(ctfdata->playerCapping);
                        
                        if (pr->team == strongTeam)
                        {
                            switchPlayer(ctfdata->playerCapping, teamTwoColor);
                            bz_killPlayer(ctfdata->playerCapping, 0);
                            bz_sendTextMessage(ctfdata->playerCapping, ctfdata->playerCapping, "-_-__-___-___++ ########################################################################## ++____-___-__-_-");
                            bz_sendTextMessagef(ctfdata->playerCapping, ctfdata->playerCapping, "-_-__-___-___++ {{{ YOU GOT SWITCHED TO THE %s TEAM | SEE '/HELP SWITCH' FOR MORE INFO }}} ++____-___-__-_-", bz_toupper(teamTwoColor.c_str()));
                            bz_sendTextMessage(ctfdata->playerCapping, ctfdata->playerCapping, "-_-__-___-___++ ########################################################################## ++____-___-__-_-");
                        }
                        
                        bz_freePlayerRecord(pr);
                    }
                    else
                    {
                        bz_BasePlayerRecord *pr = bz_getPlayerByIndex(ctfdata->playerCapping);
                        
                        if (pr->team == strongTeam)
                        {
                            switchPlayer(ctfdata->playerCapping, teamOneColor);
                            bz_killPlayer(ctfdata->playerCapping, 0);
                            bz_sendTextMessage(ctfdata->playerCapping, ctfdata->playerCapping, "-_-__-___-___++ ########################################################################## ++____-___-__-_-");
                            bz_sendTextMessagef(ctfdata->playerCapping, ctfdata->playerCapping, "-_-__-___-___++ {{{ YOU GOT SWITCHED TO THE %s TEAM | SEE '/HELP SWITCH' FOR MORE INFO }}} ++____-___-__-_-", bz_toupper(teamOneColor.c_str()));
                            bz_sendTextMessage(ctfdata->playerCapping, ctfdata->playerCapping, "-_-__-___-___++ ########################################################################## ++____-___-__-_-");
                        }

                        bz_freePlayerRecord(pr);
                    }
                }
                
                bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "Balancing unfair teams...");
                balanceTeams();
            }
        }
        break;

        case bz_eTickEvent: // The server's main loop has iterated
        {
            bz_TickEventData_V1* tickdata = (bz_TickEventData_V1*)eventData;
            
            if (alwaysBalanceTeams)
            {
                double redToBlueRatio = bz_getTeamCount(teamOne)/double(bz_getTeamCount(teamTwo));
                double blueToRedRatio = bz_getTeamCount(teamTwo)/double(bz_getTeamCount(teamOne));
            
                if ((redToBlueRatio > 1.3 || blueToRedRatio > 1.3) && !teamsUneven)
                {
                    if (redToBlueRatio > 1.3) strongTeam = teamOne;
                    else strongTeam = teamTwo;

                    teamsUneven = true;
                    timeFirstUneven = tickdata->eventTime;
                }
                else if (teamsUneven)
                {
                    if (strongTeam == teamOne)
                    {
                        if (redToBlueRatio < 1.3)
                        {
                            teamsUneven = false;
                        }
                    }
                    else
                    {
                        if (blueToRedRatio < 1.3)
                        {
                            teamsUneven = false;
                        }
                    }
                }
            
                if (teamsUneven && (timeFirstUneven + 10 < bz_getCurrentTime()))
                {
                    bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "Balancing unfair teams...");
                    balanceTeams();
                }
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
                if (strcmp(teamToSwitchTo.c_str(), "red") == 0) maxPlayersOnTeam = bz_getTeamPlayerLimit(teamOne);
                if (strcmp(teamToSwitchTo.c_str(), "blue") == 0) maxPlayersOnTeam = bz_getTeamPlayerLimit(teamTwo);
                if (strcmp(teamToSwitchTo.c_str(), "green") == 0) maxPlayersOnTeam = bz_getTeamPlayerLimit(eGreenTeam);
                if (strcmp(teamToSwitchTo.c_str(), "purple") == 0) maxPlayersOnTeam = bz_getTeamPlayerLimit(ePurpleTeam);
                if (strcmp(teamToSwitchTo.c_str(), "observer") == 0) maxPlayersOnTeam = bz_getTeamPlayerLimit(eObservers);
            
                if (maxPlayersOnTeam > 0)
                {
                    if (playerID != myID)
                    {
                        switchPlayer(myID, teamToSwitchTo);
                        bz_sendTextMessage(myID, myID, "-_-__-___-___++ ################################################################################# ++____-___-__-_-");
                        bz_sendTextMessagef(myID, myID, "-_-__-___-___++ {{{ YOU WERE SWITCHED TO THE %s TEAM BY %s | SEE '/HELP SWITCH' FOR MORE INFO }}} ++____-___-__-_-", bz_toupper(teamToSwitchTo.c_str()), bz_toupper(bz_getPlayerByIndex(playerID)->callsign.c_str()));
                        bz_sendTextMessage(myID, myID, "-_-__-___-___++ ################################################################################# ++____-___-__-_-");
                        
                        bz_BasePlayerRecord *pr = bz_getPlayerByIndex(myID);
                        pr->spawned = true;
                        bz_freePlayerRecord(pr);
                    }
                    else
                    {
                        switchPlayer(playerID, teamToSwitchTo);
                        bz_sendTextMessage(myID, myID, "-_-__-___-___++ ###################################################################### ++____-___-__-_-");
                        bz_sendTextMessagef(myID, myID, "-_-__-___-___++ {{{ YOU SWITCHED TO THE %s TEAM | SEE '/HELP SWITCH' FOR MORE INFO }}} ++____-___-__-_-", bz_toupper(teamToSwitchTo.c_str()));
                        bz_sendTextMessage(myID, myID, "-_-__-___-___++ ###################################################################### ++____-___-__-_-");
                        
                        bz_BasePlayerRecord *pr = bz_getPlayerByIndex(playerID);
                        pr->spawned = true;
                        bz_freePlayerRecord(pr);
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
    else if (command == "balance" && bz_hasPerm(playerID, "switch"))
    {
        balanceTeams();
        bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "Balancing unfair teams...");
        bz_sendTextMessage(BZ_SERVER, playerID, "Teams have been balanced");
    }
    else
    {
        bz_sendTextMessagef(BZ_SERVER, playerID, "Unknown command [%s]", command.c_str());
    }
}

