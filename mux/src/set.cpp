/*! \file set.cpp
 * \brief Commands which modify objects.
 *
 * $Id$
 *
 * These functions primarily implement commands like \@set and
 * \@name that modify attributes or basic properties of an object,
 * but this file also includes \@trigger and the use command.
 */

#include "copyright.h"
#include "autoconf.h"
#include "config.h"
#include "externs.h"

#include "attrs.h"
#include "command.h"
#include "powers.h"

void set_modified(dbref thing)
{
    CLinearTimeAbsolute ltaNow;
    ltaNow.GetLocal();
    atr_add_raw(thing, A_MODIFIED, ltaNow.ReturnDateString(7));
}

dbref match_controlled_handler(dbref executor, const UTF8 *name, bool bQuiet)
{
    dbref mat;
    init_match(executor, name, NOTYPE);
    match_everything(MAT_EXIT_PARENTS);
    if (bQuiet)
    {
        mat = match_result();
    }
    else
    {
        mat = noisy_match_result();
    }
    if (!Good_obj(mat))
    {
        return mat;
    }

    if (Controls(executor, mat))
    {
        return mat;
    }
    if (!bQuiet)
    {
        notify_quiet(executor, NOPERM_MESSAGE);
    }
    return NOTHING;
}

void do_chzone
(
    dbref executor,
    dbref caller,
    dbref enactor,
    int   key,
    int   nargs,
    UTF8 *name,
    UTF8 *newobj
)
{
    UNUSED_PARAMETER(caller);
    UNUSED_PARAMETER(enactor);
    UNUSED_PARAMETER(key);
    UNUSED_PARAMETER(nargs);

    if (!mudconf.have_zones)
    {
        notify(executor, T("Zones disabled."));
        return;
    }
    init_match(executor, name, NOTYPE);
    match_everything(0);
    dbref thing = noisy_match_result();
    if (thing == NOTHING)
    {
        return;
    }

    dbref zone;
    if (  newobj[0] == '\0'
       || !mux_stricmp(newobj, T("none")))
    {
        zone = NOTHING;
    }
    else
    {
        init_match(executor, newobj, NOTYPE);
        match_everything(0);
        zone = noisy_match_result();
        if (zone == NOTHING)
        {
            return;
        }
        if (  !isThing(zone)
           && !isRoom(zone))
        {
            notify(executor, T("Invalid zone object type."));
            return;
        }
    }

    if (  !Wizard(executor)
       && !Controls(executor, thing)
       && !check_zone_handler(executor, thing, true)
       && db[executor].owner != db[thing].owner)
    {
        notify(executor, T("You don't have the power to shift reality."));
        return;
    }

    // A player may change an object's zone to NOTHING or to an object he owns.
    //
    if (  zone != NOTHING
       && !Wizard(executor)
       && !Controls(executor, zone)
       && db[executor].owner != db[zone].owner)
    {
        notify(executor, T("You cannot move that object to that zone."));
        return;
    }

    // Only rooms may be zoned to other rooms.
    //
    if (  zone != NOTHING
       && isRoom(zone)
       && !isRoom(thing))
    {
        notify(executor, T("Only rooms may be zoned to other rooms."));
        return;
    }

    // Everything is okay, do the change.
    //
    db[thing].zone = zone;
    if (!isPlayer(thing))
    {
        // If the object is a player, resetting these flags is rather
        // inconvenient -- although this may pose a bit of a security risk. Be
        // careful when @chzone'ing wizard or royal players.
        //
        SetClearFlags(thing, mudconf.stripped_flags.word, NULL);

        // Wipe out all powers.
        //
        Powers(thing) = 0;
        Powers2(thing) = 0;
    }
    notify(executor, T("Zone changed."));
}

void do_name
(
    dbref executor,
    dbref caller,
    dbref enactor,
    int   key,
    int   nargs,
    UTF8 *name,
    UTF8 *newname
)
{
    UNUSED_PARAMETER(caller);
    UNUSED_PARAMETER(enactor);
    UNUSED_PARAMETER(key);

    dbref thing = match_controlled(executor, name);
    if (thing == NOTHING)
    {
        return;
    }

    // Check for bad name.
    //
    if (  nargs < 2
       || newname[0] == '\0')
    {
        notify_quiet(executor, T("Give it what new name?"));
        return;
    }

    // Check for renaming a player.
    //
    if (isPlayer(thing))
    {
        UTF8 *buff = trim_spaces(newname);
        if (  !ValidatePlayerName(buff)
           || !badname_check(buff))
        {
            notify_quiet(executor, T("You can't use that name."));
            free_lbuf(buff);
            return;
        }
        else if (  string_compare(buff, Name(thing))
                && lookup_player(NOTHING, buff, false) != NOTHING)
        {
            // string_compare allows changing foo to Foo, etc.
            //
            notify_quiet(executor, T("That name is already in use."));
            free_lbuf(buff);
            return;
        }

        // Everything ok, notify.
        //
        STARTLOG(LOG_SECURITY, "SEC", "CNAME");
        log_name(thing),
        log_text(T(" renamed to "));
        log_text(buff);
        ENDLOG;
        if (Suspect(thing))
        {
            raw_broadcast(WIZARD, "[Suspect] %s renamed to %s", Name(thing), buff);
        }
        delete_player_name(thing, Name(thing));
        s_Name(thing, buff);
        set_modified(thing);
        add_player_name(thing, Name(thing));
        if (!Quiet(executor) && !Quiet(thing))
        {
            notify_quiet(executor, T("Name set."));
        }
        free_lbuf(buff);
        return;
    }
    else
    {
        size_t nValidName;
        bool bValid;
        UTF8 *pValidName;

        if (isExit(thing))
        {
            pValidName = MakeCanonicalExitName(newname, &nValidName, &bValid);
        }
        else
        {
            pValidName = MakeCanonicalObjectName(newname, &nValidName, &bValid);
        }

        if (!bValid)
        {
            notify_quiet(executor, T("That is not a reasonable name."));
            return;
        }

        // Everything ok, change the name.
        //
        s_Name(thing, pValidName);
        set_modified(thing);
        if (!Quiet(executor) && !Quiet(thing))
        {
            notify_quiet(executor, T("Name set."));
        }
    }
}

/*
 * ---------------------------------------------------------------------------
 * * do_alias: Make an alias for a player or object.
 */

void do_alias
(
    dbref executor,
    dbref caller,
    dbref enactor,
    int   key,
    int   nargs,
    UTF8 *name,
    UTF8 *alias
)
{
    UNUSED_PARAMETER(caller);
    UNUSED_PARAMETER(enactor);
    UNUSED_PARAMETER(key);
    UNUSED_PARAMETER(nargs);

    dbref thing = match_controlled(executor, name);
    if (thing == NOTHING)
    {
        return;
    }

    // Check for renaming a player.
    //
    dbref aowner;
    int aflags;
    ATTR *ap = atr_num(A_ALIAS);
    if (isPlayer(thing))
    {
        // Fetch the old alias.
        //
        UTF8 *oldalias = atr_pget(thing, A_ALIAS, &aowner, &aflags);
        UTF8 *trimalias = trim_spaces(alias);

        if (!Controls(executor, thing))
        {
            // Make sure we have rights to do it. We can't do the
            // normal Set_attr check because ALIAS is only
            // writable by GOD and we want to keep people from
            // doing &ALIAS and bypassing the player name checks.
            //
            notify_quiet(executor, NOPERM_MESSAGE);
        }
        else if (!*trimalias)
        {
            // New alias is null, just clear it.
            //
            delete_player_name(thing, oldalias);
            atr_clr(thing, A_ALIAS);
            if (!Quiet(executor))
            {
                notify_quiet(executor, T("Alias removed."));
            }
        }
        else if (lookup_player(NOTHING, trimalias, false) != NOTHING)
        {
            // Make sure new alias isn't already in use.
            //
            notify_quiet(executor, T("That name is already in use."));
        }
        else if (  !(badname_check(trimalias)
                && ValidatePlayerName(trimalias)))
        {
            notify_quiet(executor, T("That's a silly name for a player!"));
        }
        else
        {
            // Remove the old name and add the new name.
            //
            delete_player_name(thing, oldalias);
            atr_add(thing, A_ALIAS, trimalias, Owner(executor), aflags);
            if (add_player_name(thing, trimalias))
            {
                if (!Quiet(executor))
                {
                    notify_quiet(executor, T("Alias set."));
                }
            }
            else
            {
                notify_quiet(executor,
                    T("That name is already in use or is illegal, alias cleared."));
                atr_clr(thing, A_ALIAS);
            }
        }
        free_lbuf(trimalias);
        free_lbuf(oldalias);
    }
    else
    {
        atr_pget_info(thing, A_ALIAS, &aowner, &aflags);

        // Make sure we have rights to do it.
        //
        if (!bCanSetAttr(executor, thing, ap))
        {
            notify_quiet(executor, NOPERM_MESSAGE);
        }
        else
        {
            atr_add(thing, A_ALIAS, alias, Owner(executor), aflags);
            if (!Quiet(executor))
            {
                notify_quiet(executor, T("Set."));
            }
        }
    }
}

// ---------------------------------------------------------------------------
// do_forwardlist: Set a forwardlist.
// ---------------------------------------------------------------------------
void do_forwardlist
(
    dbref executor,
    dbref caller,
    dbref enactor,
    int   key,
    int   nargs,
    UTF8 *target,
    UTF8 *newlist
)
{
    UNUSED_PARAMETER(caller);
    UNUSED_PARAMETER(enactor);
    UNUSED_PARAMETER(key);
    UNUSED_PARAMETER(nargs);

    dbref thing = match_controlled(executor, target);
    if (thing == NOTHING)
    {
        return;
    }
    dbref aowner, aflags;
    atr_pget_info(thing, A_FORWARDLIST, &aowner, &aflags);

    if (!Controls(executor, thing))
    {
        notify_quiet(executor, NOPERM_MESSAGE);
        return;
    }
    else if (!*newlist)
    {
        // New forwardlist is null, just clear it.
        //
        atr_clr(thing, A_FORWARDLIST);
        set_modified(thing);
        if (!Quiet(executor))
        {
            notify_quiet(executor, T("Forwardlist removed."));
        }
    }
    else if (!fwdlist_ck(executor, thing, A_FORWARDLIST, newlist))
    {
        notify_quiet(executor, T("Invalid forwardlist."));
        return;
    }
    else
    {
        atr_add(thing, A_FORWARDLIST, newlist, Owner(executor), aflags);
        if (!Quiet(executor))
        {
            notify_quiet(executor, T("Set."));
        }
    }
}

/*
 * ---------------------------------------------------------------------------
 * * do_lock: Set a lock on an object or attribute.
 */

void do_lock
(
    dbref executor,
    dbref caller,
    dbref enactor,
    int   key,
    int   nargs,
    UTF8 *name,
    UTF8 *keytext
)
{
    UNUSED_PARAMETER(caller);
    UNUSED_PARAMETER(enactor);
    UNUSED_PARAMETER(nargs);

    dbref thing;
    ATTR *ap;

    if (  parse_attrib(executor, name, &thing, &ap)
       && ap)
    {
        dbref aowner;
        int aflags;
        if (!atr_get_info(thing, ap->number, &aowner, &aflags))
        {
            notify_quiet(executor, T("Attribute not present on object."));
            return;
        }

        if (bCanLockAttr(executor, thing, ap))
        {
            aflags |= AF_LOCK;
            atr_set_flags(thing, ap->number, aflags);
            if (  !Quiet(executor)
               && !Quiet(thing))
            {
                notify_quiet(executor, T("Attribute locked."));
            }
        }
        else
        {
            notify_quiet(executor, NOPERM_MESSAGE);
        }
        return;
    }
    init_match(executor, name, NOTYPE);
    match_everything(MAT_EXIT_PARENTS);
    thing = match_result();

    switch (thing)
    {
    case NOTHING:
        notify_quiet(executor, T("I don't see what you want to lock!"));
        return;

    case AMBIGUOUS:
        notify_quiet(executor, T("I don't know which one you want to lock!"));
        return;

    default:
        if (!Controls(executor, thing))
        {
            notify_quiet(executor, T("You can't lock that!"));
            return;
        }
    }

    static UTF8 pRestrictedKeyText[LBUF_SIZE];
    StripTabsAndTruncate(keytext, pRestrictedKeyText, LBUF_SIZE-1, LBUF_SIZE-1);
    struct boolexp *okey = parse_boolexp(executor, pRestrictedKeyText, false);
    if (okey == TRUE_BOOLEXP)
    {
        notify_quiet(executor, T("I don't understand that key."));
    }
    else
    {
        // Everything ok, do it.
        //
        if (!key)
        {
            key = A_LOCK;
        }
        atr_add_raw(thing, key, unparse_boolexp_quiet(executor, okey));
        if (  !Quiet(executor)
           && !Quiet(thing))
        {
            notify_quiet(executor, T("Locked."));
        }
    }
    free_boolexp(okey);
}

/*
 * ---------------------------------------------------------------------------
 * * Remove a lock from an object of attribute.
 */

void do_unlock(dbref executor, dbref caller, dbref enactor, int eval, int key, UTF8 *name)
{
    UNUSED_PARAMETER(caller);
    UNUSED_PARAMETER(enactor);
    UNUSED_PARAMETER(eval);

    dbref thing;
    ATTR *ap;

    if (  parse_attrib(executor, name, &thing, &ap)
       && ap)
    {
        // We have been asked to unlock an attribute.
        //
        dbref aowner;
        int aflags;
        if (!atr_get_info(thing, ap->number, &aowner, &aflags))
        {
            notify_quiet(executor, T("Attribute not present on object."));
            return;
        }

        if (bCanLockAttr(executor, thing, ap))
        {
            aflags &= ~AF_LOCK;
            atr_set_flags(thing, ap->number, aflags);
            if (  !Quiet(executor)
               && !Quiet(thing))
            {
                notify_quiet(executor, T("Attribute unlocked."));
            }
        }
        else
        {
            notify_quiet(executor, NOPERM_MESSAGE);
        }
        return;
    }

    // We have been asked to change the ownership of an object.
    //
    if (!key)
    {
        key = A_LOCK;
    }
    thing = match_controlled(executor, name);
    if (thing != NOTHING)
    {
        atr_clr(thing, key);
        set_modified(thing);
        if (!Quiet(executor) && !Quiet(thing))
        {
            notify_quiet(executor, T("Unlocked."));
        }
    }
}

/*
 * ---------------------------------------------------------------------------
 * * do_unlink: Unlink an exit from its destination or remove a dropto.
 */

void do_unlink(dbref executor, dbref caller, dbref enactor, int eval, int key, UTF8 *name)
{
    UNUSED_PARAMETER(caller);
    UNUSED_PARAMETER(enactor);
    UNUSED_PARAMETER(key);
    UNUSED_PARAMETER(eval);

    dbref exit;

    init_match(executor, name, TYPE_EXIT);
    match_everything(0);
    exit = match_result();

    switch (exit)
    {
    case NOTHING:

        notify_quiet(executor, T("Unlink what?"));
        break;

    case AMBIGUOUS:

        notify_quiet(executor, T("I don't know which one you mean!"));
        break;

    default:

        if (!Controls(executor, exit))
        {
            notify_quiet(executor, NOPERM_MESSAGE);
        }
        else
        {
            switch (Typeof(exit))
            {
            case TYPE_EXIT:

                s_Location(exit, NOTHING);
                if (!Quiet(executor))
                {
                    notify_quiet(executor, T("Unlinked."));
                }
                giveto(Owner(exit), mudconf.linkcost);
                break;

            case TYPE_ROOM:

                s_Dropto(exit, NOTHING);
                if (!Quiet(executor))
                {
                    notify_quiet(executor, T("Dropto removed."));
                }
                break;

            default:

                notify_quiet(executor, T("You can't unlink that!"));
                break;
            }
        }
    }
}

void TranslateFlags_Clone
(
    FLAG     aClearFlags[3],
    dbref    executor,
    int      key
)
{
    if (key & CLONE_NOSTRIP)
    {
        if (God(executor))
        {
            // #1 using /nostrip causes nothing to be stripped.
            //
            aClearFlags[FLAG_WORD1] = 0;
        }
        else
        {
            // With #1 powers, /nostrip still strips the WIZARD bit.
            //
            aClearFlags[FLAG_WORD1] = WIZARD;
        }
        aClearFlags[FLAG_WORD2] = 0;
        aClearFlags[FLAG_WORD3] = 0;
    }
    else
    {
        if (  (key & CLONE_INHERIT)
           && Inherits(executor))
        {
            // The /inherit switch specifically allows INHERIT powers through.
            //
            aClearFlags[FLAG_WORD1] = (WIZARD | mudconf.stripped_flags.word[FLAG_WORD1]) & ~(INHERIT);
        }
        else
        {
            // Normally WIZARD and the other stripped_flags are removed.
            //
            aClearFlags[FLAG_WORD1] = WIZARD | mudconf.stripped_flags.word[FLAG_WORD1];
        }
        aClearFlags[FLAG_WORD2] = mudconf.stripped_flags.word[FLAG_WORD2];
        aClearFlags[FLAG_WORD3] = mudconf.stripped_flags.word[FLAG_WORD3];
    }
}

void TranslateFlags_Chown
(
    FLAG     aClearFlags[3],
    FLAG     aSetFlags[3],
    bool    *bClearPowers,
    dbref    executor,
    int      key
)
{
    if (key & CHOWN_NOSTRIP)
    {
        if (God(executor))
        {
            // #1 using /nostrip only clears CHOWN_OK and sets HALT.
            //
            aClearFlags[FLAG_WORD1] = CHOWN_OK;
            *bClearPowers = false;
        }
        else
        {
            // With /nostrip, CHOWN_OK and WIZARD are cleared, HALT is
            // set, and powers are cleared.
            //
            aClearFlags[FLAG_WORD1] = CHOWN_OK | WIZARD;
            *bClearPowers = true;
        }
        aClearFlags[FLAG_WORD2] = 0;
        aClearFlags[FLAG_WORD3] = 0;
    }
    else
    {
        // Without /nostrip, CHOWN_OK, WIZARD, and stripped_flags are
        // cleared, HALT is set, powers are cleared.
        //
        aClearFlags[FLAG_WORD1] = CHOWN_OK | WIZARD
                                | mudconf.stripped_flags.word[FLAG_WORD1];
        aClearFlags[FLAG_WORD2] = mudconf.stripped_flags.word[FLAG_WORD2];
        aClearFlags[FLAG_WORD3] = mudconf.stripped_flags.word[FLAG_WORD3];
        *bClearPowers = true;
    }
    aSetFlags[FLAG_WORD1] = HALT;
    aSetFlags[FLAG_WORD2] = 0;
    aSetFlags[FLAG_WORD3] = 0;
}

void SetClearFlags
(
    dbref thing,
    FLAG aClearFlags[3],
    FLAG aSetFlags[3]
)
{
    int j;
    for (j = FLAG_WORD1; j <= FLAG_WORD3; j++)
    {
        if (NULL != aClearFlags)
        {
            db[thing].fs.word[j] &= ~aClearFlags[j];
        }

        if (NULL != aSetFlags)
        {
            db[thing].fs.word[j] |= aSetFlags[j];
        }
    }
}

/*
 * ---------------------------------------------------------------------------
 * * do_chown: Change ownership of an object or attribute.
 */

void do_chown
(
    dbref executor,
    dbref caller,
    dbref enactor,
    int   key,
    int   nargs,
    UTF8 *name,
    UTF8 *newown
)
{
    UNUSED_PARAMETER(caller);
    UNUSED_PARAMETER(enactor);
    UNUSED_PARAMETER(nargs);

    dbref nOwnerOrig, nOwnerNew, thing;
    bool bDoit;
    ATTR *ap;

    if (  parse_attrib(executor, name, &thing, &ap)
       && ap
       && See_attr(executor, thing, ap))
    {
        // An attribute was given, so we worry about changing the owner of the
        // attribute.
        //
        nOwnerOrig = Owner(thing);
        if (!*newown)
        {
            nOwnerNew = nOwnerOrig;
        }
        else if (!string_compare(newown, T("me")))
        {
            nOwnerNew = Owner(executor);
        }
        else
        {
            nOwnerNew = lookup_player(executor, newown, true);
        }

        // You may chown an attr to yourself if you own the object and the attr
        // is not locked. You may chown an attr to the owner of the object if
        // you own the attribute. To do anything else you must be a wizard.
        // Only #1 can chown attributes on #1.
        //
        dbref aowner;
        int   aflags;
        if (!atr_get_info(thing, ap->number, &aowner, &aflags))
        {
            notify_quiet(executor, T("Attribute not present on object."));
            return;
        }
        bDoit = false;
        if (nOwnerNew == NOTHING)
        {
            notify_quiet(executor, T("I couldn't find that player."));
        }
        else if (  God(thing)
                && !God(executor))
        {
            notify_quiet(executor, NOPERM_MESSAGE);
        }
        else if (Wizard(executor))
        {
            bDoit = true;
        }
        else if (nOwnerNew == Owner(executor))
        {
            // Chown to me: only if I own the obj and !locked
            //
            if (  !Controls(executor, thing)
               || (aflags & AF_LOCK))
            {
                notify_quiet(executor, NOPERM_MESSAGE);
            }
            else
            {
                bDoit = true;
            }
        }
        else if (nOwnerNew == nOwnerOrig)
        {
            // chown to obj owner: only if I own attr and !locked
            //
            if (  Owner(executor) != aowner
               || (aflags & AF_LOCK))
            {
                notify_quiet(executor, NOPERM_MESSAGE);
            }
            else
            {
                bDoit = true;
            }
        }
        else
        {
            notify_quiet(executor, NOPERM_MESSAGE);
        }

        if (!bDoit)
        {
            return;
        }

        if (!bCanSetAttr(executor, executor, ap))
        {
            notify_quiet(executor, NOPERM_MESSAGE);
            return;
        }
        UTF8 *buff = atr_get("do_chown.887", thing, ap->number, &aowner, &aflags);
        atr_add(thing, ap->number, buff, nOwnerNew, aflags);
        free_lbuf(buff);
        if (!Quiet(executor))
        {
            notify_quiet(executor, T("Attribute owner changed."));
        }
        return;
    }

    // An attribute was not specified, so we are being asked to change the
    // owner of the object.
    //
    init_match(executor, name, TYPE_THING);
    match_possession();
    match_here();
    match_exit();
    match_me();
    if (Chown_Any(executor))
    {
        match_player();
        match_absolute();
    }
    switch (thing = match_result())
    {
    case NOTHING:

        notify_quiet(executor, T("You don't have that!"));
        return;

    case AMBIGUOUS:

        notify_quiet(executor, T("I don't know which you mean!"));
        return;
    }
    nOwnerOrig = Owner(thing);

    if (!*newown || !(string_compare(newown, T("me"))))
    {
        nOwnerNew = Owner(executor);
    }
    else
    {
        nOwnerNew = lookup_player(executor, newown, true);
    }

    int cost = 1, quota = 1;

    switch (Typeof(thing))
    {
    case TYPE_ROOM:

        cost = mudconf.digcost;
        quota = mudconf.room_quota;
        break;

    case TYPE_THING:

        cost = OBJECT_DEPOSIT(Pennies(thing));
        quota = mudconf.thing_quota;
        break;

    case TYPE_EXIT:

        cost = mudconf.opencost;
        quota = mudconf.exit_quota;
        break;

    case TYPE_PLAYER:

        cost = mudconf.robotcost;
        quota = mudconf.player_quota;
        break;
    }

    bool bPlayerControlsThing = Controls(executor, thing);
    if (  isGarbage(thing)
       && bPlayerControlsThing)
    {
        notify_quiet(executor, T("You shouldn't be rummaging through the garbage."));
    }
    else if (nOwnerNew == NOTHING)
    {
        notify_quiet(executor, T("I couldn't find that player."));
    }
    else if (  isPlayer(thing)
            && !God(executor))
    {
        notify_quiet(executor, T("Players always own themselves."));
    }
    else if (  (  !bPlayerControlsThing
               && !Chown_Any(executor)
               && !Chown_ok(thing))
            || (  isThing(thing)
               && Location(thing) != executor
               && !Chown_Any(executor))
            || !Controls(executor, nOwnerNew)
            || God(thing))
    {
        notify_quiet(executor, NOPERM_MESSAGE);
    }
    else if (canpayfees(executor, nOwnerNew, cost, quota))
    {
        giveto(nOwnerOrig, cost);
        if (mudconf.quotas)
        {
            add_quota(nOwnerOrig, quota);
        }

        if (!God(executor))
        {
            nOwnerNew = Owner(nOwnerNew);
        }

        s_Owner(thing, nOwnerNew);
        atr_chown(thing);

        FLAGSET clearflags;
        FLAGSET setflags;
        bool    bClearPowers;

        TranslateFlags_Chown(clearflags.word, setflags.word, &bClearPowers, executor, key);

        SetClearFlags(thing, clearflags.word, setflags.word);
        if (bClearPowers)
        {
            s_Powers(thing, 0);
            s_Powers2(thing, 0);
        }

        // Always halt the queue.
        //
        halt_que(NOTHING, thing);

        // Warn if ROYALTY is left set.
        //
        if (ROYALTY & db[thing].fs.word[FLAG_WORD1])
        {
            notify_quiet(executor,
                tprintf("Warning: @chown/nostrip on %s(#%d) leaves ROYALTY priviledge intact.",
                Moniker(thing), thing));
        }

        // Warn if INHERIT is left set.
        //
        if (INHERIT & db[thing].fs.word[FLAG_WORD1])
        {
            notify_quiet(executor,
                tprintf("Warning: @chown/nostrip on %s(#%d) leaves INHERIT priviledge intact.",
                Moniker(thing), thing));
        }

        if (!Quiet(executor))
        {
            UTF8 *buff = alloc_lbuf("do_chown.notify");
            UTF8 *bp = buff;

            safe_tprintf_str(buff, &bp, "Owner of %s(#%d) changed from ", Moniker(thing), thing);
            safe_tprintf_str(buff, &bp, "%s(#%d) to ", Moniker(nOwnerOrig), nOwnerOrig);
            safe_tprintf_str(buff, &bp, "%s(#%d).", Moniker(nOwnerNew), nOwnerNew);
            *bp = '\0';
            notify_quiet(executor, buff);
            free_lbuf(buff);
        }
    }
}

/*
 * ---------------------------------------------------------------------------
 * * do_set: Set flags or attributes on objects, or flags on attributes.
 */

static void set_attr_internal(dbref player, dbref thing, int attrnum, UTF8 *attrtext, int key)
{
    dbref aowner;
    int aflags;
    ATTR *pattr = atr_num(attrnum);
    atr_pget_info(thing, attrnum, &aowner, &aflags);
    if (  pattr
       && bCanSetAttr(player, thing, pattr))
    {
        bool could_hear = Hearer(thing);
        atr_add(thing, attrnum, attrtext, Owner(player), aflags);
        handle_ears(thing, could_hear, Hearer(thing));
        if (  !(key & SET_QUIET)
           && !Quiet(player)
           && !Quiet(thing))
        {
            notify_quiet(player, T("Set."));
        }
    }
    else
    {
        notify_quiet(player, NOPERM_MESSAGE);
    }
}

void do_set
(
    dbref executor,
    dbref caller,
    dbref enactor,
    int   key,
    int   nargs,
    UTF8 *name,
    UTF8 *flagname
)
{
    UNUSED_PARAMETER(caller);
    UNUSED_PARAMETER(enactor);
    UNUSED_PARAMETER(nargs);

    dbref thing, aowner;
    int aflags;
    ATTR *pattr;

    // See if we have the <obj>/<attr> form, which is how you set
    // attribute flags.
    //
    if (parse_attrib(executor, name, &thing, &pattr))
    {
        if (  pattr
           && See_attr(executor, thing, pattr))
        {
            // You must specify a flag name.
            //
            if (  !flagname
               || flagname[0] == '\0')
            {
                notify_quiet(executor, T("I don't know what you want to set!"));
                return;
            }

            // Check for clearing.
            //
            bool clear = false;
            if (flagname[0] == NOT_TOKEN)
            {
                flagname++;
                clear = true;
            }

            // Make sure player specified a valid attribute flag.
            //
            int flagvalue;
            if (!search_nametab(executor, indiv_attraccess_nametab, flagname, &flagvalue))
            {
                notify_quiet(executor, T("You can't set that!"));
                return;
            }

            // Make sure the object has the attribute present.
            //
            if (!atr_get_info(thing, pattr->number, &aowner, &aflags))
            {
                notify_quiet(executor, T("Attribute not present on object."));
                return;
            }

            // Make sure we can write to the attribute.
            //
            if (!bCanSetAttr(executor, thing, pattr))
            {
                notify_quiet(executor, NOPERM_MESSAGE);
                return;
            }

            // Go do it.
            //
            if (clear)
            {
                aflags &= ~flagvalue;
            }
            else
            {
                aflags |= flagvalue;
            }
            bool could_hear = Hearer(thing);

            atr_set_flags(thing, pattr->number, aflags);

            // Tell the player about it.
            //
            handle_ears(thing, could_hear, Hearer(thing));

            if (  !(key & SET_QUIET)
               && !Quiet(executor)
               && !Quiet(thing))
            {
                if (clear)
                {
                    notify_quiet(executor, T("Cleared."));
                }
                else
                {
                    notify_quiet(executor, T("Set."));
                }
            }
            return;
        }
    }

    // Find thing.
    //
    thing = match_controlled(executor, name);
    if (!Good_obj(thing))
    {
        return;
    }

    // Check for attribute set first.
    //
    UTF8 *p;
    for (p = flagname; *p && (*p != ':'); p++)
    {
        ; // Nothing.
    }

    if (*p)
    {
        *p++ = 0;
        int atr = mkattr(executor, flagname);
        if (atr <= 0)
        {
            notify_quiet(executor, T("Couldn't create attribute."));
            return;
        }
        pattr = atr_num(atr);
        if (!pattr)
        {
            notify_quiet(executor, NOPERM_MESSAGE);
            return;
        }
        if (!bCanSetAttr(executor, thing, pattr))
        {
            notify_quiet(executor, NOPERM_MESSAGE);
            return;
        }
        UTF8 *buff = alloc_lbuf("do_set");

        // Check for _
        //
        if (*p == '_')
        {
            ATTR *pattr2;
            dbref thing2;

            mux_strncpy(buff, p + 1, LBUF_SIZE-1);
            if (!( parse_attrib(executor, p + 1, &thing2, &pattr2)
                && pattr2))
            {
                notify_quiet(executor, T("No match."));
                free_lbuf(buff);
                return;
            }
            p = buff;
            atr_pget_str(buff, thing2, pattr2->number, &aowner, &aflags);

            if (!See_attr(executor, thing2, pattr2))
            {
                notify_quiet(executor, NOPERM_MESSAGE);
                free_lbuf(buff);
                return;
            }
        }

        // Go set it.
        //
        set_attr_internal(executor, thing, atr, p, key);
        free_lbuf(buff);
        return;
    }

    // Set or clear a flag.
    //
    flag_set(thing, executor, flagname, key);
}

void do_power
(
    dbref executor,
    dbref caller,
    dbref enactor,
    int   key,
    int   nargs,
    UTF8 *name,
    UTF8 *flag
)
{
    UNUSED_PARAMETER(caller);
    UNUSED_PARAMETER(enactor);
    UNUSED_PARAMETER(nargs);

    if (  !flag
       || !*flag)
    {
        notify_quiet(executor, T("I don't know what you want to set!"));
        return;
    }

    // Find thing.
    //
    dbref thing = match_controlled(executor, name);
    if (thing == NOTHING)
    {
        return;
    }
    power_set(thing, executor, flag, key);
}

void do_setattr
(
    dbref executor,
    dbref caller,
    dbref enactor,
    int   attrnum,
    int   nargs,
    UTF8 *name,
    UTF8 *attrtext
)
{
    UNUSED_PARAMETER(caller);
    UNUSED_PARAMETER(enactor);
    UNUSED_PARAMETER(nargs);

    init_match(executor, name, NOTYPE);
    match_everything(MAT_EXIT_PARENTS);
    dbref thing = noisy_match_result();

    if (!Good_obj(thing))
    {
        return;
    }
    set_attr_internal(executor, thing, attrnum, attrtext, 0);
}

void do_cpattr(dbref executor, dbref caller, dbref enactor, int eval, int key,
               UTF8 *oldpair, UTF8 *newpair[], int nargs)
{
    UNUSED_PARAMETER(eval);
    UNUSED_PARAMETER(key);

    int i;
    UTF8 *oldthing, *oldattr, *newthing, *newattr;

    if (  !*oldpair
       || !**newpair
       || !oldpair
       || !*newpair
       || nargs < 1)
    {
        return;
    }

    oldattr = oldpair;
    oldthing = parse_to(&oldattr, '/', 1);

    for (i = 0; i < nargs; i++)
    {
        newattr = newpair[i];
        newthing = parse_to(&newattr, '/', 1);

        if (!oldattr)
        {
            if (!newattr)
            {
                do_set(executor, caller, enactor, 0, 2, newthing,
                    tprintf("%s:_%s/%s", oldthing, "me", oldthing));
            }
            else
            {
                do_set(executor, caller, enactor, 0, 2, newthing,
                    tprintf("%s:_%s/%s", newattr, "me", oldthing));
            }
        }
        else
        {
            if (!newattr)
            {
                do_set(executor, caller, enactor, 0, 2, newthing,
                    tprintf("%s:_%s/%s", oldattr, oldthing, oldattr));
            }
            else
            {
                do_set(executor, caller, enactor, 0, 2, newthing,
                    tprintf("%s:_%s/%s", newattr, oldthing, oldattr));
            }
        }
    }
}


void do_mvattr(dbref executor, dbref caller, dbref enactor, int eval, int key,
               UTF8 *what, UTF8 *args[], int nargs)
{
    UNUSED_PARAMETER(eval);
    UNUSED_PARAMETER(caller);
    UNUSED_PARAMETER(enactor);
    UNUSED_PARAMETER(key);

    // Make sure we have something to do.
    //
    if (nargs < 2)
    {
        notify_quiet(executor, T("Nothing to do."));
        return;
    }

    // Find and make sure we control the target object.
    //
    dbref thing = match_controlled(executor, what);
    if (thing == NOTHING)
    {
        return;
    }

    // Look up the source attribute.  If it either doesn't exist or isn't
    // readable, use an empty string.
    //
    int in_anum = -1;
    UTF8 *astr = alloc_lbuf("do_mvattr");
    ATTR *in_attr = atr_str(args[0]);
    int aflags = 0;
    if (in_attr == NULL)
    {
        *astr = '\0';
    }
    else
    {
        dbref aowner;
        atr_get_str(astr, thing, in_attr->number, &aowner, &aflags);
        if (See_attr(executor, thing, in_attr))
        {
            in_anum = in_attr->number;
        }
        else
        {
            *astr = '\0';
        }
    }

    // Copy the attribute to each target in turn.
    //
    bool bCanDelete = true;
    int  nCopied = 0;
    for (int i = 1; i < nargs; i++)
    {
        int anum = mkattr(executor, args[i]);
        if (anum <= 0)
        {
            notify_quiet(executor, tprintf("%s: That's not a good name for an attribute.", args[i]));
            continue;
        }
        ATTR *out_attr = atr_num(anum);
        if (!out_attr)
        {
            notify_quiet(executor, tprintf("%s: Permission denied.", args[i]));
        }
        else if (out_attr->number == in_anum)
        {
            // It doesn't make sense to delete a source attribute if it's also
            // included as a destination.
            //
            bCanDelete = false;
        }
        else
        {
            if (!bCanSetAttr(executor, thing, out_attr))
            {
                notify_quiet(executor, tprintf("%s: Permission denied.", args[i]));
            }
            else
            {
                nCopied++;
                atr_add(thing, out_attr->number, astr, Owner(executor), aflags);
                if (!Quiet(executor))
                {
                    notify_quiet(executor, tprintf("%s: Set.", out_attr->name));
                }
            }
        }
    }

    // Remove the source attribute if we were able to copy it successfully to
    // even one destination object.
    //
    if (nCopied <= 0)
    {
        if (in_attr)
        {
            notify_quiet(executor, tprintf("%s: Not copied anywhere. Not cleared.", in_attr->name));
        }
        else
        {
            notify_quiet(executor, T("Not copied anywhere. Non-existent attribute."));
        }
    }
    else if (  in_anum > 0
            && bCanDelete)
    {
        in_attr = atr_num(in_anum);
        if (in_attr)
        {
            if (bCanSetAttr(executor, thing, in_attr))
            {
                atr_clr(thing, in_attr->number);
                if (!Quiet(executor))
                {
                    notify_quiet(executor, tprintf("%s: Cleared.", in_attr->name));
                }
            }
            else
            {
                notify_quiet(executor,
                    tprintf("%s: Could not remove old attribute.  Permission denied.",
                    in_attr->name));
            }
        }
        else
        {
            notify_quiet(executor, T("Could not remove old attribute. Non-existent attribute."));
        }
    }
    free_lbuf(astr);
}

/*
 * ---------------------------------------------------------------------------
 * * parse_attrib, parse_attrib_wild: parse <obj>/<attr> tokens.
 */

bool parse_attrib(dbref player, UTF8 *str, dbref *thing, ATTR **attr)
{
    ATTR *tattr = NULL;
    *thing = NOTHING;

    if (!str)
    {
        *attr = tattr;
        return false;
    }

    // Break apart string into obj and attr.
    //
    UTF8 *buff = alloc_lbuf("parse_attrib");
    mux_strncpy(buff, str, LBUF_SIZE-1);
    UTF8 *AttrName;
    bool retval = parse_thing_slash(player, buff, &AttrName, thing);

    // Get the named attribute from the object if we can.
    //
    if (retval)
    {
        tattr = atr_str(AttrName);
    }

    free_lbuf(buff);
    *attr = tattr;
    return retval;
}

static void find_wild_attrs(dbref player, dbref thing, UTF8 *str, bool check_exclude, bool hash_insert, bool get_locks)
{
    ATTR *pattr;
    dbref aowner;
    int ca, ok, aflags;

    // Walk the attribute list of the object.
    //
    atr_push();
    unsigned char *as;
    for (ca = atr_head(thing, &as); ca; ca = atr_next(&as))
    {
        pattr = atr_num(ca);

        // Discard bad attributes and ones we've seen before.
        //
        if (!pattr)
        {
            continue;
        }

        if (  check_exclude
           && (  (pattr->flags & AF_PRIVATE)
              || hashfindLEN(&ca, sizeof(ca), &mudstate.parent_htab)))
        {
            continue;
        }

        // If we aren't the top level remember this attr so we exclude it in
        // any parents.
        //
        atr_get_info(thing, ca, &aowner, &aflags);
        if (  check_exclude
           && (aflags & AF_PRIVATE))
        {
            continue;
        }

        if (get_locks)
        {
            ok = bCanReadAttr(player, thing, pattr, false);
        }
        else
        {
            ok = See_attr(player, thing, pattr);
        }

        mudstate.wild_invk_ctr = 0;
        if (  ok
           && quick_wild(str, pattr->name))
        {
            olist_add(ca);
            if (hash_insert)
            {
                hashaddLEN(&ca, sizeof(ca), pattr, &mudstate.parent_htab);
            }
        }
    }
    atr_pop();
}

bool parse_attrib_wild(dbref player, UTF8 *str, dbref *thing,
    bool check_parents, bool get_locks, bool df_star)
{
    if (!str)
    {
        return false;
    }

    dbref parent;
    int lev;
    bool check_exclude, hash_insert;
    UTF8 *buff = alloc_lbuf("parse_attrib_wild");
    mux_strncpy(buff, str, LBUF_SIZE-1);

    // Separate name and attr portions at the first /.
    //
    if (!parse_thing_slash(player, buff, &str, thing))
    {
        // Not in obj/attr format, return if not defaulting to.
        //
        if (!df_star)
        {
            free_lbuf(buff);
            return false;
        }

        // Look for the object, return failure if not found.
        //
        init_match(player, buff, NOTYPE);
        match_everything(MAT_EXIT_PARENTS);
        *thing = match_result();

        if (!Good_obj(*thing))
        {
            free_lbuf(buff);
            return false;
        }
        str = (UTF8 *)"*";
    }

    // Check the object (and optionally all parents) for attributes.
    //
    if (check_parents)
    {
        check_exclude = false;
        hash_insert = check_parents;
        hashflush(&mudstate.parent_htab);
        ITER_PARENTS(*thing, parent, lev)
        {
            if (!Good_obj(Parent(parent)))
            {
                hash_insert = false;
            }
            find_wild_attrs(player, parent, str, check_exclude, hash_insert, get_locks);
            check_exclude = true;
        }
    }
    else
    {
        find_wild_attrs(player, *thing, str, false, false, get_locks);
    }
    free_lbuf(buff);
    return true;
}

/*
 * ---------------------------------------------------------------------------
 * * edit_string, edit_string_ansi, do_edit: Modify attributes.
 */

void edit_string(UTF8 *src, UTF8 **dst, UTF8 *from, UTF8 *to)
{
    UTF8 *cp;

    // Do the substitution.  Idea for prefix/suffix from R'nice@TinyTIM.
    //
    if (!strcmp((char *)from, "^"))
    {
        // Prepend 'to' to string.
        //
        *dst = alloc_lbuf("edit_string.^");
        cp = *dst;
        safe_str(to, *dst, &cp);
        safe_str(src, *dst, &cp);
        *cp = '\0';
    }
    else if (!strcmp((char *)from, "$"))
    {
        // Append 'to' to string.
        //
        *dst = alloc_lbuf("edit_string.$");
        cp = *dst;
        safe_str(src, *dst, &cp);
        safe_str(to, *dst, &cp);
        *cp = '\0';
    }
    else
    {
        // Replace all occurances of 'from' with 'to'. Handle the special
        // cases of from = \$ and \^.
        //
        if (  (  from[0] == '\\'
              || from[0] == '%')
           && (  from[1] == '$'
              || from[1] == '^')
           && from[2] == '\0')
        {
            from++;
        }
        *dst = replace_string(from, to, src);
    }
}

static void edit_string_ansi(UTF8 *src, UTF8 **dst, UTF8 **returnstr, UTF8 *from, UTF8 *to)
{
    UTF8 *cp, *rp;

    // Do the substitution.  Idea for prefix/suffix from R'nice@TinyTIM
    //
    if (!strcmp((char *)from, "^"))
    {
        // Prepend 'to' to string.
        //
        *dst = alloc_lbuf("edit_string.^");
        cp = *dst;
        safe_str(to, *dst, &cp);
        safe_str(src, *dst, &cp);
        *cp = '\0';

        // Do the ansi string used to notify.
        //
        *returnstr = alloc_lbuf("edit_string_ansi.^");
        rp = *returnstr;
        safe_str((UTF8 *)COLOR_INTENSE, *returnstr, &rp);
        safe_str(to, *returnstr, &rp);
        safe_str((UTF8 *)COLOR_RESET, *returnstr, &rp);
        safe_str(src, *returnstr, &rp);
        *rp = '\0';

    }
    else if (!strcmp((char *)from, "$"))
    {
        // Append 'to' to string
        //
        *dst = alloc_lbuf("edit_string.$");
        cp = *dst;
        safe_str(src, *dst, &cp);
        safe_str(to, *dst, &cp);
        *cp = '\0';

        // Do the ansi string used to notify.
        //
        *returnstr = alloc_lbuf("edit_string_ansi.$");
        rp = *returnstr;
        safe_str(src, *returnstr, &rp);
        safe_str((UTF8 *)COLOR_INTENSE, *returnstr, &rp);
        safe_str(to, *returnstr, &rp);
        safe_str((UTF8 *)COLOR_RESET, *returnstr, &rp);
        *rp = '\0';

    }
    else
    {
        // Replace all occurances of 'from' with 'to'. Handle the special
        // cases of from = \$ and \^.
        //
        if (  ((from[0] == '\\') || (from[0] == '%'))
           && ((from[1] == '$')  || (from[1] == '^'))
           && ( from[2] == '\0'))
        {
            from++;
        }

        *dst = replace_string(from, to, src);
        *returnstr = replace_string(from, tprintf("%s%s%s", COLOR_INTENSE,
                             to, COLOR_RESET), src);
    }
}

void do_edit(dbref executor, dbref caller, dbref enactor, int eval, int key,
             UTF8 *it, UTF8 *args[], int nargs)
{
    UNUSED_PARAMETER(eval);
    UNUSED_PARAMETER(caller);
    UNUSED_PARAMETER(enactor);
    UNUSED_PARAMETER(key);

    dbref thing, aowner;
    int atr, aflags;
    bool bGotOne;
    UTF8 *from, *to, *result, *returnstr, *atext;
    ATTR *ap;

    // Make sure we have something to do.
    //
    if (  nargs < 1
       || !*args[0])
    {
        notify_quiet(executor, T("Nothing to do."));
        return;
    }
    from = args[0];
    to = (nargs >= 2) ? args[1] : (UTF8 *)"";

    // Look for the object and get the attribute (possibly wildcarded)
    //
    olist_push();
    if (  !it
       || !*it
       || !parse_attrib_wild(executor, it, &thing, false, false, false))
    {
        notify_quiet(executor, T("No match."));
        return;
    }

    // Iterate through matching attributes, performing edit.
    //
    bGotOne = 0;
    atext = alloc_lbuf("do_edit.atext");
    bool could_hear = Hearer(thing);

    for (atr = olist_first(); atr != NOTHING; atr = olist_next())
    {
        ap = atr_num(atr);
        if (ap)
        {
            // Get the attr and make sure we can modify it.
            //
            atr_get_str(atext, thing, ap->number, &aowner, &aflags);
            if (bCanSetAttr(executor, thing, ap))
            {
                // Do the edit and save the result
                //
                bGotOne = true;
                edit_string_ansi(atext, &result, &returnstr, from, to);
                atr_add(thing, ap->number, result, Owner(executor), aflags);
                if (!Quiet(executor))
                {
                    notify_quiet(executor, tprintf("Set - %s: %s", ap->name,
                                 returnstr));
                }
                free_lbuf(result);
                free_lbuf(returnstr);
            }
            else
            {
                // No rights to change the attr.
                //
                notify_quiet(executor, tprintf("%s: Permission denied.", ap->name));
            }

        }
    }

    // Clean up.
    //
    free_lbuf(atext);
    olist_pop();

    if (!bGotOne)
    {
        notify_quiet(executor, T("No matching attributes."));
    }
    else
    {
        handle_ears(thing, could_hear, Hearer(thing));
    }
}

void do_wipe(dbref executor, dbref caller, dbref enactor, int eval, int key, UTF8 *it)
{
    UNUSED_PARAMETER(caller);
    UNUSED_PARAMETER(enactor);
    UNUSED_PARAMETER(eval);
    UNUSED_PARAMETER(key);

    dbref thing;

    olist_push();
    if (  !it
       || !*it
       || !parse_attrib_wild(executor, it, &thing, false, false, true))
    {
        notify_quiet(executor, T("No match."));
        return;
    }
    if (  mudconf.safe_wipe
       && has_flag(NOTHING, thing, T("SAFE")))
    {
        notify_quiet(executor, T("SAFE objects may not be @wiped."));
        return;
    }

    // Iterate through matching attributes, zapping the writable ones
    //
    int atr;
    ATTR *ap;
    bool bGotOne = false, could_hear = Hearer(thing);

    for (atr = olist_first(); atr != NOTHING; atr = olist_next())
    {
        ap = atr_num(atr);
        if (ap)
        {
            // Get the attr and make sure we can modify it.
            //
            if (bCanSetAttr(executor, thing, ap))
            {
                atr_clr(thing, ap->number);
                bGotOne = true;
            }
        }
    }

    // Clean up
    //
    olist_pop();

    if (!bGotOne)
    {
        notify_quiet(executor, T("No matching attributes."));
    }
    else
    {
        set_modified(thing);
        handle_ears(thing, could_hear, Hearer(thing));
        if (!Quiet(executor))
        {
            notify_quiet(executor, T("Wiped."));
        }
    }
}

void do_trigger(dbref executor, dbref caller, dbref enactor, int eval, int key,
                UTF8 *object, UTF8 *argv[], int nargs)
{
    dbref thing;
    ATTR *pattr;

    if (!( parse_attrib(executor, object, &thing, &pattr)
        && pattr))
    {
        notify_quiet(executor, T("No match."));
        return;
    }

    if (!Controls(executor, thing))
    {
        notify_quiet(executor, NOPERM_MESSAGE);
        return;
    }
    did_it(executor, thing, 0, NULL, 0, NULL, pattr->number, 0, argv, nargs);

    if (key & TRIG_NOTIFY)
    {
        UTF8 *tbuf = alloc_lbuf("trigger.notify_cmd");
        mux_strncpy(tbuf, T("@notify/quiet me"), LBUF_SIZE-1);
        CLinearTimeAbsolute lta;
        wait_que(executor, caller, enactor, eval, false, lta, NOTHING, A_SEMAPHORE,
            tbuf,
            0, NULL,
            mudstate.global_regs);
        free_lbuf(tbuf);
    }

    if (  !(key & TRIG_QUIET)
       && !Quiet(executor))
    {
        notify_quiet(executor, T("Triggered."));
    }
}

void do_use(dbref executor, dbref caller, dbref enactor, int eval, int key, UTF8 *object)
{
    UNUSED_PARAMETER(caller);
    UNUSED_PARAMETER(enactor);
    UNUSED_PARAMETER(eval);
    UNUSED_PARAMETER(key);

    UTF8 *df_use, *df_ouse, *temp;
    dbref thing, aowner;
    int aflags;

    init_match(executor, object, NOTYPE);
    match_neighbor();
    match_possession();
    if (Wizard(executor))
    {
        match_absolute();
        match_player();
    }
    match_me();
    match_here();
    thing = noisy_match_result();
    if (thing == NOTHING)
        return;

    // Make sure player can use it.
    //
    if (!could_doit(executor, thing, A_LUSE))
    {
        did_it(executor, thing, A_UFAIL,
               T("You can't figure out how to use that."),
               A_OUFAIL, NULL, A_AUFAIL, 0, NULL, 0);
        return;
    }
    temp = alloc_lbuf("do_use");
    bool doit = false;
    if (*atr_pget_str(temp, thing, A_USE, &aowner, &aflags))
    {
        doit = true;
    }
    else if (*atr_pget_str(temp, thing, A_OUSE, &aowner, &aflags))
    {
        doit = true;
    }
    else if (*atr_pget_str(temp, thing, A_AUSE, &aowner, &aflags))
    {
        doit = true;
    }
    free_lbuf(temp);

    if (doit)
    {
        df_use = alloc_lbuf("do_use.use");
        df_ouse = alloc_lbuf("do_use.ouse");
        mux_sprintf(df_use, LBUF_SIZE, "You use %s", Moniker(thing));
        mux_sprintf(df_ouse, LBUF_SIZE, "uses %s", Moniker(thing));
        did_it(executor, thing, A_USE, df_use, A_OUSE, df_ouse, A_AUSE, 0,
            NULL, 0);
        free_lbuf(df_use);
        free_lbuf(df_ouse);
    }
    else
    {
        notify_quiet(executor, T("You can't figure out how to use that."));
    }
}

/*
 * ---------------------------------------------------------------------------
 * * do_setvattr: Set a user-named (or possibly a predefined) attribute.
 */

void do_setvattr
(
    dbref executor,
    dbref caller,
    dbref enactor,
    int   key,
    int   nargs,
    UTF8 *arg1,
    UTF8 *arg2
)
{
    UNUSED_PARAMETER(key);
    UNUSED_PARAMETER(nargs);

    UTF8 *s;
    int anum;

    // Skip the '&'
    //
    arg1++;

    // Take to the space
    //
    for (s = arg1; *s && !mux_isspace(*s); s++)
    {
        ; // Nothing.
    }

    // Split it
    //
    if (*s)
    {
        *s++ = '\0';
    }

    // Get or make attribute
    //
    anum = mkattr(executor, (UTF8 *)arg1);

    if (anum <= 0)
    {
        notify_quiet(executor, T("That's not a good name for an attribute."));
        return;
    }
    do_setattr(executor, caller, enactor, anum, 2, s, arg2);
}
