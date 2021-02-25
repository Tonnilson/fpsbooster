# FPS Booster
The main purpose of this plugin is to kill off combat log, a known performance killer that runs even when turned off. This is a collaborative effort with SH4RK who provided information and ID's for removing UI Elements. Released in a not complete state, missing other functionality that was planned and also proned to crashing due to invalid data. Releasing source code for if anyone wants to improve and continue the plugin due to me leaving Blade and Soul.

## In-game Keybinds
###### Alt + 1
Turns off/on a lot of text parsing & display, somewhat similar to stripping local file but not quite the same gains.
###### Alt + 3
Turns off/on chatbox based notifications (Used XXX item, boss notifications, death log etc). These are notifications only in the chat box, not on-screen.
###### Alt + X
An alternative to Ctrl + X but hides only select UI elements, this adds a significant boost in performance depending on how many elements you hide. There is a known issue with DirectX 9 (D3D9) having a performance issue drawing text on the screen, limiting the amount text being drawn to the screen will increase performance and lower frametime (time spent in a frame).
###### Alt + Insert
Loads or reloads booster.txt located in `Documents\BnS\booster.txt`

## booster.txt
`booster.txt is optional and not required for use of fpsbooster.dll`

This is a way for users to customize which UI elements are hidden from the screen.

###### Default booster.txt list
```
# chatbox
3450
# Premium/Clan Buff/Other Buffs
5507
# HM Level / Character Name / Money
1282
# Quick Access Icons
1283
1279
# XP bar
1284
# Quest Bar
6139
6222
# Auto combat icon
5514
# Map
6126
# Party
5496
```

###### List of known ID's provided from SH4RK, some ID's are no longer valid and have changed.
```
Map: 6126
Premiun/Clan buff: 5507
Bottomright icons big: 1283
Bottomright icons small: 1279
Whole name, levels unity thing: 1282
Chat: 3450
XP: 1284
AutoCombat: 5514
Quest: 6139
Basin: 6222
Alliance: 5574 reappearance = ?
Party: 5496 reappearance = ?

Minimalist:
Buffs: 5499, other ones should be around here
Stamina: 5542
HP, Focus, dragon hearts: 5519
Map all: 6125
boss hp: 3117
other boss thing, better?: 2618
crtl r thing: 973
Boss buffs: 3115
```

# FAQ
### Does this remove the need for a custom local dat file?
No, but it does compliment it very well and using this in conjunction will see a significant boost in performance. It will however help provide a noticeble boost in performance with a default/vanilla local dat file, good alternative for those that want to keep all text in the game and use a Hotkey toggle for removing various text.

### Does this work on all regions?
Confirmed working on NA/EU, TW and KR. Other regions may or may not work but I can tell you it definitely will not work on garena servers due to their anti-cheat.

### My game crashes a lot when using Alt + X too many times
This is a known problem, I implemented some basic data verification to alleivate crashes but it is still prone to crashing, the code behind it is not finished / bullet proof that has received extensive testing. There is also functionality missing as well due to demotivation and quitting Blade and Soul.
