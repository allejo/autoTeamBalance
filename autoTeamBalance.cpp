/*
League Overseer
	Copyright (C) 2013-2016 Vladimir Jimenez

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "bzfsAPI.h"
#include "bzToolkit/bzToolkitAPI.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <memory>

// Define plugin name
std::string PLUGIN_NAME = "Automatic Team Balance";

// Define plugin version numbering
int MAJOR = 2;
int MINOR = 0;
int REV = 0;
int BUILD = 70;

class teamSwitch : public bz_Plugin, public bz_CustomSlashCommandHandler
{
public:
	virtual const char* Name (void) { return bztk_pluginName(); }
	virtual void Init (const char* commandLine);
	virtual void Cleanup (void);
	virtual void Event (bz_EventData *eventData);
	virtual bool SlashCommand (int playerID, bz_ApiString command, bz_ApiString message, bz_APIStringList *params);

	virtual bool balanceTeams (void);
	virtual void queuePlayerSwap (int playerID, bz_eTeamType targetTeam);
	virtual bool teamsUnfair (bz_eTeamType *strongTeam, bz_eTeamType *weakTeam);
	virtual bz_APIIntList* getStrongestPlayers (bz_eTeamType team);

	typedef std::map<int, int> MapInt;

	struct Player {
		int          playerID;
		double       lastSwapTime;
		bz_eTeamType targetTeam;

		Player (int _playerID = -1) :
			playerID(_playerID),
			lastSwapTime(-1),
			targetTeam(eNoTeam)
		{}
	};

	std::map<int, Player> players;

	bz_eTeamType TEAM_ONE, TEAM_TWO;

	double timeFirstUneven;
	bool   balanceQueued, // A balance has already been queued
		   teamsUneven;   // The teams are uneven
};

BZ_PLUGIN(teamSwitch)

void teamSwitch::Init (const char* /*commandLine*/)
{
	// Register our events with Register()
	Register(bz_eAllowCTFCaptureEvent);
	Register(bz_eCaptureEvent);
	Register(bz_ePlayerJoinEvent);
	Register(bz_ePlayerSpawnEvent);
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
			 bztk_eTeamTypeLiteral(TEAM_ONE),
			 bztk_eTeamTypeLiteral(TEAM_TWO));

	// Register some custom BZDB variables
	bztk_registerCustomStringBZDB("_atbSwapPlayerAlgorithm", "random");

	bztk_registerCustomBoolBZDB("_atbAlwaysBalanceTeams", false);
	bztk_registerCustomBoolBZDB("_atbBalanceTeamsOnCap", false);
	bztk_registerCustomBoolBZDB("_atbDisableCapWithUnfairTeams", false);

	bztk_registerCustomIntBZDB ("_atbBalanceDelay", 30);

	teamsUneven = false;
}

void teamSwitch::Cleanup ()
{
	Flush(); // Clean up all the events

	// Clean up our custom slash commands
	bz_removeCustomSlashCommand("balance");
	bz_removeCustomSlashCommand("switch");
}

bz_APIIntList* teamSwitch::getStrongestPlayers(bz_eTeamType team)
{
	bz_APIIntList* list = bz_newIntList();

	std::unique_ptr<bz_APIIntList> allPlayers(bz_getPlayerIndexList());
	MapInt scoreboard;

	for (unsigned int i = 0; i < allPlayers->size(); i++)
	{
		int playerId = allPlayers->get(i);

		if (bz_getPlayerTeam(playerId) != team)
		{
			continue;
		}

		int losses = bz_getPlayerLosses(playerId);
		int wins   = bz_getPlayerWins(playerId);

		int    key   = playerId;
		double ratio = (wins - losses);

		if ((wins + losses) != 0) // Do this to avoid division by zero error
		{
			ratio /= (wins + losses); // This the normalized score with the formula (wins-losses)/(wins+losses) value of -1 to 1
			ratio += 1;               // This makes the negative ratio a postive while keeping the value compared to other players
			key = ((int)(ratio * 100) * 100) + playerId; // *100 to make it an integer. *100 again to make room to add the playerid so it makes players with same ratio unique
		}

		scoreboard[key] = playerId;
	}

	for (MapInt::reverse_iterator ii = scoreboard.rbegin(); ii != scoreboard.rend(); ++ii)
	{
		int playerID = (*ii).second;
		list->push_back(playerID);
	}

	return list;
}

bool teamSwitch::balanceTeams (void)
{
	// If a balance has already been queued, then don't try balancing again or else we'll swap more players
	// on accident
	if (balanceQueued)
	{
		return false;
	}

	// We'll queue our balance
	balanceQueued = true;

	bz_eTeamType strongTeam, weakTeam;

	bool teamsAreUnfair  = teamsUnfair(&strongTeam, &weakTeam);
	int  strongTeamCount = bz_getTeamCount(strongTeam),
		 weakTeamCount   = bz_getTeamCount(weakTeam);

	// Teams are already even
	if (teamsAreUnfair)
	{
		bz_debugMessagef(2, "DEBUG :: Automatic Team Balance :: Teams are considered even.");
		return false;
	}

	int teamDifference = strongTeamCount - weakTeamCount;
	unsigned int amountOfPlayersToSwitch = teamDifference / 2;

	bz_debugMessagef(2, "DEBUG :: Automatic Team Balance :: Amount of players to be swapped: %d", amountOfPlayersToSwitch);

	// Sanity check to make sure we are not emptying a team
	if ((strongTeamCount - amountOfPlayersToSwitch) < 1)
	{
		bz_debugMessagef(2, "DEBUG :: Automatic Team Balance :: Attempted to empty a team.");
		return false;
	}

	// Sorting Algorithms
	if (bz_getBZDBString("_atbSwapPlayerAlgorithm") == "strength")
	{
		bz_debugMessagef(3, "DEBUG :: Automatic Team Balance :: Using strength algorithm to balance teams.");

		std::unique_ptr<bz_APIIntList> playerList(getStrongestPlayers(strongTeam));
		int count = std::min(amountOfPlayersToSwitch, playerList->size());

		for (int i = 0; i < count; i++)
		{
			queuePlayerSwap(playerList->get(i), weakTeam);
		}
	}
	else if (bz_getBZDBString("_atbSwapPlayerAlgorithm") == "random")
	{
		bz_debugMessagef(3, "DEBUG :: Automatic Team Balance :: Using random algorithm to balance teams.");

		for (int i = 0; i < amountOfPlayersToSwitch; i++)
		{
			queuePlayerSwap(bztk_randomPlayer(strongTeam), weakTeam);
		}
	}

	// This variable is used for the automatic balancing based on a delay to mark the teams as
	// unfair or not
	teamsUneven = false;

	return true;
}

void teamSwitch::queuePlayerSwap (int playerID, bz_eTeamType targetTeam)
{
	Player &queuedPlayer = players[playerID];
	queuedPlayer.targetTeam = targetTeam;

	bz_debugMessagef(2, "DEBUG :: Automatic Team Balance :: Player %s queued to be swapped", bz_getPlayerCallsign(playerID));
}

bool teamSwitch::teamsUnfair (bz_eTeamType *strongTeam = NULL, bz_eTeamType *weakTeam = NULL)
{
	bool teamOneStronger = (bz_getTeamCount(TEAM_ONE) > bz_getTeamCount(TEAM_TWO));

	*strongTeam = (teamOneStronger)  ? TEAM_ONE : TEAM_TWO;
	*weakTeam   = (!teamOneStronger) ? TEAM_TWO : TEAM_ONE;

	int strongTeamCount = bz_getTeamCount(*strongTeam),
		weakTeamCount   = bz_getTeamCount(*weakTeam);

	if ((strongTeamCount - weakTeamCount) < 2)
	{
		return false;
	}

	int allowedDiff = round((strongTeamCount + weakTeamCount) * .1); // Rounded 10% of the total players

	return !((strongTeamCount - weakTeamCount) < allowedDiff);
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
				if (teamsUnfair(&strongTeam, &weakTeam))
				{
					std::unique_ptr<bz_BasePlayerRecord> playerData(bz_getPlayerByIndex(playerID));

					// If the player capping is part of the strong team so we can switch them
					if (playerData->team == strongTeam)
					{
						allowctfdata->allow = false;

						bz_resetFlag(bz_getPlayerFlagID(playerID));
						bz_killPlayer(playerID, false);
						bztk_changeTeam(playerID, weakTeam);

						bz_sendTextMessagef(BZ_SERVER, playerID, "You were switched to the %s team.", bz_tolower(bztk_eTeamTypeLiteral(weakTeam)));
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
				if (teamsUnfair(&strongTeam, &weakTeam))
				{
					std::unique_ptr<bz_BasePlayerRecord> playerData(bz_getPlayerByIndex(playerID));

					// If the player who capped is part of the stronger team
					if (playerData->team == strongTeam)
					{
						bz_killPlayer(playerID, false);
						bztk_changeTeam(playerID, weakTeam);

						bz_sendTextMessagef(BZ_SERVER, playerID, "You were automatically switched to the %s team to make the teams fair.", bztk_eTeamTypeLiteral(weakTeam));
					}
				}
			}
		}
		break;

		case bz_ePlayerJoinEvent: // This event is called each time a player joins the game
		{
			bz_PlayerJoinPartEventData_V1* joinData = (bz_PlayerJoinPartEventData_V1*)eventData;

			players[joinData->playerID] = Player(joinData->playerID);
		}
		break;

		case bz_ePlayerSpawnEvent: // This event is called each time a tank is killed.
		{
			bz_PlayerSpawnEventData_V1* spawnData = (bz_PlayerSpawnEventData_V1*)eventData;

			int playerID = spawnData->playerID;
			Player &player = players[playerID];

			if (player.targetTeam != eNoTeam)
			{
				bztk_changeTeam(playerID, player.targetTeam);
				bz_sendTextMessagef(BZ_SERVER, playerID, "You were automatically switched to the %s team to make the teams fair.", bztk_eTeamTypeLiteral(player.targetTeam));

				player.targetTeam = eNoTeam;
				player.lastSwapTime = bz_getCurrentTime();
			}
		}
		break;

		case bz_eTickEvent:
		{
			// Check to see whether we were asked to always balance the teams
			if (bz_getBZDBBool("_atbAlwaysBalanceTeams"))
			{
				// Store this value for easy access
				bool teamsAreUnfair = teamsUnfair();

				if (teamsAreUnfair && !teamsUneven)
				{
					teamsUneven = true;
					timeFirstUneven = bz_getCurrentTime();
				}
				else if (!teamsAreUnfair && teamsUneven)
				{
					teamsUneven = false;
				}

				if (teamsUneven && (timeFirstUneven + bz_getBZDBInt("_atbBalanceDelay") < bz_getCurrentTime()))
				{
					if (!balanceQueued && balanceTeams())
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

		bz_PlayerRecordV2* victim;
		bz_eTeamType targetTeam;

		if (params->size() == 2)
		{
			victim     = (bz_PlayerRecordV2*)bz_getPlayerBySlotOrCallsign(params->get(0).c_str());
			targetTeam = bztk_eTeamType(params->get(1).c_str());
		}
		else
		{
			victim     = (bz_PlayerRecordV2*)bz_getPlayerByIndex(playerID);
			targetTeam = bztk_eTeamType(params->get(0).c_str());
		}

		if (victim != NULL)
		{
			if (victim->team == eObservers && victim->motto == "bzadmin")
			{
				bz_sendTextMessage(BZ_SERVER, playerID, "Warning: In order to prevent bzadmin clients from crashing, you cannot 'switch' bzadmin clients.");
				bz_freePlayerRecord(victim);
				return true;
			}

			if (bztk_changeTeam(victim->playerID, targetTeam))
			{
				if (playerID != victim->playerID)
				{
					bz_sendTextMessagef(BZ_SERVER, victim->playerID, "You were switched to the %s team by %s.",
									bztk_eTeamTypeLiteral(targetTeam),
									bz_getPlayerByIndex(playerID)->callsign.c_str());
				}
				else
				{
					bz_sendTextMessagef(BZ_SERVER, victim->playerID, "You switched to the %s team.", bztk_eTeamTypeLiteral(targetTeam));
				}
			}
			else
			{
				bz_sendTextMessagef(BZ_SERVER, playerID, "The %s team does not exist on this server.", bztk_eTeamTypeLiteral(targetTeam));
			}

			bz_freePlayerRecord(victim);
		}
		else
		{
			bz_sendTextMessagef(BZ_SERVER, playerID, "player '%s' not found", params->get(0).c_str());
		}

		return true;
	}
	else if (command == "balance" && bz_hasPerm(playerID, "switch"))
	{
		if (!teamsUnfair())
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
