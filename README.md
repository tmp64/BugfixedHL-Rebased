Bugfixed and improved Half-Life
===============================

Bugfixed and Improved Half-Life Release (*BugfixedHL* for short) is a Half-Life modification that aims
to improve DM experience for players and fix server bugs for server owners while being completely
backwards-compatible with vanilla clients and servers.

*BugfixedHL-Rebased* is port of [Lev's BugfixedHL](https://github.com/LevShisterov/BugfixedHL) on
new [Half-Life SDK](https://github.com/ValveSoftware/halflife).
It features improved UI using VGUI2 library and refactored source code for better maintainablity.


Features
--------

The mod features a number of improvements over stock client. Please note: some of them may not be available at the time.

Client:
- [x] Unlocked bunnyhopping
- [ ] Colored HUD
- [ ] Raw mouse input via DirectInput
- [x] Customizable crosshairs (like in CS:GO)
- [ ] Improved user interface
  - [x] Scoreboard
  - [x] MOTD
  - [x] Chatbox
  - [x] Spectator UI
  - [x] Team selection
  - [ ] Command menu
  - [ ] Options dialog
  - [ ] Demo list dialog
- [ ] Automatic demo recording
- [ ] Support for WeaponMod
- [ ] Slowhacking protection (for older game versions)


Server:
- Bugfixes, crash fixes
- Selfgauss can be disabled
- Welcome camera mode - after connecting, player is put into semi-spectator mode and can spawn with MOUSE1
- HTML and Unicode MOTD for client that support them
- Working spectator mode
- Invisible model fix
- UTF-8 support


SDK changes
-----------

BugfixedHL has been integrated into Valve's latest HLSDK and has been refactored:

- CMake as build system (instead of Makefiles and VS projects).
- Source code formatted to one style with clang-format.
- Organized source code files:
  - HLSDK sources moved to */src/*.
  - */cl_dll*, */dlls* and */game_shared* moved to */src/game/*.
  - */game_shared* cleaned up from unused code.
  - removed */utils* completely (used to contain utilities like map and sprite compilers).
- Client sources refactoring:
  - VGUI1 replaced by VGUI2 (like in CS1.6).
  - Moved all VGUI code to *client/vgui*.
  - Moved all HUD elements to *client/hud*, each of them now has its own *.h* file.
  - Replaced HUD messages and commands macros with templates.
  - HUD elements are no longer referenced in *hud.h* (improves compilation times when changing *hud/\*.h*.
  - Removed unused code.
- Fixed include guards in common header files.
- Documented engine APIs (thanks to [SoloKiller](https://github.com/SoloKiller)).


Supported game versions
-----------------------

Type `version` in the console. You will see something like this.

```
] version 
Protocol version 48
Exe version 1.1.2.2/Stdio (valve)
Exe build: 15:17:55 Jul 24 2019 (8308)
```

*8308* in the last line is your engine version.

| Engine version | Status          |
| :------------: | --------------  |
| 3xxx           | Not supported   |
| 4554           | Being worked on |
| 8xxx+          | Supported       |
| Anything else  | Not supported   |


Reporting Issues
----------------

If you encounter an issue while using BugfixedHL, first search the [issue list](https://github.com/tmp64/BugfixedHL-Rebased/issues)
to see if it has already been reported. Include closed issues in your search.

If it has not been reported, create a new issue with at least the following information:

- a short, descriptive title;
- a detailed description of the issue, including any output from the command line or game console;
- steps for reproducing the issue;
- your system information\*;
- the `version` output from the in-game console;
- the `about` output from the in-game console.

Please place logs either in a code block (press `M` in your browser for a GFM cheat sheet) or a [gist](https://gist.github.com).

\* The preferred and easiest way to get this information is from Steam's Hardware Information viewer from the
menu (`Help -> System Information`). Once your information appears: right-click within the dialog, choose `Select All`,
right-click again, and then choose `Copy`. Paste this information into your report, preferably in a code block.


Conduct
-------

There are basic rules of conduct that should be followed at all times by everyone participating in the
discussions.  While this is generally a relaxed environment, please remember the following:

- Do not insult, harass, or demean anyone.
- Do not intentionally multi-post an issue.
- Do not use ALL CAPS when creating an issue report.
- Do not repeatedly update an open issue remarking that the issue persists.

Thanks
------

- Lev for creating [the original BFAIHLSDK](https://github.com/LevShisterov/BugfixedHL).
  - Valve for HLSDK release.
  - Willday for his HLSDK patch.
  - BubbleMod and Bigguy from hlpp.thewavelength.net for parts of spectator code.
  - Uncle Mike from hlfx.ru for his Xash3D engine which was very helpful in hard moments.
  - KORD_12.7 for constant helping and nice suggestions.
  - JetBrains company for free access to great developer tools.
- SamVanheer for [Half-Life Enhanced](https://github.com/SoloKiller/HLEnhanced) and reverse engineering GoldSrc engine.
- AGHL.RU community for bug reporting and suggestions.
