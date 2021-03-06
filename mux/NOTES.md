---
author: Brazil
date: July 2020
title: NOTES
---

This is a list of information that never seems to be handy enough when you
need it.

# Handy Stuff:

 - The default password for `Wizard(#1)` is `potrzebie`.

 - For new games, change the password for `#1` first thing and *write it down*
   in a safe place offline.

 - It is worth noting that keeping a record of changes in the `#1` character
   password might save you some flatfile hacking for forgotten passwords.

 - The default port has been changed to `2860`.  Change this in the
   `netmux.conf` or `GAMENAME.conf` file.

 - Always be sure to ftp files in binary mode.  Otherwise, you can expect
   your data to be unusable.

 - Default `GAMENAME` will be `netmux`.  If you changed the `GAMENAME` in
   `mux.config`, be sure to change the filenames in `GAMENAME.conf` as well,
   otherwise, TinyMUX and (perhaps more importantly), `./Backup`, won't be
   able to find your DB.

 - Read `wizhelp config parameters` when you get the TinyMUX started.  If you
   are porting from PennMUSH, for example, where the master room is set
   as `#2`, you would have to place the config parameter `master_room #2` in
   your configuration file.

 - If you had a mail database previously, remember to adjust
   `mail_expiration` accordingly, or else all `@mail` older than the default
   value of 14 days will be deleted.

 - `./Backup` has superceeded the traditional use of `db_unload` for making
   flatfiles.  The script makes a flatfile of the DB and also creates a `tar.gz`
   file containing the `mail.db`, `comsys.db`, configuration files, and the
   customized files in `game/text`.  When porting, all you have to do is place
   the backup in the game directory, extract the archive and `db_load` before
   restarting the game.

# Changes to `dbconvert`:

`./dbconvert` is the means by which the binary game data is converted to
flatfile format and back again.  The `db_load` and `db_unload` scripts
simplify the process for the user.

The syntax of `db_load` is:
```
    ./db_load netmux netmux.flat netmux.db
```
This converts flatfiled database to binary for use by the server and would be
done with `dbconvert` thus:
```
    ../bin/dbconvert -dnetmux -inetmux.flat -onetmux.db -l
```
The syntax of `db_unload` is:
```
    ./db_unload netmux netmux.db.new netmux.flat
```
This converts binary data to flatfile for would be done with `dbconvert` thus:
```
    ../bin/dbconvert -dnetmux -inetmux.db.new -onetmux.flat -u
```

# On Flatfiles:

 - You MUST unload to flatfile if you are using a disk-based database when
   you move to a new machine, no matter what platform; only the flatfile is
   portable!

 - When you unload the database to flatfile format it is important to
   use your original `db_unload` or `dbconvert`.  *We cannot stress this enough.*
   Data loss is possible, especially since `db_unload` or `dbconvert` sometimes
   changes between releases.

 - Flatfiles are the most stable format to store and move your data in.  Yes,
   the occasional miracle happens to let someone bring a game back up from
   binary data moved from the nether regions of the net.  However, this isn't
   the place for beginners or the faint of heart.  As always, store backups
   offline.

# On Making Backups:

TinyMUX 2.13 includes a backup script.  It produces a flatfile with the
name `GAMENAME.DATE.tar.gz` that contains both the `mail.db` and
`comsys.db` which will appear in the `game` directory.  To make use of the
`Backup` script:

 - `@shutdown` the game.

 - `cd` to the `game` directory.

 - Type `./Backup`
    
 - Restart the game using `./Startmux`

 - Move the backup to onsite and offsite storage locations.

# On Tools:

 - `announce.c` sits and listens on a specified port.  Whenever anyone
   connects, it announces a message and disconnects them.

 - You should create a message and call the file `message_file`.  This will
   serve as your announcement to people trying to connect.

 - Compile `announce.c` by typing `gcc -o announce announce.c`.  If you are
   not using the GNU compiler, substitute the C compiler in use on your
   system.

 - Type `announce port < message_file` and the port announcer will take
   over from there.
