/* scenario: play a scenario file through the game, for tools/scenarios/run.py (#5).
 *
 * Linked against the game's object files, with comm.c compiled as -Dmain=game_main. Nothing here is the game's:
 * it calls the game's own nanny, show_string, substitute_alias (which calls interpret), the updates and
 * process_output, in the order game_loop_unix calls them, on connections whose "socket" is one end of a
 * socketpair. There is no network and no real clock: a pulse is a call, and the clock moves a second every 4.
 *
 * Adapted from QuickMUD-Python's tools/oracle/scenario.c (at its c-final tag). The game's source isn't changed for
 * it (#5, decision 1):
 *   - the seed: linked with -Wl,--wrap=srandom, so init_mm's srandom (time (NULL) ^ getpid ()) gets @seed's value;
 *   - the updates: the driver runs them itself, on update_handler's schedule, and only those @updates names;
 *   - random(): linked with -Wl,--wrap=random, so every draw is counted and hashed.
 *
 * Usage: scenario < FILE (run from the area directory, like the game).
 *
 * The scenario format, one thing per line (# starts a comment line):
 *   @seed N              the seed for srandom (before anything else; default 20261117)
 *   @clock T             current_time at boot (before anything else; default 1763400000)
 *   @updates LIST        which updates run: all, or names from area, song, mobile, violence, weather, char, obj
 *                        and aggr, comma-separated (before anything else; default area). The pulse counters and
 *                        the TICK wiznet line carry on whatever it says.
 *   @place NAME DEST     copy the fixture tests/files/NAME (../tests/files/NAME here) to DEST, relative to the
 *                        area directory, before boot reads it: a ban list (ban.txt), say
 *   @remove PATH         delete PATH, relative to the area directory, before boot reads it (music.txt, say)
 *   @log                 send the game's log (its stderr) to the output, among the steps (before boot)
 *   @exit N              the game is expected to exit with status N, as it does on a fatal bug() at boot;
 *                        the driver ignores it, and run.py passes the scenario only if the driver exits N
 *   @connect NAME        a new connection, called NAME in the output, which becomes the one lines go to
 *   @as NAME             send the following lines to connection NAME
 *   @pulse N             run N pulses with no new input
 *   @pulse until N TEXT  run pulses until a connection is sent TEXT (a fight's "is DEAD!!"), at most N
 *   @mob VNUM ROOM       create_mobile and char_to_room
 *   @obj VNUM ROOM       create_object (level 0) and obj_to_room
 *   @obj VNUM ROOM V0 V1 V2 V3 V4   the same, then sets its five values (furniture, portals, locks)
 *   @owner NAME          the last @obj's owner, as make_corpse sets a player's corpse's (can_loot)
 *   @charm               charm the last @mob: it follows the current connection's character, in its group
 *   @charm pet           the same, and it's that character's pet, as do_buy makes one
 *   @spec [N]            run the last @mob's special N times (default 1), as mobile_update would
 *   @state               dump the characters and objects in every room a connection's character is in
 *   @return              an empty line of input (blank lines are skipped), then one pulse
 *   @file PATH           print a file the game wrote, such as ../player/NAME (relative to the area directory)
 *   @mkdir PATH          make a directory at PATH (after boot), so the game can't open a file there for writing,
 *                        even as root: ban.txt, say
 *   anything else        a line of input for the current connection, then one pulse
 *
 * The output, for every step (a line or an @ line):
 *   step N TEXT          what the step was
 *   out NAME "BYTES"     what connection NAME was sent during the step (escaped: see esc)
 *   closed NAME          the game closed connection NAME
 *   file PATH "BYTES"    @file's file, or "file PATH missing"
 *   spec R...            what each @spec run returned (1 or 0), or "gone" once the mob was extracted
 *   pulses K             how many pulses @pulse until ran (", not seen" if TEXT never came)
 *   rng DRAWS HASH       random() calls so far, and an FNV-1a hash of every value they returned
 *   state ...            @state's lines: for each room, its exits (where each leads, exit_info and key), then each
 *                        character (position, points, wait, flags, train, the furniture it's on and its five
 *                        permanent stats) with its affects and what it carries, then the objects on the floor
 *
 * Unlike c-final's driver:
 *   - a connection that's sent a line has its character's idle timer reset in that pulse, as game_loop_unix does
 *     for a descriptor with input waiting;
 *   - a new connection is checked against the site bans (host "scenario") as init_descriptor does, and a banned
 *     one is told so and closed before the greeting.
 *
 * Part of 2.4b6-fixed's tools (MIT, tools/LICENSE). It calls the game's code but contains none of it.
 */
#define _GNU_SOURCE  /* memmem */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include "merc.h"
#include "recycle.h"
#include "music.h"

/* The game's functions that no header declares. */
void nanny (DESCRIPTOR_DATA * d, char *argument);   /* comm.c */
void read_from_buffer (DESCRIPTOR_DATA * d);         /* comm.c */
bool process_output (DESCRIPTOR_DATA * d, bool fPrompt);
void stop_idling (CHAR_DATA * ch);
bool write_to_descriptor (int desc, char *txt, int length);
void mobile_update (void);                           /* update.c */
void weather_update (void);
void char_update (void);
void obj_update (void);
void aggr_update (void);
extern char *help_greeting;                          /* db.c */

/* -- random(), counted; srandom(), seeded by the scenario ------------------------------------------------------ */

long __real_random (void);
void __real_srandom (unsigned int seed);
static unsigned long draws;
static unsigned int rng_hash = 2166136261u;
static unsigned int seed_value = 20261117u;

long __wrap_random (void)
{
    long v = __real_random ();
    int i;

    draws++;
    for (i = 0; i < 4; i++)  /* FNV-1a over the value's 4 low bytes, little end first */
    {
        rng_hash ^= (unsigned int) ((v >> (8 * i)) & 0xff);
        rng_hash *= 16777619u;
    }
    return v;
}

void __wrap_srandom (unsigned int seed)
{
    (void) seed;  /* init_mm's time (NULL) ^ getpid () */
    __real_srandom (seed_value);
}

/* -- the updates: update_handler's schedule, with only the named ones run ----------------------------------- */

#define UPD_AREA     (1 << 0)
#define UPD_SONG     (1 << 1)
#define UPD_MOBILE   (1 << 2)
#define UPD_VIOLENCE (1 << 3)
#define UPD_WEATHER  (1 << 4)
#define UPD_CHAR     (1 << 5)
#define UPD_OBJ      (1 << 6)
#define UPD_AGGR     (1 << 7)

static int updates_on;

static int parse_updates (const char *list)
{
    static const struct { const char *name; int bit; } names[] = {
        {"area", UPD_AREA}, {"song", UPD_SONG}, {"mobile", UPD_MOBILE}, {"violence", UPD_VIOLENCE},
        {"weather", UPD_WEATHER}, {"char", UPD_CHAR}, {"obj", UPD_OBJ}, {"aggr", UPD_AGGR}, {"all", -1},
        {NULL, 0}
    };
    char copy[256], *name;
    int on = 0, i;

    snprintf (copy, sizeof copy, "%s", list);
    for (name = strtok (copy, ","); name != NULL; name = strtok (NULL, ","))
    {
        for (i = 0; names[i].name != NULL && strcmp (name, names[i].name); i++)
            ;
        if (names[i].name == NULL)
        {
            fprintf (stderr, "scenario: @updates: no update called %s\n", name);
            exit (2);
        }
        on |= names[i].bit;
    }
    return on;
}

/* update_handler, line for line, with each update run only if it's on. */
static void updates (void)
{
    static int pulse_area, pulse_mobile, pulse_violence, pulse_point, pulse_music;

    if (--pulse_area <= 0)
    {
        pulse_area = PULSE_AREA;
        if (updates_on & UPD_AREA) area_update ();
    }
    if (--pulse_music <= 0)
    {
        pulse_music = PULSE_MUSIC;
        if (updates_on & UPD_SONG) song_update ();
    }
    if (--pulse_mobile <= 0)
    {
        pulse_mobile = PULSE_MOBILE;
        if (updates_on & UPD_MOBILE) mobile_update ();
    }
    if (--pulse_violence <= 0)
    {
        pulse_violence = PULSE_VIOLENCE;
        if (updates_on & UPD_VIOLENCE) violence_update ();
    }
    if (--pulse_point <= 0)
    {
        wiznet ("TICK!", NULL, NULL, WIZ_TICKS, 0, 0);
        pulse_point = PULSE_TICK;
        if (updates_on & UPD_WEATHER) weather_update ();
        if (updates_on & UPD_CHAR) char_update ();
        if (updates_on & UPD_OBJ) obj_update ();
    }
    if (updates_on & UPD_AGGR) aggr_update ();
    tail_chain ();
}

/* -- connections ------------------------------------------------------------------------------------------- */

#define MAX_CONN 16

struct conn
{
    char name[32];
    DESCRIPTOR_DATA *d;
    int peer;      /* our end of the socketpair: what the game writes to d->descriptor arrives here */
    bool closed;
    bool input;    /* sent a line this pulse: game_loop_unix's FD_ISSET */
    unsigned char got[65536];  /* read from peer during this step, not printed yet */
    size_t ngot;
};

static struct conn conns[MAX_CONN];
static int nconn, current = -1;
static long pulses;
static time_t start_time;

static int find_conn (const char *name)
{
    int i;
    for (i = 0; i < nconn; i++)
        if (!strcmp (conns[i].name, name))
            return i;
    fprintf (stderr, "scenario: no connection called %s\n", name);
    exit (2);
}

static bool listed (DESCRIPTOR_DATA * d)
{
    DESCRIPTOR_DATA *x;
    for (x = descriptor_list; x != NULL; x = x->next)
        if (x == d)
            return TRUE;
    return FALSE;
}

static void connect_as (const char *name)
{
    int sv[2];
    DESCRIPTOR_DATA *d;
    struct conn *c;

    if (nconn == MAX_CONN || socketpair (AF_UNIX, SOCK_STREAM, 0, sv) < 0)
    {
        fprintf (stderr, "scenario: can't connect %s\n", name);
        exit (2);
    }
    fcntl (sv[1], F_SETFL, O_NONBLOCK);
    /* The game's end gets descriptor 100 + its index, as c-final's driver numbers it. */
    if (dup2 (sv[0], 100 + nconn) < 0)
    {
        fprintf (stderr, "scenario: can't number %s's descriptor\n", name);
        exit (2);
    }
    close (sv[0]);
    sv[0] = 100 + nconn;
    /* The rest is init_descriptor's, from new_descriptor on. */
    d = new_descriptor ();
    d->descriptor = sv[0];
    d->connected = CON_GET_NAME;
    d->showstr_head = NULL;
    d->showstr_point = NULL;
    d->outsize = 2000;
    d->outbuf = alloc_mem (d->outsize);
    d->host = str_dup ("scenario");
    c = &conns[nconn];
    snprintf (c->name, sizeof c->name, "%s", name);
    c->d = d;
    c->peer = sv[1];
    c->closed = FALSE;
    c->input = FALSE;
    current = nconn++;
    if (check_ban (d->host, BAN_ALL))  /* a ban on "scenario", say *nar* */
    {
        write_to_descriptor (sv[0], "Your site has been banned from this mud.\n\r", 0);
        close (sv[0]);
        free_descriptor (d);  /* never listed, so collect reports it closed */
        return;
    }
    d->next = descriptor_list;
    descriptor_list = d;
    write_to_buffer (d, help_greeting[0] == '.' ? help_greeting + 1 : help_greeting, 0);
}

static struct conn *conn_of (DESCRIPTOR_DATA * d)
{
    int i;
    for (i = 0; i < nconn; i++)
        if (conns[i].d == d && !conns[i].closed)
            return &conns[i];
    return NULL;
}

/* -- the pulse: game_loop_unix's input, update and output steps, without select() -------------------------- */

static void pulse (void)
{
    DESCRIPTOR_DATA *d, *d_next;
    struct conn *c;

    for (d = descriptor_list; d != NULL; d = d_next)
    {
        d_next = d->next;
        d->fcommand = FALSE;
        if ((c = conn_of (d)) != NULL && c->input)
        {
            c->input = FALSE;
            if (d->character != NULL)
                d->character->timer = 0;
        }
        if (d->character != NULL && d->character->daze > 0)
            --d->character->daze;
        if (d->character != NULL && d->character->wait > 0)
        {
            --d->character->wait;
            continue;
        }
        read_from_buffer (d);
        if (d->incomm[0] != '\0')
        {
            d->fcommand = TRUE;
            stop_idling (d->character);
            if (d->showstr_point)
                show_string (d, d->incomm);
            else if (d->connected == CON_PLAYING)
                substitute_alias (d, d->incomm);
            else
                nanny (d, d->incomm);
            d->incomm[0] = '\0';
        }
    }
    updates ();
    for (d = descriptor_list; d != NULL; d = d_next)
    {
        d_next = d->next;
        if (d->fcommand || d->outtop > 0)
        {
            if (!process_output (d, TRUE))
            {
                if (d->character != NULL && d->connected == CON_PLAYING)
                    save_char_obj (d->character);
                d->outtop = 0;
                close_socket (d);
            }
        }
    }
    pulses++;
    current_time = start_time + pulses / PULSE_PER_SECOND;
}

/* -- output ------------------------------------------------------------------------------------------------ */

/* Bytes on one line: \\, \", \n, \r, and \xNN for anything outside printable ASCII. */
static void esc (const unsigned char *s, size_t n)
{
    size_t i;
    putchar ('"');
    for (i = 0; i < n; i++)
    {
        if (s[i] == '\\') fputs ("\\\\", stdout);
        else if (s[i] == '"') fputs ("\\\"", stdout);
        else if (s[i] == '\n') fputs ("\\n", stdout);
        else if (s[i] == '\r') fputs ("\\r", stdout);
        else if (s[i] < 32 || s[i] > 126) printf ("\\x%02x", s[i]);
        else putchar (s[i]);
    }
    putchar ('"');
}

/* Read what the game has sent each open connection so far this step. */
static void drain (void)
{
    int i;
    ssize_t n;

    for (i = 0; i < nconn; i++)
    {
        struct conn *c = &conns[i];
        if (c->closed)
            continue;
        while (c->ngot < sizeof c->got && (n = read (c->peer, c->got + c->ngot, sizeof c->got - c->ngot)) > 0)
            c->ngot += n;
    }
}

/* Has an open connection been sent TEXT this step? */
static bool sent (const char *text)
{
    int i;
    for (i = 0; i < nconn; i++)
        if (!conns[i].closed && memmem (conns[i].got, conns[i].ngot, text, strlen (text)) != NULL)
            return TRUE;
    return FALSE;
}

static void collect (void)
{
    int i;

    drain ();
    for (i = 0; i < nconn; i++)
    {
        struct conn *c = &conns[i];

        if (c->closed)
            continue;
        if (c->ngot > 0)
        {
            printf ("out %s ", c->name);
            esc (c->got, c->ngot);
            putchar ('\n');
            c->ngot = 0;
        }
        if (!listed (c->d))  /* close_socket took it off the list (and closed its end) */
        {
            printf ("closed %s\n", c->name);
            c->closed = TRUE;
            close (c->peer);
        }
    }
    printf ("rng %lu %08x\n", draws, rng_hash);
}

static void dump_obj (const char *where, OBJ_DATA * obj, int depth)
{
    OBJ_DATA *in;
    printf ("state %*sobj %s %d wear %d timer %d cost %d values %d %d %d %d %d\n", depth * 2, "", where,
            obj->pIndexData->vnum, obj->wear_loc, obj->timer, obj->cost, obj->value[0], obj->value[1],
            obj->value[2], obj->value[3], obj->value[4]);
    for (in = obj->contains; in != NULL; in = in->next_content)
        dump_obj ("in", in, depth + 1);
}

static void state (void)
{
    ROOM_INDEX_DATA *seen[MAX_CONN];
    int nseen = 0, i, j;

    for (i = 0; i < nconn; i++)
    {
        ROOM_INDEX_DATA *room;
        CHAR_DATA *ch;
        OBJ_DATA *obj;
        AFFECT_DATA *af;

        if (conns[i].closed || !listed (conns[i].d) || conns[i].d->character == NULL
            || (room = conns[i].d->character->in_room) == NULL)
            continue;
        for (j = 0; j < nseen && seen[j] != room; j++)
            ;
        if (j < nseen)
            continue;
        seen[nseen++] = room;
        printf ("state room %d light %d people", room->vnum, room->light);
        for (ch = room->people; ch != NULL; ch = ch->next_in_room)
            printf (" %s", IS_NPC (ch) ? "mob" : "player");
        putchar ('\n');
        for (j = 0; j < 6; j++)
            if (room->exit[j] != NULL)
                printf ("state exit %d to %d info %d key %d\n", j,
                        room->exit[j]->u1.to_room ? room->exit[j]->u1.to_room->vnum : 0,
                        room->exit[j]->exit_info, room->exit[j]->key);
        for (ch = room->people; ch != NULL; ch = ch->next_in_room)
        {
            printf ("state char %s %d pos %d hit %d/%d mana %d/%d move %d/%d wait %d daze %d exp %d gold %ld "
                    "silver %ld act %ld affected_by %ld train %d on %d stats %d %d %d %d %d\n",
                    IS_NPC (ch) ? "mob" : ch->name,
                    IS_NPC (ch) ? ch->pIndexData->vnum : ch->level, ch->position, ch->hit, ch->max_hit,
                    ch->mana, ch->max_mana, ch->move, ch->max_move, ch->wait, ch->daze, ch->exp, ch->gold,
                    ch->silver, ch->act, ch->affected_by, ch->train, ch->on ? ch->on->pIndexData->vnum : 0,
                    ch->perm_stat[0], ch->perm_stat[1], ch->perm_stat[2], ch->perm_stat[3], ch->perm_stat[4]);
            for (af = ch->affected; af != NULL; af = af->next)
                printf ("state   affect %d level %d duration %d location %d modifier %d bitvector %d\n", af->type,
                        af->level, af->duration, af->location, af->modifier, af->bitvector);
            for (obj = ch->carrying; obj != NULL; obj = obj->next_content)
                dump_obj ("carried", obj, 1);
        }
        for (obj = room->contents; obj != NULL; obj = obj->next_content)
            dump_obj ("floor", obj, 0);
    }
}

/* -- the script -------------------------------------------------------------------------------------------- */

static void chomp (char *s)
{
    size_t n = strlen (s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r'))
        s[--n] = '\0';
}

/* @place: copy a fixture file byte for byte; 0 if it worked. */
static int place (const char *from, const char *to)
{
    char buf[8192];
    size_t n;
    FILE *in = fopen (from, "rb"), *out;
    if (in == NULL)
        return -1;
    if ((out = fopen (to, "wb")) == NULL)
    {
        fclose (in);
        return -1;
    }
    while ((n = fread (buf, 1, sizeof buf, in)) > 0)
        fwrite (buf, 1, n, out);
    fclose (in);
    return fclose (out) == 0 ? 0 : -1;
}

int main (void)
{
    char line[MAX_STRING_LENGTH];
    const char *updates_list = "area";
    long clock = 1763400000;
    bool booted = FALSE;
    long step = 0;
    CHAR_DATA *last_mob = NULL;
    OBJ_DATA *last_obj = NULL;

    while (fgets (line, sizeof line, stdin) != NULL)
    {
        char word[32];
        long a = 0, b = 0, v[5];
        int n, j;

        chomp (line);
        if (line[0] == '#' || line[0] == '\0')
            continue;
        if (!booted)
        {
            static char updbuf[64];
            unsigned long s;
            if (sscanf (line, "@seed %lu", &s) == 1) { seed_value = (unsigned int) s; continue; }
            if (sscanf (line, "@clock %ld", &clock) == 1) continue;
            if (sscanf (line, "@updates %63s", updbuf) == 1) { updates_list = updbuf; continue; }
            if (!strcmp (line, "@log"))  /* the game's log (stderr) joins the output, in order */
            {
                setvbuf (stdout, NULL, _IONBF, 0);
                dup2 (1, 2);
                continue;
            }
            if (sscanf (line, "@exit %d", &n) == 1)  /* read by run.py */
                continue;
            {
                char src[96], dest[128], from[160];
                if (sscanf (line, "@place %95s %127s", src, dest) == 2)
                {
                    snprintf (from, sizeof from, "../tests/files/%s", src);
                    if (place (from, dest) != 0)
                    {
                        fprintf (stderr, "scenario: @place %s %s: can't copy it\n", src, dest);
                        return 2;
                    }
                    continue;
                }
                if (sscanf (line, "@remove %127s", dest) == 1)
                {
                    if (remove (dest) != 0)
                    {
                        fprintf (stderr, "scenario: @remove %s: can't remove it\n", dest);
                        return 2;
                    }
                    continue;
                }
            }
            updates_on = parse_updates (updates_list);
            current_time = start_time = (time_t) clock;
            if ((fpReserve = fopen (NULL_FILE, "r")) == NULL)  /* main reserves one, which file code swaps */
            {
                perror (NULL_FILE);
                return 2;
            }
            boot_db ();
            booted = TRUE;
            printf ("boot seed %u clock %ld updates %s\n", seed_value, clock, updates_list);
            collect ();
        }
        printf ("step %ld ", ++step);
        esc ((unsigned char *) line, strlen (line));
        putchar ('\n');
        if (line[0] != '@' || !strcmp (line, "@return"))
        {
            DESCRIPTOR_DATA *d;
            if (line[0] == '@')
                line[0] = '\0';  /* @return: the input is the empty line */
            if (current < 0 || conns[current].closed || !listed (d = conns[current].d))
            {
                fprintf (stderr, "scenario: step %ld: no open connection to send to\n", step);
                return 2;
            }
            if (strlen (d->inbuf) + strlen (line) + 2 >= sizeof d->inbuf)
            {
                fprintf (stderr, "scenario: step %ld: the input buffer is full\n", step);
                return 2;
            }
            strcat (d->inbuf, line);
            strcat (d->inbuf, "\n");
            conns[current].input = TRUE;
            pulse ();
        }
        else if (sscanf (line, "@connect %31s", word) == 1)
            connect_as (word);
        else if (sscanf (line, "@as %31s", word) == 1)
            current = find_conn (word);
        else if (sscanf (line, "@pulse until %ld %n", &a, &n) == 1 && n > 0 && line[n] != '\0')
        {
            long k = 0;
            while (k < a && !sent (line + n))
            {
                pulse ();
                drain ();
                k++;
            }
            printf ("pulses %ld%s\n", k, sent (line + n) ? "" : ", not seen");
        }
        else if (sscanf (line, "@pulse %ld", &a) == 1)
            while (a-- > 0)
                pulse ();
        else if (sscanf (line, "@mob %ld %ld", &a, &b) == 2)
        {
            CHAR_DATA *mob = create_mobile (get_mob_index (a));
            char_to_room (mob, get_room_index (b));
            last_mob = mob;
        }
        else if (!strcmp (line, "@charm") || !strcmp (line, "@charm pet"))
        {
            CHAR_DATA *ch;
            if (last_mob == NULL || current < 0 || conns[current].closed || !listed (conns[current].d)
                || (ch = conns[current].d->character) == NULL)
            {
                fprintf (stderr, "scenario: step %ld: @charm needs a @mob and a character\n", step);
                return 2;
            }
            if (line[6] == ' ')
                SET_BIT (last_mob->act, ACT_PET);
            SET_BIT (last_mob->affected_by, AFF_CHARM);
            add_follower (last_mob, ch);
            last_mob->leader = ch;
            if (line[6] == ' ')
                ch->pet = last_mob;
        }
        else if ((n = sscanf (line, "@obj %ld %ld %ld %ld %ld %ld %ld", &a, &b, &v[0], &v[1], &v[2], &v[3],
                              &v[4])) == 2 || n == 7)
        {
            OBJ_DATA *obj = create_object (get_obj_index (a), 0);
            if (n == 7)
                for (j = 0; j < 5; j++)
                    obj->value[j] = v[j];
            obj_to_room (obj, get_room_index (b));
            last_obj = obj;
        }
        else if (sscanf (line, "@owner %31s", word) == 1)
        {
            if (last_obj == NULL)
            {
                fprintf (stderr, "scenario: step %ld: @owner needs an @obj\n", step);
                return 2;
            }
            free_string (last_obj->owner);
            last_obj->owner = str_dup (word);
        }
        else if (!strcmp (line, "@spec") || (sscanf (line, "@spec %ld%n", &a, &n) == 1 && line[n] == '\0'))
        {
            long times = line[5] == ' ' ? a : 1;
            if (last_mob == NULL || last_mob->spec_fun == NULL)
            {
                fprintf (stderr, "scenario: step %ld: @spec needs a @mob with a special\n", step);
                return 2;
            }
            printf ("spec");
            while (times-- > 0)
            {
                CHAR_DATA *ch;
                for (ch = char_list; ch != NULL && ch != last_mob; ch = ch->next)
                    ;
                if (ch == NULL)
                {
                    printf (" gone");  /* extracted: its memory may be reused, so stop */
                    break;
                }
                printf (" %d", (*last_mob->spec_fun) (last_mob) ? 1 : 0);
            }
            putchar ('\n');
        }
        else if (!strcmp (line, "@state"))
            state ();
        else if (!strncmp (line, "@file ", 6))
        {
            static unsigned char fbuf[262144];
            FILE *fp = fopen (line + 6, "rb");
            if (fp == NULL)
                printf ("file %s missing\n", line + 6);
            else
            {
                size_t got = fread (fbuf, 1, sizeof fbuf, fp);
                fclose (fp);
                printf ("file %s ", line + 6);
                esc (fbuf, got);
                putchar ('\n');
            }
        }
        else if (!strncmp (line, "@mkdir ", 7))
        {
            if (mkdir (line + 7, 0755) != 0)
            {
                fprintf (stderr, "scenario: @mkdir %s: can't make it\n", line + 7);
                return 2;
            }
        }
        else
        {
            fprintf (stderr, "scenario: step %ld: unknown line %s\n", step, line);
            return 2;
        }
        collect ();
        fflush (stdout);
    }
    return 0;
}
