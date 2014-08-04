/*
League Overseer
    Copyright (C) 2013-2014 Vladimir Jimenez

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.    See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.    If not, see <http://www.gnu.org/licenses/>.
 */

#include <cmath>
#include <cstdlib>
#include <memory>
#include <string>

#include "bzfsAPI.h"
#include "bzToolkit/bzToolkitAPI.h"

// Define plugin name
const std::string PLUGIN_NAME = "Automatic Team Balance";

// Define plugin version numbering
const int MAJOR = 1;
const int MINOR = 6;
const int REV = 0;
const int BUILD = 62;

class teamSwitch : public bz_Plugin, public bz_CustomSlashCommandHandler
{
public:
    virtual const char* Name (void);
    virtual void Init (const char* commandLine);
    virtual void Cleanup (void);
    virtual void Event (bz_EventData *eventData);

    virtual bool SlashCommand (int playerID, bz_ApiString command, bz_ApiString message, bz_APIStringList *params);

    virtual bool balanceTeams (void);
    virtual void queuePlayerSwap (int playerID, bz_eTeamType targetTeam);
    virtual void resetFlag (int flagID, int playerID);
    virtual bool teamsUnfair (bz_eTeamType &strongTeam, bz_eTeamType &weakTeam);
    virtual std::unique_ptr<bz_APIIntList> getStrongestTeamPlayers(bz_eTeamType team, int numberOfPlayers);

    bool swapQueue[256], teamsUneven;
    double timeFirstUneven;
    bz_eTeamType TEAM_ONE, TEAM_TWO, targetTeamQueue[256];
};

BZ_PLUGIN(teamSwitch)

const char* teamSwitch::Name (void)
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

void teamSwitch::Init (const char* /*commandLine*/)
{
    // Register our events with Register()
    Register(bz_eAllowCTFCaptureEvent);
    Register(bz_eCaptureEvent);
    Register(bz_ePlayerDieEvent);
    Register(bz_ePlayerJoinEvent);
    Register(bz_eTickEvent);

    // Register our custom slash commands
    bz_registerCustomSlashCommand("balance", this);
    bz_registerCustomSlashCommand("switch", this);

    // Assign our two team colors to eNoTeam simply so we have something to check for
    // when we are trying to find the two colors the map is using
    TEAM_ONE = eNoTeam;
    TEAM_TWO = eNoTeam;

    // Loop through all the team colors
    for (bz_eTeamType t = eRedTeam; t <= ePurpleTeam; t = (bz_eTeamType) (t + 1))
    {
        // If the current team's player limit is more than 0, that means that we found a
        // team color that the map is using
        if (bz_getTeamPlayerLimit(t) > 0)
        {
            // If team one is eNoTeam, then that means this is just the first team with player limit
            // that we have found. If it's not eNoTeam, that means we've found the second team
            if (TEAM_ONE == eNoTeam)
            {
                TEAM_ONE = t;
            }
            else if (TEAM_TWO == eNoTeam)
            {
                TEAM_TWO = t;
                break; // After we've found the second team, there's no need to continue so break out of here
            }
        }
    }

    bz_debugMessagef(2, "DEBUG :: Automatic Team Balance :: Teams detected -> %s vs %s",
             bztk_eTeamTypeLiteral(TEAM_ONE).c_str(),
             bztk_eTeamTypeLiteral(TEAM_TWO).c_str());

    // Register some custom BZDB variables
    bztk_registerCustomBoolBZDB("_atbAlwaysBalanceTeams", false);
    bztk_registerCustomBoolBZDB("_atbBalanceTeamsOnCap", false);
    bztk_registerCustomIntBZDB("_atbBalanceDelay", 30);
    bztk_registerCustomBoolBZDB("_atbDisableCapWithUnfairTeams", false);
    bztk_registerCustomBoolBZDB("_atbResetFlagToBase", false);

    teamsUneven = false;
}

void teamSwitch::Cleanup ()
{
    Flush(); // Clean up all the events

    // Clean up our custom slash commands
    bz_removeCustomSlashCommand("balance");
    bz_removeCustomSlashCommand("switch");
}

std::unique_ptr<bz_APIIntList> teamSwitch::getStrongestTeamPlayers(bz_eTeamType team, int numberOfPlayers)
{
    std::unique_ptr<bz_APIIntList> playerlist(bztk_getTeamPlayerIndexList(team));
    std::unique_ptr<bz_APIIntList> resp(bz_newIntList());

    std::map<int, int> playerKDRatio; // Map to hold the Kill/Death Ratio of each team player. Std maps automatically sort keys, hence using KD ratio as key

    for (unsigned int i = 0; i < playerlist->size(); i++)
    {
        int playerId = playerlist->get(i);
        int losses   = bz_getPlayerLosses(playerId);
        int wins     = bz_getPlayerWins(playerId);

        int    key   = playerId;
        double ratio = (wins-losses);

        if ((wins + losses) != 0) // Do this to avoid division by zero error
        {
            ratio /= (wins + losses); // This the normalized score with the formula (wins-losses)/(wins+losses) value of -1 to 1
            ratio += 1;               // This makes the negative ratio a postive while keeping the value compared to other players
            key = ((int)(ratio * 100) * 100) + playerId; // *100 to make it an integer. *100 again to make room to add the playerid so it makes players with same ratio unique
        }

        playerKDRatio[key] = playerId;
    }

    int j = 0;

    for (std::map<int, int>::reverse_iterator ii = playerKDRatio.rbegin(); ii != playerKDRatio.rend(); ++ii)
    {
        int second = (*ii).second;
        resp->push_back(second);
        j++;

        if (j == numberOfPlayers)
        {
            break;
        }
    }

    return resp;
}

bool teamSwitch::balanceTeams (void)
{
    // Find which team is the strongest team by getting the amount of players on it
    bz_eTeamType strongTeam;
    bz_eTeamType weakTeam;

    if (bz_getTeamCount(TEAM_ONE) > bz_getTeamCount(TEAM_TWO))
    {
        strongTeam = TEAM_ONE;
        weakTeam = TEAM_TWO;
    }
    else
    {
        strongTeam = TEAM_TWO;
        weakTeam = TEAM_ONE;
    }

    int strongTeamCount = bz_getTeamCount(strongTeam);
    int weakTeamCount   = bz_getTeamCount(weakTeam);

    // Sanity check
    if (strongTeamCount == weakTeamCount)
    {
        bz_debugMessagef(2, "DEBUG :: Automatic Team Balance :: Sanity check failed. Teams are even.");
        return false;
    }

    int teamDifference = strongTeamCount-weakTeamCount;
    int amountOfPlayersToSwitch = teamDifference / 2;

    // Sanity check to make sure we are emptying a team
    if ((strongTeamCount - amountOfPlayersToSwitch) < 1)
    {
        bz_debugMessagef(2, "DEBUG :: Automatic Team Balance :: Sanity check failed. Attempted to empty a team.");
        return false;
    }

    // Sanity check
    std::unique_ptr<bz_APIIntList> playerlist(getStrongestTeamPlayers(strongTeam, amountOfPlayersToSwitch));

    for (unsigned int i = 0; i < playerlist->size(); i++)
    {
        if (bz_getPlayerTeam(playerlist->get(i)) != strongTeam)
        {
            bz_debugMessagef(2, "DEBUG :: Automatic Team Balance :: Sanity check failed. Attempting to switch a player from the weak team.");

            return false;
        }
    }

    // Loop through the players to switch
    for (int i = 0; i < playerlist->size(); i++)
    {
        int playerMoved = playerlist->get(i);
        queuePlayerSwap(playerMoved, weakTeam);
    }

    // This variable is used for the automatic balancing based on a delay to mark the teams as
    // unfair or not
    if (weakTeamCount == 0)
    {
        bz_ApiString teamflag = "";

        if (weakTeam == eRedTeam)    teamflag = "R*";
        if (weakTeam == eBlueTeam)   teamflag = "B*";
        if (weakTeam == eGreenTeam)  teamflag = "G*";
        if (weakTeam == ePurpleTeam) teamflag = "P*";

        if (teamflag!="")
        {
            for (unsigned int i = 0; i < bz_getNumFlags(); i++)
            {
                bz_ApiString flagtype = bz_getFlagName(i);

                if (teamflag == flagtype)
                {
                    bz_resetFlag(i);
                    break;
                }
            }
        }
    }

    teamsUneven = false;
    return true;
}

void teamSwitch::queuePlayerSwap (int playerID, bz_eTeamType targetTeam = eNoTeam)
{
    swapQueue[playerID] = true;
    targetTeamQueue[playerID] = targetTeam;
}

void teamSwitch::resetFlag (int flagID, int playerID)
{
    // The middle of the map
    float pos[3] = {0, 0, 0};

    // Check if the player has a specific flag ID. Typically the flag ID will be 0 or 1 so we
    // can check if the player has a team flag or not and see where to send the flag.
    if (bz_flagPlayer(flagID) == playerID)
    {
        bz_removePlayerFlag(playerID);

        if (bz_getBZDBBool("_atbResetFlagToBase"))
        {
            bz_resetFlag(flagID);
        }
        else
        {
            bz_moveFlag(flagID, pos);
        }
    }
}

bool teamSwitch::teamsUnfair (bz_eTeamType &strongTeam, bz_eTeamType &weakTeam)
{
    // Find which team is the strongest team by getting the amount of players on it
    strongTeam   = (bz_getTeamCount(TEAM_ONE) > bz_getTeamCount(TEAM_TWO)) ? TEAM_ONE : TEAM_TWO;
    weakTeam     = (bz_getTeamCount(TEAM_ONE) > bz_getTeamCount(TEAM_TWO)) ? TEAM_TWO : TEAM_ONE;

    int strongTeamCount = bz_getTeamCount(strongTeam);
    int weakTeamCount   = bz_getTeamCount(weakTeam);

    if ((strongTeamCount - weakTeamCount) < 2)
    {
        return false;
    }

    int alloweddiff = round((strongTeamCount+weakTeamCount) * .1); // Rounded 10% of the total players

    return !((strongTeamCount - weakTeamCount) < alloweddiff);
}

void teamSwitch::Event (bz_EventData* eventData)
{
    switch (eventData->eventType)
    {
        case bz_eAllowCTFCaptureEvent:
        {
            bz_AllowCTFCaptureEventData_V1* allowctfdata = (bz_AllowCTFCaptureEventData_V1*)eventData;
            int playerID = allowctfdata->playerCapping;
            bz_eTeamType strongTeam, weakTeam;

            // We will only disallow the capture if we're told to
            if (bz_getBZDBBool("_atbDisableCapWithUnfairTeams"))
            {
                // Check if the teams are unfair
                if (teamsUnfair(strongTeam, weakTeam))
                {
                    std::unique_ptr<bz_BasePlayerRecord> playerData(bz_getPlayerByIndex(playerID));

                    // If the player capping is part of the strong team so we can switch them
                    if (playerData->team == strongTeam)
                    {
                        allowctfdata->allow = false;

                        // Change the player's team
                        bztk_changeTeam(playerID, weakTeam);

                        // Check which team flag the player is carrying so we can reset it
                        resetFlag(0, playerID);
                        resetFlag(1, playerID);

                        bz_sendTextMessagef(BZ_SERVER, playerID, "You were switched to the %s.", bz_tolower(bztk_eTeamTypeLiteral(weakTeam).c_str()));
                    }
                }
            }
        }
        break;

        case bz_eCaptureEvent:
        {
            bz_CTFCaptureEventData_V1* ctfdata = (bz_CTFCaptureEventData_V1*)eventData;
            int playerID = ctfdata->playerCapping;
            bz_eTeamType strongTeam, weakTeam;

            // If the player is allowed to capture the flag, check to see if we need to balance the teams
            if (bz_getBZDBBool("_atbBalanceTeamsOnCap"))
            {
                // Check if the teams are unfair
                if (teamsUnfair(strongTeam, weakTeam))
                {
                    std::unique_ptr<bz_BasePlayerRecord> playerData(bz_getPlayerByIndex(playerID));

                    // If the player who capped is part of the stronger team
                    if (playerData->team == strongTeam)
                    {
                        // Switch the player
                        bztk_changeTeam(playerID, weakTeam);

                        // Kill the player as punishment and swap them to the weak team
                        bz_killPlayer(playerID, false);

                        bz_sendTextMessagef(BZ_SERVER, playerID, "You were automatically switched to the %s team to make the teams fair.", bztk_eTeamTypeLiteral(weakTeam).c_str());
                    }
                }
            }
        }
        break;

        case bz_ePlayerDieEvent: // This event is called each time a tank is killed.
        {
            bz_PlayerDieEventData_V1* dieData = (bz_PlayerDieEventData_V1*)eventData;

            int playerID = dieData->playerID;

            if (swapQueue[playerID])
            {
                if (targetTeamQueue[playerID] != eNoTeam)
                {
                    bztk_changeTeam(playerID, targetTeamQueue[playerID]);
                    bz_sendTextMessagef(BZ_SERVER, playerID, "You were automatically switched to the %s team to make the teams fair.", bztk_eTeamTypeLiteral(targetTeamQueue[playerID]).c_str());
                }

                swapQueue[playerID] = false;
                targetTeamQueue[playerID] = eNoTeam;
            }
        }
        break;

        case bz_ePlayerJoinEvent: // This event is called each time a player joins the game
        {
            bz_PlayerJoinPartEventData_V1* joinData = (bz_PlayerJoinPartEventData_V1*)eventData;

            int playerID = joinData->playerID;

            swapQueue[playerID] = false;
            targetTeamQueue[playerID] = eNoTeam;
        }
        break;

        case bz_eTickEvent:
        {
            bz_eTeamType strongTeam, weakTeam;

            // Check to see whether we were asked to always balance the teams
            if (bz_getBZDBBool("_atbAlwaysBalanceTeams"))
            {
                // Store this value for easy access
                bool _teamsUnfair = teamsUnfair(strongTeam, weakTeam);

                if (_teamsUnfair && !teamsUneven)
                {
                    teamsUneven = true;
                    timeFirstUneven = bz_getCurrentTime();
                }
                else if (teamsUneven && !_teamsUnfair)
                {
                    teamsUneven = false;
                }

                if (teamsUneven && (timeFirstUneven + bz_getBZDBInt("_atbBalanceDelay") < bz_getCurrentTime()))
                {
                    if (balanceTeams())
                    {
                        bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "Teams will be balanced automatically");
                    }
                }
            }
        }
        break;

        default:
            break;
    }
}

bool teamSwitch::SlashCommand(int playerID, bz_ApiString command, bz_ApiString /*message*/, bz_APIStringList *params)
{
    if (command == "switch" && bz_hasPerm(playerID, "switch"))
    {
        if (params->size() != 1 && params->size() != 2)
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "Syntax: /switch <player slot or callsign> rogue|red|green|blue|purple|observer");
            return true;
        }

        int victimID;
        bz_eTeamType targetTeam;

        if (params->size() == 2)
        {
            if (std::string(params->get(0).c_str()).find("#") != std::string::npos)
            {
                victimID = atoi(std::string(params->get(0).c_str()).erase(0,1).c_str());
            }
            else
            {
                std::unique_ptr<bz_BasePlayerRecord> playerData(bztk_getPlayerByCallsign(params->get(0).c_str()));
                victimID = (playerData != NULL) ? playerData->playerID : -1;
            }

            targetTeam = bztk_eTeamType(params->get(1).c_str());
        }
        else
        {
            victimID = playerID;
            targetTeam = bztk_eTeamType(params->get(0).c_str());
        }

        if (bztk_isValidPlayerID(victimID))
        {
            bz_PlayerRecordV2* pr = (bz_PlayerRecordV2*)bz_getPlayerByIndex(victimID);

            if (pr->team == eObservers && pr->motto == "bzadmin")
            {
                bz_sendTextMessage(BZ_SERVER, playerID, "Warning: In order to prevent bzadmin clients from crashing, you cannot 'switch' bzadmin clients.");
                bz_freePlayerRecord(pr);
                return true;
            }

            bz_freePlayerRecord(pr);

            if (bztk_changeTeam(victimID, targetTeam))
            {
                if (playerID != victimID)
                {
                    bz_sendTextMessagef(BZ_SERVER, victimID, "You were switched to the %s team by %s.",
                                    bztk_eTeamTypeLiteral(targetTeam).c_str(),
                                    bz_getPlayerByIndex(playerID)->callsign.c_str());
                }
                else
                {
                    bz_sendTextMessagef(BZ_SERVER, victimID, "You switched to the %s team.", bztk_eTeamTypeLiteral(targetTeam).c_str());
                }
            }
            else
            {
                bz_sendTextMessagef(BZ_SERVER, playerID, "The %s team does not exist on this server.", bztk_eTeamTypeLiteral(targetTeam).c_str());
            }
        }
        else
        {
            bz_sendTextMessagef(BZ_SERVER, playerID, "player '%s' not found", params->get(0).c_str());
        }

        return true;
    }
    else if (command == "balance" && bz_hasPerm(playerID, "switch"))
    {
        bz_eTeamType strongTeam, weakTeam;

        if (!teamsUnfair(strongTeam, weakTeam))
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "Teams are detected to be even.");
            return true;
        }

        if (balanceTeams())
        {
            bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "Teams will be balanced automatically");
            bz_sendTextMessage(BZ_SERVER, playerID, "Players will be swapped teams as they die in order to balance teams");
        }

        return true;
    }
    else
    {
        bz_sendTextMessagef(BZ_SERVER, playerID, "Unknown command [%s]", command.c_str());
        return true;
    }
}