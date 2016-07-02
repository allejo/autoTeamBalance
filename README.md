Automatic Team Balance
======================

This BZFlag plugin will ensure fair teams for a 2 team CTF match. This plugin has the capability to swap players to a different team (including observer) without requiring them to rejoin; this will allow for the plugin to automatically balance teams based on a delay or whenever a flag is about to captured or has been captured.

Author
------

Vladimir "allejo" Jimenez

Compiling
---------

### Requirements

- BZFlag 2.4.0+

- [bzToolkit](https://github.com/allejo/bztoolkit/)

### How To Compile

1.  Check out the BZFlag source code.

    `git clone -b 2.4 https://github.com/BZFlag-Dev/bzflag.git`

2.  Go into the newly checked out source code and then the plugins directory.

    `cd bzflag/plugins`

3.  Run a git clone of this repository from within the plugins directory. This should have created a new autoTeamBalance directory within the plugins directory.

    `git clone https://github.com/allejo/autoTeamBalance.git`

4.  Now you will need to checkout the required submodules so the plugin has the proper dependencies so it can compile properly.

    `cd autoTeamBalance; git submodule update --init`

5.  Instruct the build system to generate a Makefile and then compile and install the plugin.

    `cd ../..; ./autogen.sh; ./configure --enable-custom-plugins=autoTeamBalance; make; make install;`

Server Details
--------------

### How to Use

To use this plugin after it has been compiled, simply load the plugin via the configuration file.

`-loadplugin autoTeamBalance`

### Custom BZDB Variables

    _atbAlwaysBalanceTeams
    _atbBalanceTeamsOnCap
    _atbBalanceDelay
    _atbDisableCapWithUnfairTeams
    _atbResetFlagToBase
    
_atbAlwaysBalanceTeams

- Default: _false_

- Description: This variable corresponds with the `_atbBalanceDelay` variable, where it will always be checking if teams are unfair. If the teams were to be unfair for more than 30 seconds (the default `_atbBalanceDelay` value), then it will balance the teams.

_atbBalanceTeamsOnCap

- Default: _false_

- Description: With this variable set to true, whenever a flag is capture it will balance the teams as well as switching the player who captured the flag unevenly.

_atbBalanceDelay

- Default: 30

- Description: This variable corresponds with the `_atbAlwaysBalanceTeams` variable, where it will check if unfair teams have lasted for whatever value this variable contains in seconds.

_atbDisableCapWithUnfairTeams

- Default: _false_

- Description: With this variable set to true, it will disallow a capture when teams are unfair, will switch the player who attempted to capture the flag, balance the teams, and move the flag to either the middle of the map or back to its base.

_atbResetFlagToBase

- Default: _false_

- Description: When `_atbDisableCapWithUnfairTeams` is set to true, the team flag will be sent to the middle of the map (position 0 0 0) by default. With this value is set to true, it will send the team flag back to its home base.

### Using Custom BZDB Variables

Because this plugin utilizes custom BZDB variables, using `-set _atbAlwaysBalanceTeams 1` in a configuration file or in an options block will cause an error; instead, `-setforced` must be used to set the value of the custom variable: `-setforced _atbAlwaysBalanceTeams 1`. These variables can be set and changed normally in-game with the `/set` command.

*Warning:* For boolean BZDB variables, it is required that you use the respective 0 or 1 equivalent to true or false.

### Custom Slash Commands

    /balance
    /switch <player slot or callsign> <rogue|red|green|blue|purple|observer>
    /switch <rogue|red|green|blue|purple|observer>

/balance

- Permission Requirement: Switch

- Description: Force a balance of the teams

/switch

- Permission Requirement: Switch

- Description: If a player slot or callsign is provided, it will allow an admin to switch another player. If there is no player slot or callsign, then the admin will be switched.

Contributing
------------

If you would like to contribute to this project, please be sure to follow the formatting that already exists (e.g. using 4 spaces for indentation instead of tabs). Please be sure that if you fork this repository, you sync the fork respectively with your fork to get any changes that have been made to the repository. Take a look at GitHub's [syncing a fork](https://help.github.com/articles/syncing-a-fork) article that explains the process.

License
-------

[GNU General Public License v3](https://github.com/allejo/autoTeamBalance/blob/master/LICENSE.markdown)
