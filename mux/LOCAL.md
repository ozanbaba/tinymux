---
author: Brazil
date: July 2020
title: LOCAL
---

# Overall

The `local.cpp` file contains several functions to enable the extension of the
server source code (AKA hardcode).  These functions provide an interface to
reduce the need to implement changes within the server code itself.

Developers should be familiar with the TinyMUX source prior to utilizing the
local API.  Utilizing the local extensions is just like modifying the server
source code and can cause instability if done improperly.


# API Description

Function: `local_startup()`

`local_startup` is called after all normal initialization is complete just
before the game begins to process input and output for connected players.
This function can be used to load databases, perform initialization or other
tasks that need to occur prior to the game being active.  The function
accepts no arguments and returns no value.

Function: `local_dump_database(int dump_type)`

`local_dump_database()` is called prior to the game performing a database
dump.  It is invoked via the periodic dump timer, `@restart`, `@shutdown`,
and `@dump`.

`dump_type` is one of the five dump type definitions declared in `externs.h`.
It can be used to perform different procedures depending on what type
of dump the game is performing.

Currently, the following types are available:

|                  |                                                     |
|------------------|-----------------------------------------------------|
| `DUMP_I_NORMAL`  | perform a normal dump                               |
| `DUMP_I_PANIC`   | write a panic database (game output to crashdb)     |
| `DUMP_I_RESTART` | game writes database to the inputdb                 |
| `DUMP_I_FLAT`    | game is unloading the db to a flatfile              |
| `DUMP_I_SIGNAL`  | game is unloading the db to a flatfile via a signal |

Function: `local_shutdown()`

`local_shutdown()` is called after the game database has been saved but the
logfiles remain open.  It can be utilized to perform any cleanup prior
to the process being terminated.  Database saves need not occur within
local_shutdown since `local_dump_database()` is called prior to it.

Function: `local_dbck()`

`local_dbck()` is invoked during all database consistency checks (periodic
or manual `@dbck`).

Function: `local_connect(dbref player, int isnew, int num)`

`local_connect()` is called when a player connects to the game.  The
argument isnew is set to 1 if the player object was just created,
or 0 if it was pre-existing.  The number of current connections
is passed via the num argument where any value >1 indicates multiple
connections.

Function: `local_disconnect(dbref player, int num)`

`local_disconnect()` is called when a player disconnects from the game.
Arguments contain the dbref and the number of connections at the time
of the disconnect.  A value of >1 for num indicates the player still
has an active connection.

Function: `local_data_create(dbref object)`

`local_data_create()` is called when any object is created.  Note: this
function catches player creation via `@pcreate` as well as creations
at the connection screen so is a better location for player based
initialization if required.

Function: `local_data_clone(dbref clone, dbref source)`

Cloning is a unique creation process so it has a stand-alone handler.
Called after the source object has been cloned and set with all
relevant information.

Function: `local_data_free(dbref object)`

`local_data_free()` is called after the target object has been cleared of
all information but just prior to it being truly destroyed.  This occurs
at the point of true destruction prior to being available to the freelist
not when it is just set `GOING`.

# Periodic Processing

The TinyMUX local implementation does not provide a function implementing the
functionality of PennMUSH's `local_timer()` one second cycle callback.  Instead,
developers should make use of the TinyMUX scheduler to instantiate periodic or
one-shot timers.  The scheduler accepts a function pointer to execute at the
designated timeframe.  Regularly occuring timers must re-schedule themselves
during the callback execution.

The function format required by the sceduler is:

```C++
void funname(void *, int)
```

The function accepts a void pointer to user data and an integer value.
Most timer functions do not make use of these parameters.

To instantiate a timer, first declare the function to be called.  This
example shows a timer that happens only once.

```C++
void myTimer(void* pUnused, int iUnused)
{
    // ... insert timer code here
}
```

Once the function is available, it must be scheduled.  The following
example is shown using `local_startup`.

```C++
const CLinearTimeDelta period_15s   = 15*FACTOR_100NS_PER_SECOND;

void local_startup(void)
{
    CLinearTimeAbsolute ltaNow;   // declare a time object
    ltaNow.GetUTC();              // set the value to UTC

    // Schedule myTimer to occur at 15 seconds in the future.
    // The final two args to DeferTask are the user data values.
    //
    scheduler.DeferTask(ltaNow+period_15s, PRIORITY_SYSTEM,
        myTimer, 0, 0);
}
```

If the timer should occur on a periodic basis, it must reschedule itself.

```C++
void myPeriodicTimer(void *pUnused, int iUnused)
{
    // ... perform processing
    //

    // Reschedule this timer.
    //
    CLinearTimeAbsolute ltaNow;   // declare a time object
    ltaNow.GetUTC();              // set the value to UTC
    scheduler.DeferTask(ltaNow+period_15s, PRIORITY_SYSTEM,
        myTimer, 0, 0);
}
```

Individuals using scheduled tasks should examine the `CScheduler` and
`CLinearTimeAbsolute` classes for more details on their use.
