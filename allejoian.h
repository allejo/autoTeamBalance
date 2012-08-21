#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "../../src/bzfs/GameKeeper.h"
#include "../../src/bzfs/bzfs.h"
#include "../../src/bzfs/CmdLineOptions.h"

/*
Code by mdskpr
*/
void fixTeamCount()
{
    int playerIndex, teamNum;
    for (teamNum = RogueTeam; teamNum < HunterTeam; teamNum++)
        team[teamNum].team.size = 0;
    for (playerIndex = 0; playerIndex < curMaxPlayers; playerIndex++) {
        GameKeeper::Player *p = GameKeeper::Player::getPlayerByIndex(playerIndex);
        if (p && p->player.isPlaying()) {
            teamNum = p->player.getTeam();
            if (teamNum == HunterTeam)
                teamNum = RogueTeam;
            team[teamNum].team.size++;
        }
    }
}

void removePlayer(int playerIndex)
{
    GameKeeper::Player *playerData
    = GameKeeper::Player::getPlayerByIndex(playerIndex);
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

        if (playerData->getIndex() == index) {
            // send all players info about player[playerIndex]
            broadcastMessage(MsgAddPlayer, (char*)buf - (char*)bufStart, bufStart);
        } else {
            directMessage(index, MsgAddPlayer, (char*)buf - (char*)bufStart, bufStart);
        }
        int teamNum = int(playerData->player.getTeam());
        team[teamNum].team.size++;
        sendTeamUpdate(-1, teamNum);
        fixTeamCount();


}

bool switchPlayer(int index, std::string teamColor)
{
    GameKeeper::Player *playerData = GameKeeper::Player::getPlayerByIndex(index);
    if (!playerData) return true;
    removePlayer(index);
    if (strcmp(teamColor.c_str(), "rogue") == 0) playerData->player.setTeam((TeamColor)RogueTeam);
    if (strcmp(teamColor.c_str(), "red") == 0) playerData->player.setTeam((TeamColor)RedTeam);
    if (strcmp(teamColor.c_str(), "green") == 0) playerData->player.setTeam((TeamColor)GreenTeam);
    if (strcmp(teamColor.c_str(), "blue") == 0) playerData->player.setTeam((TeamColor)BlueTeam);
    if (strcmp(teamColor.c_str(), "purple") == 0) playerData->player.setTeam((TeamColor)PurpleTeam);
    if (strcmp(teamColor.c_str(), "observer") == 0) playerData->player.setTeam((TeamColor)ObserverTeam);
    /*if ((int)playerData->player.getTeam() == (int)eRogueTeam ) playerData->player.setTeam((TeamColor)teamColor); */
    //else playerData->player.setTeam((TeamColor)ObserverTeam);
    addPlayer(playerData,index);
}

/*
Code by Joshua Sigona
*/
int bz_getPlayerCount(bool observers=false) {
	return bz_getTeamCount(eRogueTeam)+
	bz_getTeamCount(eRedTeam)+
	bz_getTeamCount(eGreenTeam)+
	bz_getTeamCount(eBlueTeam)+
	bz_getTeamCount(ePurpleTeam)+
	bz_getTeamCount(eRabbitTeam)+
	bz_getTeamCount(eHunterTeam)+
	(observers?bz_getTeamCount(eObservers):0);
}

bool bz_anyPlayers(bool observers=false) {
	return (bool)(bz_getTeamCount(eRogueTeam)+
	bz_getTeamCount(eRedTeam)+
	bz_getTeamCount(eGreenTeam)+
	bz_getTeamCount(eBlueTeam)+
	bz_getTeamCount(ePurpleTeam)+
	bz_getTeamCount(eRabbitTeam)+
	bz_getTeamCount(eHunterTeam)+
	(observers?bz_getTeamCount(eObservers):0));
}

int bz_randomPlayer(bz_eTeamType team=eNoTeam) {
	//Returns the index of a random player. Team is optional.
	//-1 is returned if no one is found.
	srand(time(NULL));
	if (team==eNoTeam) {
		if (bz_anyPlayers()) {
			bz_debugMessage(4,"There are players.");
			bz_APIIntList*playerlist=bz_getPlayerIndexList();
			int picked;
			picked=(*playerlist)[rand()%playerlist->size()];
			bz_deleteIntList(playerlist);
			return picked;
		} else {
			return -1;
		}
	} else {
		if (bz_getTeamCount(team)>0) {
			int picked=0;
			bz_APIIntList*playerlist=bz_getPlayerIndexList();
			while (true) {
				picked=rand()%playerlist->size();
				if (bz_getPlayerTeam(picked)==team) {
					break;
				}
			}
			bz_deleteIntList(playerlist);
			return picked;
		} else {
			return -1;
		}
	}
}

void bz_killPlayers(bz_eTeamType team=eNoTeam) {
	//Kills everyone if team and player are left unchanged.
	//Kills only one team if team is specified, and kills
	//a certain player when player is not -1.
	if (team!=eNoTeam) {
		bz_APIIntList*playerlist=bz_getPlayerIndexList();
		int i;
		for (i=0;i<playerlist->size();i++) {
			if (bz_getPlayerTeam((*playerlist)[i])==team) {
				bz_killPlayer((*playerlist)[i],false,BZ_SERVER);
			}
		}
		bz_deleteIntList(playerlist);
	} else {
		bz_APIIntList*playerlist=bz_getPlayerIndexList();
		int i;
		for (i=0;i<playerlist->size();i++) {
			bz_killPlayer((*playerlist)[i],false,BZ_SERVER);
		}
		bz_deleteIntList(playerlist);
	}
}

void bz_playSound(bz_eTeamType team=eNoTeam,int player=-1,const char*sound="") {
	//Plays a sound to a specific player if specified,
	//then sees if a specific team to play it for is specified,
	//otherwise, plays it for everyone in the game.
	if (player!=-1) {
		bz_sendPlayCustomLocalSound(player,sound);
	} else
	if (team!=eNoTeam) {
		bz_APIIntList*playerlist=bz_getPlayerIndexList();
		int i;
		for (i=0;i<playerlist->size();i++) {
			if (bz_getPlayerTeam((*playerlist)[i])==team) {
				bz_sendPlayCustomLocalSound((*playerlist)[i],sound);
			}
		}
		bz_deleteIntList(playerlist);
	} else
	{
		bz_APIIntList*playerlist=bz_getPlayerIndexList();
		int i;
		for (i=0;i<playerlist->size();i++) {
			bz_sendPlayCustomLocalSound((*playerlist)[i],sound);
		}
		bz_deleteIntList(playerlist);
	}
}

/*
Code by Vlad Jimenez
*/
std::string convertToString(int myInt) //Convert an integer into a string
{
	std::string myString;
	std::stringstream string;
	string << myInt; //Use an stringstream to pass an int through a string
	myString = string.str();

	return myString;
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

bool isValidCallsign(std::string myCallsign)
{
	bz_APIIntList *playerList = bz_newIntList(); //Get the list of current players
	bz_getPlayerIndexList(playerList);

	for (unsigned int i = 0; i < playerList->size(); i++)
	{
		bz_BasePlayerRecord *pr = bz_getPlayerByIndex(playerList->get(i)); //Get the player record for each individual player
		std::string tmp = pr->callsign.c_str();

		if (tmp.compare(myCallsign) == 0)
			return true; //The callsign inputted matches a callsign on the server

		bz_freePlayerRecord(pr); //Delete the player record
	}

	bz_deleteIntList(playerList); //Free the memory that was allocated for this

	return false; //The callsign was not found
}
    
bool isValidPlayerID(std::string myID) //Check to see if a player ID exists on the server
{
	if(myID.find("#") != 0) //If it doesn't have a # it's not a player ID
	{
		return false;
	}

	bz_APIIntList *playerList = bz_newIntList(); //Get the list of all the current player IDs
	bz_getPlayerIndexList(playerList);

	for (unsigned int i = 0; i < playerList->size(); i++ )
	{
		if (playerList->get(i) == atoi(myID.erase(0,1).c_str()))
		{
        	bz_deleteIntList(playerList); //Free the memory allocated
			return true; //The player ID inputted, matches an ID that is on the map
		}
	}

	bz_deleteIntList(playerList); //Free the memory allocated

	return false; //Player ID was not found on the server
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