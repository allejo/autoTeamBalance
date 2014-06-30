/*
 League Overseer
 Copyright (C) 2013-2014 Vladimir Jimenez
 
 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <cmath>
#include <cstdlib>
#include <memory>
#include <string>

#include "bzfsAPI.h"
#include "bzToolkitAPI.h"

class teamSwitch : public bz_Plugin, public bz_CustomSlashCommandHandler
{
public:
  virtual const char* Name () {return "Automatic Team Balance";}
  virtual void Init (const char* commandLine);
  virtual void Cleanup (void);
  virtual void Event (bz_EventData *eventData);
  
  virtual bool SlashCommand (int playerID, bz_ApiString command, bz_ApiString message, bz_APIStringList *params);
  
  virtual void balanceTeams (void);
  virtual bool playerCountLow (void);
  virtual void resetFlag (int flagID, int playerID);
  virtual bool teamsUnfair (bz_eTeamType &strongTeam, bz_eTeamType &weakTeam);
  virtual bz_APIIntList* getStrongestTeamPlayers(bz_eTeamType team, int numberOfPlayers);
  
  bool teamsUneven;
  double timeFirstUneven;
  bz_eTeamType TEAM_ONE, TEAM_TWO;
};

BZ_PLUGIN(teamSwitch)

void teamSwitch::Init (const char* /*commandLine*/)
{
  // Register our events with Register()
  Register(bz_eAllowCTFCaptureEvent);
  Register(bz_eCaptureEvent);
  Register(bz_eTickEvent);
  
  // Register our custom slash commands
  bz_registerCustomSlashCommand("balance", this);
  bz_registerCustomSlashCommand("switch", this);
  bz_registerCustomSlashCommand("testteambalance", this);
  
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

bz_APIIntList* teamSwitch::getStrongestTeamPlayers(bz_eTeamType team, int numberOfPlayers)
{
  bz_APIIntList* playerlist = bztk_getTeamPlayerIndexList(team);
  bz_APIIntList* resp = bz_newIntList();
  std::map<int, int> playerKDRatio;    //  Map to hold the Kill/Death Ratio of each team player.  Std maps automatically sort keys, hence using KD ratio as key
  
  for(unsigned int i=0; i<playerlist->size(); i++) {
    int playerId = playerlist->get(i);
    int losses = bz_getPlayerLosses(playerId);
    int wins = bz_getPlayerWins(playerId);
    wins++; // increase by 1 to avaoid a division by zero error
    losses++; // increase by 1 to avoid a division by zero error
    double ratio = wins;
    ratio/=losses;
    int key = (ratio*100)+playerId; //Guarantees a unique index for the map while retaining ratio sort value.  If ration is the same between two players, higher index will sort higher
    playerKDRatio[key] = playerId;
  }
  
  int j=0;
  for(std::map<int, int>::reverse_iterator ii=playerKDRatio.rbegin(); ii!=playerKDRatio.rend(); ++ii){
    int second = (*ii).second;
    resp->push_back(second);
    j++;
    if(j==numberOfPlayers) break;
  }
  bz_deleteIntList(playerlist);
  return resp;
}

void teamSwitch::balanceTeams ()
{
  // If the player count is low, then there is no need to balance the teams
  if (playerCountLow())
  {
    bz_debugMessagef(2, "DEBUG :: Automatic Team Balance :: Player Count is low");
    return;
  }
  
  // Find which team is the strongest team by getting the amount of players on it
  bz_eTeamType strongTeam = (bz_getTeamCount(TEAM_ONE) > bz_getTeamCount(TEAM_TWO)) ? TEAM_ONE : TEAM_TWO;
  
  // See if the difference between the teams is an even or odd amount, we always need to switch an
  // even amount of players so subtract one if the amount is odd
  int teamDifference = abs(bz_getTeamCount(TEAM_ONE) - bz_getTeamCount(TEAM_TWO));
////  if (teamDifference/2 != 0) { teamDifference--; }
  
  // We will always have an even number of players here
  int amountOfPlayersToSwitch = teamDifference / 2;
  
  bz_APIIntList* playerlist = getStrongestTeamPlayers(strongTeam, amountOfPlayersToSwitch);

  
  
  // Loop through the players to switch
  for (int i = 0; i < playerlist->size(); i++)
  {
    int playerMoved = playerlist->get(i);
    if (strongTeam == TEAM_TWO)
    {
      bztk_changeTeam(playerMoved, TEAM_ONE);
      bz_sendTextMessagef(BZ_SERVER, playerMoved, "You were automatically switched to the %s team.", bz_tolower(bztk_eTeamTypeLiteral(TEAM_ONE).c_str()));
    }
    else if (strongTeam == TEAM_ONE)
    {
      bztk_changeTeam(playerMoved, TEAM_TWO);
      bz_sendTextMessagef(BZ_SERVER, playerMoved, "You were automatically switched to the %s team.", bz_tolower(bztk_eTeamTypeLiteral(TEAM_TWO).c_str()));
    }
    
    // Commented out due to purpose being uncertain as to if it works or not
    //std::unique_ptr<bz_BasePlayerRecord> playerData(bz_getPlayerByIndex(playerMoved));
    //playerData->spawned = true;
  }
  bz_deleteIntList(playerlist);

  // This variable is used for the automatic balancing based on a delay to mark the teams as
  // unfair or not
  teamsUneven = false;
}

bool teamSwitch::playerCountLow ()
{
  
  int difference = abs(bz_getTeamCount(TEAM_ONE) - bz_getTeamCount(TEAM_TWO));

////Dracos Change:  Allow zero team count so if all players in a team leave, it will auto balance other team if difference > 1
  if(bz_getTeamCount(TEAM_ONE)<4 && bz_getTeamCount(TEAM_TWO)<4 && difference<2) return true;
  
/*
  if (bz_getTeamCount(TEAM_ONE) == 0 || bz_getTeamCount(TEAM_TWO) == 0)
  {
    return true;
  }
  
  // Break if it's 2-1, 2-3, or 3-4
  if (((bz_getTeamCount(TEAM_ONE) == 1 || bz_getTeamCount(TEAM_TWO) == 1) && (bz_getTeamCount(TEAM_ONE) == 2 || bz_getTeamCount(TEAM_TWO) == 2)) ||
      ((bz_getTeamCount(TEAM_ONE) == 2 || bz_getTeamCount(TEAM_TWO) == 2) && (bz_getTeamCount(TEAM_ONE) == 3 || bz_getTeamCount(TEAM_TWO) == 3)) ||
      ((bz_getTeamCount(TEAM_ONE) == 3 || bz_getTeamCount(TEAM_TWO) == 3) && (bz_getTeamCount(TEAM_ONE) == 4 || bz_getTeamCount(TEAM_TWO) == 4)))
  {
    return true;
  }
*/
  
  return false;
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
  strongTeam = (bz_getTeamCount(TEAM_ONE) > bz_getTeamCount(TEAM_TWO)) ? TEAM_ONE : TEAM_TWO;
  weakTeam   = (bz_getTeamCount(TEAM_ONE) > bz_getTeamCount(TEAM_TWO)) ? TEAM_TWO : TEAM_ONE;
 
  
  
  // Calculate the amount of bonus points given for a flag capture
  int bonusPoints = 8 * (bz_getTeamCount(weakTeam) - bz_getTeamCount(strongTeam)) + 3 * bz_getTeamCount(weakTeam);

  return (bonusPoints < 0);
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
	// Check if our player count is low
	if (playerCountLow())
	{
	  allowctfdata->allow = false;
	  
	  // Check which team flag the player is carrying so we can reset it
	  resetFlag(0, playerID);
	  resetFlag(1, playerID);
	  
	  bz_sendTextMessage(BZ_SERVER, playerID, "Do not capture the flag with unfair teams; the flag has been reset.");
	  
	  return;
	}
	
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
	  
	  bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "Balancing unfair teams...");
	  balanceTeams();
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
	// If the player count is low, don't do anything because we can't do much
	if (playerCountLow())
	{
	  return;
	}
	
	// Check if the teams are unfair
	if (teamsUnfair(strongTeam, weakTeam))
	{
	  std::unique_ptr<bz_BasePlayerRecord> playerData(bz_getPlayerByIndex(playerID));
	  
	  // If the player who capped is part of the stronger team
	  if (playerData->team == strongTeam)
	  {
	    // Switch the player
	    bztk_changeTeam(playerID, weakTeam);
	    
	    // Kill the player so they are forced to spawn on the base
	    bz_killPlayer(playerID, 1);
	    
	    bz_sendTextMessagef(BZ_SERVER, playerID, "You were automatically switched to the %s team to make the teams fair.", bztk_eTeamTypeLiteral(weakTeam).c_str());
	  }
	  
	  bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "Balancing unfair teams...");
	  balanceTeams();
	}
      }
    }
      break;
      
    case bz_eTickEvent:
    {
      bz_eTeamType strongTeam, weakTeam;
      
      // Check to see whether we were asked to always balance the teams
      if (bz_getBZDBBool("_atbAlwaysBalanceTeams"))
      {
	// If the teams are low, then there's no need to check whether or not to the teams are uneven
	if (playerCountLow())
	{
	  return;
	}
	
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
    
    balanceTeams();
    bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "Balancing unfair teams...");
    bz_sendTextMessage(BZ_SERVER, playerID, "Teams have been balanced");
    
    return true;
  }
  else
  {
    bz_sendTextMessagef(BZ_SERVER, playerID, "Unknown command [%s]", command.c_str());
    return true;
  }
}