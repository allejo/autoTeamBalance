#include "allejoian.h"
#include "bzfsAPI.h"
#include <cmath>
#include "../../src/bzfs/GameKeeper.h"
#include "../../src/bzfs/bzfs.h"
#include "../../src/bzfs/CmdLineOptions.h"

class kierra : public bz_Plugin, public bz_CustomSlashCommandHandler
{
public:
    virtual const char* Name (){return "kierra";}
    virtual void Init(const char* /*commandLine*/);
    virtual void Cleanup(void);

    virtual void Event(bz_EventData *eventData);
    virtual bool SlashCommand(int playerID, bz_ApiString command, bz_ApiString message, bz_APIStringList *params);
    
    bool teamsUneven;
    double timeFirstUneven;
};

BZ_PLUGIN(kierra);

void kierra::Init(const char* /*commandLine*/)
{
    Register(bz_eCaptureEvent);
    Register(bz_eTickEvent);

    bz_registerCustomSlashCommand("switch", this);
    
    teamsUneven = false;

    bz_debugMessage(4,"kierra plugin loaded");
}

void kierra::Cleanup()
{
    Flush();
    bz_removeCustomSlashCommand("switch");

    bz_debugMessage(4,"kierra plugin unloaded");
}

void kierra::Event(bz_EventData* eventData)
{
    switch (eventData->eventType)
    {
        case bz_eCaptureEvent: // A flag is captured
        {
            bz_CTFCaptureEventData_V1* ctfdata = (bz_CTFCaptureEventData_V1*)eventData;

            if (bz_getTeamCount(eRedTeam)/bz_getTeamCount(eBlueTeam) < 1.3)
            {
                switchPlayer(ctfdata->playerCapping, "blue");
                bz_sendTextMessage(BZ_SERVER, ctfdata->playerCapping, "You're an asshole for capping! You've been moved to the blue team.");
            }
            else
            {
                switchPlayer(ctfdata->playerCapping, "red");
                bz_sendTextMessage(BZ_SERVER, ctfdata->playerCapping, "You're an asshole for capping! You've been moved to the red team.");
            }
        }
        break;

        case bz_eTickEvent: // The server's main loop has iterated
        {
            bz_TickEventData_V1* tickdata = (bz_TickEventData_V1*)eventData;
            
            if (bz_getTeamCount(eRedTeam) == 0 || bz_getTeamCount(eBlueTeam) == 0) break;

            if (bz_getTeamCount(eRedTeam)/bz_getTeamCount(eBlueTeam) < 1.3 && !teamsUneven)
            {
                teamsUneven = true;
                timeFirstUneven = tickdata->eventTime;
            }
            else if (bz_getTeamCount(eBlueTeam)/bz_getTeamCount(eRedTeam) < 1.3 && !teamsUneven)
            {
                teamsUneven = true;
                timeFirstUneven = tickdata->eventTime;
            }
            else if ((bz_getTeamCount(eBlueTeam)/bz_getTeamCount(eRedTeam) < 1.3 || bz_getTeamCount(eRedTeam)/bz_getTeamCount(eBlueTeam) < 1.3) && teamsUneven)
            {
                teamsUneven = false;
            }
            
            if (teamsUneven && (timeFirstUneven + 30 == bz_getCurrentTime()))
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
                    while (true)
                    {
                        playerMoved = bz_randomPlayer();
                        bz_BasePlayerRecord* pr = bz_getPlayerByIndex(playerMoved);
                        
                        if (pr->team != strongTeam)
                        {
                            break;
                        }
                    }
                    
                    if (strongTeam == eBlueTeam)
                    {
                        switchPlayer(playerMoved, "red");
                        bz_sendTextMessage(BZ_SERVER, playerMoved, "You have been switched to the communist team");
                    }
                    else if (strongTeam == eRedTeam)
                    {
                        switchPlayer(playerMoved, "blue");
                        bz_sendTextMessage(BZ_SERVER, playerMoved, "You have been switched to the blue team.");
                    }
                }
            }
        }
        break;

        default:
        break;
    }
}

bool kierra::SlashCommand(int playerID, bz_ApiString command, bz_ApiString message, bz_APIStringList *params)
{
    if (command == "switch" && bz_hasPerm(playerID, "ban"))
    {
        if (isValidPlayerID(params->get(0).c_str()) || isValidCallsign(params->get(0).c_str()))
        {
            if (strcmp(params->get(1).c_str(), "rogue") ||
                strcmp(params->get(1).c_str(), "red") ||
                strcmp(params->get(1).c_str(), "blue") ||
                strcmp(params->get(1).c_str(), "green") ||
                strcmp(params->get(1).c_str(), "purple") ||
                strcmp(params->get(1).c_str(), "observer"))
            {
                int maxPlayersOnTeam = 0;
                if (strcmp(params->get(1).c_str(), "rogue")) maxPlayersOnTeam = bz_getTeamPlayerLimit(eRogueTeam);
                if (strcmp(params->get(1).c_str(), "red")) maxPlayersOnTeam = bz_getTeamPlayerLimit(eRedTeam);
                if (strcmp(params->get(1).c_str(), "blue")) maxPlayersOnTeam = bz_getTeamPlayerLimit(eBlueTeam);
                if (strcmp(params->get(1).c_str(), "green")) maxPlayersOnTeam = bz_getTeamPlayerLimit(eGreenTeam);
                if (strcmp(params->get(1).c_str(), "purple")) maxPlayersOnTeam = bz_getTeamPlayerLimit(ePurpleTeam);
                if (strcmp(params->get(1).c_str(), "observer")) maxPlayersOnTeam = bz_getTeamPlayerLimit(eObservers);
                
                if (maxPlayersOnTeam > 0)
                {
                    switchPlayer(atoi(std::string(params->get(0).c_str()).erase(0,1).c_str()), params->get(1).c_str());
                }
                else
                {
                    bz_sendTextMessagef(BZ_SERVER, playerID, "The %s team does not exist on this map.", params->get(1).c_str());
                }
            }
            else
            {
                bz_sendTextMessagef(BZ_SERVER, playerID, "The %s team does not exist.", params->get(1).c_str());
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

