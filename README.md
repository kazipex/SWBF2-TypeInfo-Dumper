# SWBF2Dumper
A fork of BFVDumper, ported to work with Star Wars Battlefront II (2017).

Credits go to reunion (UnknownCheats) who originally wrote this dumper targeting Battlefield V, artemking4 who updated reunion's original tool and published it as BFVDumper, adding JSON export and ArmchairDevelopers for the SigScan module extracted from their OpenGameCamera project, which allows this dumper to target Star Wars Battlefront 2 directly.

Usage:

Build the project (x64, Release).

Launch Star Wars Battlefront II and load into a level.

Inject the built DLL using the injector of your choice.

A console window should open that states that it is dumping, this process should only last a few seconds.

A dump.json will now be written to the game's install folder.

You can use this to locate classes to edit using memory scanners.

If the console prints Signature scan failed to resolve a valid TypeInfo pointer instead of done, the signature didn't find a match this is most likely because a future game update changed the surrounding code enough to break the pattern, in which case the signature in StaticOffsets.h needs to be changed to support the changes.
