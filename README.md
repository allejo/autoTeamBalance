# Automatic Team Balance

This BZFlag plug-in has the capability of swapping players to a different team without requiring a rejoin on the user's behalf.

## Compiling
Add this plug-in to the BZFlag build system and compile.

    sh newplug.sh teamSwitch
    cd teamSwitch
    make
    sudo make install

## Setup

    bzfs -loadplugin /path/to/teamSwitch.so,1,2,3,4

### Parameters
 1. [Integer] The amount of seconds the plugin should wait before automatically balancing teams when they are unfair. Default is 30 seconds.
 2. [Boolean] This will determine whether the plugin should automatically balance teams after X seconds of being unfair. Default is 0.
 3. [Boolean] This will determine whether the plugin should automatially balance teams on the event of a flag being captured. Default is 0.
 4. [Boolean] If set to true, this will override parameter 3. This option will void a capture event and send the flag to the middle of the map while automatically balancing the teams.

### BZDB Variables
These are custom BZDB variables that can be set in game in order to change the plug-in's functionality.

 * _atbBalanceDelay - corresponds with parameter #1
 * _atbAlwaysBalanceTeams - corresponds with parameter #2
 * _atbBalanceTeamsOnCap - corresponds with parameter #3
 * _atbDisableCapWithUnfairTeams  - corresponds with parameter #4

### Slash Commands
These slash commands require the 'switch' permission and should only be given to admins to avoid abuse from players.

```
    /balance
    /switch <player slot> <rogue|red|green|blue|purple|observer>
    /switch <rogue|red|green|blue|purple|observer>
```
<em>Note: Unlike other plug-ins, this plug-in requires that you enter the # for a player slot in order for the commands to feel like a natural BZFlag slash command.</em>

## The Algorithm

```8 * (weakTeam - strongTeam) + 3 * (weakTeam)```

'weakTeam' is defined as the team with less players and 'strongTeam' is defined as the team with more players. If this equation returns a negative number, then the teams are unfair and the plug-in will balance the teams provided the plug-in is configured to do so.

## Notes
Because this plug-in bypasses the BZFlag API, this plug-in will not function on a Windows Server.

## License

BSD

## Thanks To
 * Murielle
 * mdskpr
 * sigonasr2
