/*
Copyright (c) 2012-2013 Vladimir Jimenez
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
/switch <color>
/balance

License:
BSD

Version:
1.4.2

Thanks to:
Murielle
mdskpr
sigonasr2
*/

#include "bzfsAPI.h"
#include <cmath>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <time.h>
#include "../../src/bzfs/GameKeeper.h"
#include "../../src/bzfs/bzfs.h"
#include "../../src/bzfs/CmdLineOptions.h"

void fixTeamCount()
{
    int playerIndex, teamNum;
    for (teamNum = RogueTeam; teamNum < HunterTeam; teamNum++)
        team[teamNum].team.size = 0;

    for (playerIndex = 0; playerIndex < curMaxPlayers; playerIndex++)
    {
        GameKeeper::Player *p = GameKeeper::Player::getPlayerByIndex(playerIndex);

        if (p && p->player.isPlaying())
        {
            teamNum = p->player.getTeam();

            if (teamNum == HunterTeam)
                teamNum = RogueTeam;

            team[teamNum].team.size++;
        }
    }
}

void removePlayer(int playerIndex)
{
    GameKeeper::Player *playerData = GameKeeper::Player::getPlayerByIndex(playerIndex);

    if (!playerData)
        return;

    void *buf, *bufStart = getDirectMessageBuffer();
    buf = nboPackUByte(bufStart, playerIndex);

    broadcastMessage(MsgRemovePlayer, (char*)buf-(char*)bufStart, bufStart);

    int teamNum = int(playerData->player.getTeam());
    --team[teamNum].team.size;
    sendTeamUpdate(-1, teamNum);
    fixTeamCount();
}

void addPlayer(GameKeeper::Player *playerData, int index)
{
    void *bufStart = getDirectMessageBuffer();
    void *buf      = playerData->packPlayerUpdate(bufStart);

    broadcastMessage(MsgAddPlayer, (char*)buf - (char*)bufStart, bufStart);

    int teamNum = int(playerData->player.getTeam());
    team[teamNum].team.size++;
    sendTeamUpdate(-1, teamNum);
    fixTeamCount();
}

bool switchPlayer(int index, std::string teamColor)
{
    GameKeeper::Player *playerData = GameKeeper::Player::getPlayerByIndex(index);

    if (!playerData)
        return true;

    removePlayer(index);

    if (strcmp(teamColor.c_str(), "rogue") == 0) playerData->player.setTeam((TeamColor)RogueTeam);
    if (strcmp(teamColor.c_str(), "red") == 0) playerData->player.setTeam((TeamColor)RedTeam);
    if (strcmp(teamColor.c_str(), "green") == 0) playerData->player.setTeam((TeamColor)GreenTeam);
    if (strcmp(teamColor.c_str(), "blue") == 0) playerData->player.setTeam((TeamColor)BlueTeam);
    if (strcmp(teamColor.c_str(), "purple") == 0) playerData->player.setTeam((TeamColor)PurpleTeam);
    if (strcmp(teamColor.c_str(), "observer") == 0) playerData->player.setTeam((TeamColor)ObserverTeam);

    addPlayer(playerData,index);
    sendPlayerInfo();
    sendIPUpdate(-1, index);
}

bool bz_anyPlayers(bool observers = false)
{
    return (bool)(bz_getTeamCount(eRogueTeam) +
                  bz_getTeamCount(eRedTeam) +
                  bz_getTeamCount(eGreenTeam) +
                  bz_getTeamCount(eBlueTeam) +
                  bz_getTeamCount(ePurpleTeam) +
                  bz_getTeamCount(eRabbitTeam) +
                  bz_getTeamCount(eHunterTeam) +
                  (observers?bz_getTeamCount(eObservers):0));
}

int bz_randomPlayer(bz_eTeamType team = eNoTeam)
{
    srand(time(NULL));

    if (team == eNoTeam)
    {
        if (bz_anyPlayers())
        {
            bz_debugMessage(4,"There are players.");
            bz_APIIntList*playerlist=bz_getPlayerIndexList();
            int picked;
            picked=(*playerlist)[rand()%playerlist->size()];
            bz_deleteIntList(playerlist);

            return picked;
        }
        else
            return -1;
    }
    else
    {
        if (bz_getTeamCount(team) > 0)
        {
            int picked=0;
            bz_APIIntList* playerlist = bz_getPlayerIndexList();

            while (true)
            {
                picked = rand() % playerlist->size();
                if (bz_getPlayerTeam(picked) == team)
                    break;
            }

            bz_deleteIntList(playerlist);
            return picked;
        }
        else
            return -1;
    }
}

bool isDigit(std::string myString) //Check to see if a string is a digit
{
    for(int i = 0; i < myString.size(); i++) //Go through entire string
    {
        if(!isdigit(myString[i])) //If one character is not a digit, then the string is not a digit
            return false;
    }
    return true; //All characters are digits
}

bool isValidPlayerID(int myID) //Check to see if a player ID exists on the server
{
    bz_APIIntList *playerList = bz_newIntList(); //Get the list of all the current player IDs
    bz_getPlayerIndexList(playerList);

    for (unsigned int i = 0; i < playerList->size(); i++ )
    {
        if (playerList->get(i) == myID)
        {
            bz_deleteIntList(playerList); //Free the memory allocated
            return true; //The player ID inputted, matches an ID that is on the map
        }
    }

    bz_deleteIntList(playerList); //Free the memory allocated

    return false; //Player ID was not found on the server
}

class teamSwitch : public bz_Plugin, public bz_CustomSlashCommandHandler
{
public:
    virtual const char* Name (){return "Automatic Team Balance";}
    virtual void Init(const char* commandLine);
    virtual void Cleanup(void);

    virtual void Event(bz_EventData *eventData);
    virtual bool SlashCommand(int playerID, bz_ApiString command, bz_ApiString message, bz_APIStringList *params);

    virtual void balanceTeams(void);
    virtual bool playerCountLow(void);

    bool alwaysBalanceTeams;
    bool balanceTeamsOnCap;
    bool disableCapWithUnfairTeams;
    int checkTeamBalanceTime;

    bool teamsUneven;
    double timeFirstUneven;
    bz_eTeamType strongTeam, weakTeam, teamOne, teamTwo;
    std::string teamOneColor, teamTwoColor;
};

BZ_PLUGIN(teamSwitch);

void teamSwitch::Init(const char* commandLine)
{
    bz_debugMessage(1, "Automatic Team Balance plugin loaded");

    Register(bz_eAllowCTFCaptureEvent);
    Register(bz_eBZDBChange);
    Register(bz_eCaptureEvent);
    Register(bz_eTickEvent);

    bz_registerCustomSlashCommand("switch", this);
    bz_registerCustomSlashCommand("balance", this);

    checkTeamBalanceTime = 30;
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

    bz_debugMessagef(1, "[DEBUG] Automatic Team Balance :: Teams detected -> %s vs %s", bz_toupper(teamTwoColor.c_str()), bz_toupper(teamOneColor.c_str()));

    int count = 0;
    std::string params = commandLine;
    std::string myParameters[4] = {"0"};
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

    if (atoi(myParameters[0].c_str()) > 0)
    {
        checkTeamBalanceTime = atoi(myParameters[0].c_str());
        bz_setBZDBInt("_atbBalanceDelay", checkTeamBalanceTime, 0, false);
        bz_debugMessagef(1, "[DEBUG] Automatic Team Balance :: Balance delay set to %i seconds.", checkTeamBalanceTime);
    }
    else
    {
        checkTeamBalanceTime = 30;
        bz_setBZDBInt("_atbBalanceDelay", checkTeamBalanceTime, 0, false);
        bz_debugMessage(1, "[DEBUG] Automatic Team Balance :: Balance delay automatically set to 30 seconds.");
    }

    if (strcmp(myParameters[1].c_str(), "1") == 0)
    {
        alwaysBalanceTeams = true;
        bz_setBZDBBool("_atbAlwaysBalanceTeams", true, 0, false);
        bz_debugMessagef(1, "[DEBUG] Automatic Team Balance :: Teams will be balanced automatically after %i seconds of being unfair.", checkTeamBalanceTime);
    }
    else
    {
        alwaysBalanceTeams = false;
        bz_setBZDBBool("_atbAlwaysBalanceTeams", false, 0, false);
    }

    if (strcmp(myParameters[2].c_str(), "1") == 0)
    {
        balanceTeamsOnCap = true;
        bz_setBZDBBool("_atbBalanceTeamsOnCap", true, 0, false);
        bz_debugMessage(1, "[DEBUG] Automatic Team Balance :: Teams will be automatically balanced after an unfair capture.");
    }
    else
    {
        balanceTeamsOnCap = false;
        bz_setBZDBBool("_atbBalanceTeamsOnCap", false, 0, false);
    }

    if (strcmp(myParameters[3].c_str(), "1") == 0)
    {
        disableCapWithUnfairTeams = true;
        bz_setBZDBBool("_atbDisableCapWithUnfairTeams", true, 0, false);
        bz_debugMessage(1, "[DEBUG] Automatic Team Balance :: Unfair team captures have been disabled and will cause an automatic balance.");

        if (strcmp(myParameters[2].c_str(), "1") == 0)
        {
            balanceTeamsOnCap = false;
            bz_setBZDBBool("_atbBalanceTeamsOnCap", false, 0, false);
            bz_debugMessage(1, "[DEBUG] Automatic Team Balance :: _atbBalanceTeamsOnCap has been turned off.");
        }
    }
    else
    {
        disableCapWithUnfairTeams = false;
        bz_setBZDBBool("_atbDisableCapWithUnfairTeams", false, 0, false);
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
    if (playerCountLow())
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

bool teamSwitch::playerCountLow(void)
{
    if (bz_getTeamCount(teamOne) == 0 || bz_getTeamCount(teamTwo) == 0) return true;

    //Break if it's 2-1, 2-3, or 3-4
    if (((bz_getTeamCount(teamOne) == 1 || bz_getTeamCount(teamTwo) == 1) && (bz_getTeamCount(teamOne) == 2 || bz_getTeamCount(teamTwo) == 2)) ||
        ((bz_getTeamCount(teamOne) == 2 || bz_getTeamCount(teamTwo) == 2) && (bz_getTeamCount(teamOne) == 3 || bz_getTeamCount(teamTwo) == 3)) ||
        ((bz_getTeamCount(teamOne) == 3 || bz_getTeamCount(teamTwo) == 3) && (bz_getTeamCount(teamOne) == 4 || bz_getTeamCount(teamTwo) == 4)))
        return true;

    return false;
}

void teamSwitch::Event(bz_EventData* eventData)
{
    switch (eventData->eventType)
    {
        case bz_eAllowCTFCaptureEvent:
        {
            bz_AllowCTFCaptureEventData_V1* allowctfdata = (bz_AllowCTFCaptureEventData_V1*)eventData;

            if (disableCapWithUnfairTeams)
            {
                if (playerCountLow())
                    return;

                if (bz_getTeamCount(teamOne) > bz_getTeamCount(teamTwo)) {strongTeam = teamOne; weakTeam = teamTwo;}
                else {strongTeam = teamTwo; weakTeam = teamOne;}
                int bonusPoints = 8 * (bz_getTeamCount(weakTeam) - bz_getTeamCount(strongTeam)) + 3 * bz_getTeamCount(weakTeam);

                if (bonusPoints < 0)
                {
                    if (strongTeam == teamOne)
                    {
                        bz_BasePlayerRecord *pr = bz_getPlayerByIndex(allowctfdata->playerCapping);

                        if (pr->team == strongTeam)
                        {
                            allowctfdata->allow = false;
                            switchPlayer(allowctfdata->playerCapping, teamTwoColor);
                            float pos[3] = {0, 0, 0};

                            if (bz_flagPlayer(0) == allowctfdata->playerCapping)
                            {
                                bz_removePlayerFlag(pr->playerID);
                                bz_moveFlag(0, pos);
                            }
                            else if (bz_flagPlayer(1) == allowctfdata->playerCapping)
                            {
                                bz_removePlayerFlag(pr->playerID);
                                bz_moveFlag(1, pos);
                            }

                            bz_sendTextMessage(allowctfdata->playerCapping, allowctfdata->playerCapping, "-_-__-___-___++ ########################################################################## ++____-___-__-_-");
                            bz_sendTextMessagef(allowctfdata->playerCapping, allowctfdata->playerCapping, "-_-__-___-___++ {{{ YOU GOT SWITCHED TO THE %s TEAM | SEE '/HELP SWITCH' FOR MORE INFO }}} ++____-___-__-_-", bz_toupper(teamTwoColor.c_str()));
                            bz_sendTextMessage(allowctfdata->playerCapping, allowctfdata->playerCapping, "-_-__-___-___++ ########################################################################## ++____-___-__-_-");
                        }

                        bz_freePlayerRecord(pr);
                    }
                    else
                    {
                        bz_BasePlayerRecord *pr = bz_getPlayerByIndex(allowctfdata->playerCapping);

                        if (pr->team == strongTeam)
                        {
                            allowctfdata->allow = false;
                            switchPlayer(allowctfdata->playerCapping, teamOneColor);
                            float pos[3] = {0, 0, 0};

                            if (bz_flagPlayer(0) == allowctfdata->playerCapping)
                            {
                                bz_removePlayerFlag(pr->playerID);
                                bz_moveFlag(0, pos);
                            }
                            else if (bz_flagPlayer(1) == allowctfdata->playerCapping)
                            {
                                bz_removePlayerFlag(pr->playerID);
                                bz_moveFlag(1, pos);
                            }

                            bz_sendTextMessage(allowctfdata->playerCapping, allowctfdata->playerCapping, "-_-__-___-___++ ########################################################################## ++____-___-__-_-");
                            bz_sendTextMessagef(allowctfdata->playerCapping, allowctfdata->playerCapping, "-_-__-___-___++ {{{ YOU GOT SWITCHED TO THE %s TEAM | SEE '/HELP SWITCH' FOR MORE INFO }}} ++____-___-__-_-", bz_toupper(teamOneColor.c_str()));
                            bz_sendTextMessage(allowctfdata->playerCapping, allowctfdata->playerCapping, "-_-__-___-___++ ########################################################################## ++____-___-__-_-");
                        }

                        bz_freePlayerRecord(pr);
                    }

                    bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "Balancing unfair teams...");
                    balanceTeams();
                }
            }
        }
        break;

        case bz_eBZDBChange:
        {
            bz_BZDBChangeData_V1* bzdbChange = (bz_BZDBChangeData_V1*)eventData;

            if (bzdbChange->key == "_atbAlwaysBalanceTeams")
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

            if (bzdbChange->key == "_atbBalanceTeamsOnCap")
            {
                if (bzdbChange->value == "1" || bzdbChange->value == "true")
                {
                    balanceTeamsOnCap = true;
                    disableCapWithUnfairTeams = false;
                }
                else
                {
                    balanceTeamsOnCap = false;
                }
            }

            if (bzdbChange->key == "_atbDisableCapWithUnfairTeams")
            {
                if (bzdbChange->value == "1" || bzdbChange->value == "true")
                {
                    disableCapWithUnfairTeams = true;
                    balanceTeamsOnCap = false;
                }
                else
                {
                    disableCapWithUnfairTeams = false;
                }
            }

            if (bzdbChange->key == "_atbBalanceDelay")
            {
                if (atoi(bzdbChange->value.c_str()) > 0)
                {
                    checkTeamBalanceTime = atoi(bzdbChange->value.c_str());
                }
                else
                {
                    checkTeamBalanceTime = 30;
                }
            }
        }
        break;

        case bz_eCaptureEvent: // A flag is captured
        {
            bz_CTFCaptureEventData_V1* ctfdata = (bz_CTFCaptureEventData_V1*)eventData;

            if (balanceTeamsOnCap)
            {
                if (playerCountLow())
                    return;

                if (bz_getTeamCount(teamOne) > bz_getTeamCount(teamTwo)) {strongTeam = teamOne; weakTeam = teamTwo;}
                else {strongTeam = teamTwo; weakTeam = teamOne;}
                int bonusPoints = 8 * (bz_getTeamCount(weakTeam) - bz_getTeamCount(strongTeam)) + 3 * bz_getTeamCount(weakTeam);

                if (bonusPoints < 0)
                {
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

                    bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "Balancing unfair teams...");
                    balanceTeams();
                }
            }
        }
        break;

        case bz_eTickEvent: // The server's main loop has iterated
        {
            bz_TickEventData_V1* tickdata = (bz_TickEventData_V1*)eventData;

            if (alwaysBalanceTeams)
            {
                if (playerCountLow())
                    return;

                if (bz_getTeamCount(teamOne) > bz_getTeamCount(teamTwo)) {strongTeam = teamOne; weakTeam = teamTwo;}
                else {strongTeam = teamTwo; weakTeam = teamOne;}
                int bonusPoints = 8 * (bz_getTeamCount(weakTeam) - bz_getTeamCount(strongTeam)) + 3 * bz_getTeamCount(weakTeam);

                if (bonusPoints < 0 && !teamsUneven)
                {
                    teamsUneven = true;
                    timeFirstUneven = tickdata->eventTime;
                }
                else if (teamsUneven)
                {
                    if (bonusPoints < 0)
                    {
                        teamsUneven = false;
                    }
                }

                if (teamsUneven && (timeFirstUneven + checkTeamBalanceTime < bz_getCurrentTime()))
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
        std::string teamToSwitchTo, tmp;

        if (params->size() == 2)
        {
            if (std::string(params->get(0).c_str()).find("#") != std::string::npos)
                tmp = std::string(params->get(0).c_str()).erase(0,1).c_str();
            else
                tmp = params->get(0).c_str();

            if (isDigit(tmp))
                myID = atoi(tmp.c_str());
            else
            {
                bz_sendTextMessagef(BZ_SERVER, playerID, "Player '%s' not found.", tmp.c_str());
                return true;
            }

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
