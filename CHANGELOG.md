### 1.6.2 (planned)

- `Fixed` Plug-in no longer attempts to balance teams indefinitely due to queued players not having died yet
- `Fixed` Ability to /switch players based on their callsign
- `Improved` Use of new bzToolkit API functions

### 1.6.1 (66)

- `New` Reintroduced the random selection for team swapping and set it to default
- `New` Added BZDB variable `_atbSwapPlayerAlgorithm`

### 1.6.0 (64)

- `New` Balance algorithm now chooses players based on strength
- `New` Change players only after death
- `New` Use of updated bzToolkit API
- `New` New algorithm to determine if teams are unfair
- `Fixed` Slash commands now actually balance teams
- `Improved` Use of more C++11 features for improved code quality
- `Improved` Team changing rules

### 1.5.0 (39)

 - Rewrote the plugin from scratch using bzToolkit
 - Fixed BZDB variables to accept `-setforced` values
 - Changed the messages sent to the players when they have been swapped

### 1.4.1 (14)

 - Fixed bug where 3-2 caps caused endless loop

### 1.4.0 (11)

 - Removed allejoian.h
 - Code clean up
 - Fixed bug where player slot would default to 0 with /switch

### 1.3.0 (10)

 - Added a new option to void captures and send the flag to the middle of the map while balancing the teams

### 1.2.0 (8)

 - Support for all 4 team colors
 - Fixed switch to the same team bug
 - Fixed the bug where you can't kill after being switch
 - Made switch message more visible

### 1.1.0 (6)

 - Added BZDB support and commandline parameters
 - Fixed unfairness balance when teams had less than 5 players
 - Added "switch" permission

### 1.0.0 (1)

 - Initial Release