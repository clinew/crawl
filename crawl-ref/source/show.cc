/**
 * @file
 * @brief Updates the screen via map_knowledge.
**/

#include "AppHdr.h"

#include "show.h"

#include "areas.h"
#include "cloud.h"
#include "coord.h"
#include "coordit.h"
#include "dgnevent.h"
#include "dgn-overview.h"
#include "directn.h"
#include "dungeon.h"
#include "env.h"
#include "exclude.h"
#include "fprop.h"
#include "itemprop.h"
#include "mon-place.h"
#include "mon-util.h"
#include "monster.h"
#include "options.h"
#include "random.h"
#include "showsymb.h"
#include "state.h"
#include "areas.h"
#include "terrain.h"
#ifdef USE_TILE
 #include "tileview.h"
#endif
#include "tiledef-main.h"
#include "traps.h"
#include "travel.h"
#include "viewmap.h"

show_type::show_type()
    : cls(SH_NOTHING),
      feat(DNGN_UNSEEN),
      item(SHOW_ITEM_NONE),
      mons(MONS_NO_MONSTER),
      colour(0)
{
}

show_type::show_type(monster_type montype)
    : cls(SH_MONSTER),
      feat(DNGN_UNSEEN),
      item(SHOW_ITEM_NONE),
      mons(montype),
      colour(0)
{
}

show_type::show_type(dungeon_feature_type f)
    : cls(SH_FEATURE),
      feat(f),
      item(SHOW_ITEM_NONE),
      mons(MONS_NO_MONSTER),
      colour(0)
{
}

static show_item_type _item_to_show_code(const item_def &item);

show_type::show_type(const item_def &it)
    : cls(SH_ITEM),
      feat(DNGN_UNSEEN),
      item(_item_to_show_code(it)),
      mons(MONS_NO_MONSTER),
      colour(0)
{
}

show_type::show_type(show_item_type t)
    : cls(SH_ITEM),
      feat(DNGN_UNSEEN),
      item(t),
      mons(MONS_NO_MONSTER),
      colour(0)
{
}

bool show_type::operator < (const show_type &other) const
{
    if (cls < other.cls)
        return false;
    else if (cls > other.cls)
        return true;
    switch (cls)
    {
    case SH_FEATURE:
        return (feat < other.feat);
    case SH_ITEM:
        return (item < other.item);
    case SH_MONSTER:
        return (mons < other.mons);
    default:
        return false;
    }
}

// Returns true if this is a monster that can be hidden for clean_map
// purposes. All non-immobile monsters are hidden when out of LOS with
// Options.clean_map, or removed from the map when the clear-map
// command (^C) is used.
bool show_type::is_cleanable_monster() const
{
    return (cls == SH_MONSTER && !mons_class_is_stationary(mons));
}

static void _update_feat_at(const coord_def &gp)
{
    if (!you.see_cell(gp))
        return;

    dungeon_feature_type feat = grid_appearance(gp);
    unsigned colour = env.grid_colours(gp);
    trap_type trap = TRAP_UNASSIGNED;
    if (feat_is_trap(feat))
        trap = get_trap_type(gp);

    env.map_knowledge(gp).set_feature(feat, colour, trap);

    if (haloed(gp))
        env.map_knowledge(gp).flags |= MAP_HALOED;

    if (umbraed(gp))
        env.map_knowledge(gp).flags |= MAP_UMBRAED;

    if (suppressed(gp))
        env.map_knowledge(gp).flags |= MAP_SUPPRESSED;

    if (silenced(gp))
        env.map_knowledge(gp).flags |= MAP_SILENCED;

    if (liquefied(gp, false))
        env.map_knowledge(gp).flags |= MAP_LIQUEFIED;

    if (orb_haloed(gp))
        env.map_knowledge(gp).flags |= MAP_ORB_HALOED;

    if (is_sanctuary(gp))
    {
        if (testbits(env.pgrid(gp), FPROP_SANCTUARY_1))
            env.map_knowledge(gp).flags |= MAP_SANCTUARY_1;
        else if (testbits(env.pgrid(gp), FPROP_SANCTUARY_2))
            env.map_knowledge(gp).flags |= MAP_SANCTUARY_2;
    }

    if (you.get_beholder(gp))
        env.map_knowledge(gp).flags |= MAP_WITHHELD;

    if (you.get_fearmonger(gp))
        env.map_knowledge(gp).flags |= MAP_WITHHELD;

    if (feat >= DNGN_STONE_STAIRS_DOWN_I
        && feat <= DNGN_ESCAPE_HATCH_UP
        && is_exclude_root(gp))
    {
        env.map_knowledge(gp).flags |= MAP_EXCLUDED_STAIRS;
    }

    if (is_bloodcovered(gp))
        env.map_knowledge(gp).flags |= MAP_BLOODY;

    if (is_moldy(gp))
    {
        env.map_knowledge(gp).flags |= MAP_MOLDY;
        if (glowing_mold(gp))
            env.map_knowledge(gp).flags |= MAP_GLOWING_MOLDY;
    }

    if (slime_wall_neighbour(gp))
        env.map_knowledge(gp).flags |= MAP_CORRODING;

    if (emphasise(gp))
        env.map_knowledge(gp).flags |= MAP_EMPHASIZE;

    // Tell the world first.
    dungeon_events.fire_position_event(DET_PLAYER_IN_LOS, gp);

    if (is_notable_terrain(feat))
        seen_notable_thing(feat, gp);

    dgn_seen_vault_at(gp);
}

static show_item_type _item_to_show_code(const item_def &item)
{
    switch (item.base_type)
    {
    case OBJ_ORBS:       return SHOW_ITEM_ORB;
    case OBJ_WEAPONS:    return SHOW_ITEM_WEAPON;
    case OBJ_MISSILES:   return SHOW_ITEM_MISSILE;
    case OBJ_ARMOUR:     return SHOW_ITEM_ARMOUR;
    case OBJ_WANDS:      return SHOW_ITEM_WAND;
    case OBJ_FOOD:       return SHOW_ITEM_FOOD;
    case OBJ_SCROLLS:    return SHOW_ITEM_SCROLL;
    case OBJ_JEWELLERY:
        return (jewellery_is_amulet(item)? SHOW_ITEM_AMULET : SHOW_ITEM_RING);
    case OBJ_POTIONS:    return SHOW_ITEM_POTION;
    case OBJ_BOOKS:      return SHOW_ITEM_BOOK;
    case OBJ_STAVES:     return SHOW_ITEM_STAVE;
    case OBJ_RODS:       return SHOW_ITEM_STAVE;
    case OBJ_MISCELLANY: return SHOW_ITEM_MISCELLANY;
    case OBJ_CORPSES:    return SHOW_ITEM_CORPSE;
    case OBJ_GOLD:       return SHOW_ITEM_GOLD;
    case OBJ_DETECTED:   return SHOW_ITEM_DETECTED;
    default:             return SHOW_ITEM_ORB; // bad item character
   }
}

static void _update_item_at(const coord_def &gp)
{
    if (!in_bounds(gp))
        return;

    item_def eitem;
    bool more_items = false;

    if (you.see_cell(gp))
    {
        if (you.visible_igrd(gp) != NON_ITEM)
            eitem = mitm[you.visible_igrd(gp)];
        else
            return;

        // monster(mimic)-owned items have link = NON_ITEM+1+midx
        if (eitem.link > NON_ITEM && you.visible_igrd(gp) != NON_ITEM)
            more_items = true;
        else if (eitem.link < NON_ITEM && !crawl_state.game_is_arena())
            more_items = true;
    }
    else
    {
        const std::vector<item_def> stash = item_list_in_stash(gp);
        if (stash.empty())
            return;

        eitem = stash[0];
        if (stash.size() > 1)
            more_items = true;
    }
    env.map_knowledge(gp).set_item(get_item_info(eitem), more_items);
}

static void _update_cloud(int cloudno)
{
    cloud_struct& cloud = env.cloud[cloudno];
    const coord_def gp = cloud.pos;

    unsigned short ch = 0;

    tileidx_t index = 0;
    if (!cloud.tile.empty())
    {
        if (!tile_main_index(cloud.tile.c_str(), &index))
        {
            mprf(MSGCH_ERROR, "Invalid tile requested for cloud: '%s'.", cloud.tile.c_str());
            ch = TILE_ERROR;
        }
        else
        {
            int offset = tile_main_count(index);
            ch = index + offset;
        }
    }

    int dur = cloud.decay/20;
    if (dur < 0)
        dur = 0;
    else if (dur > 3)
        dur = 3;

    cloud_info ci(cloud.type, get_cloud_colour(cloudno), dur, ch, gp);
    env.map_knowledge(gp).set_cloud(ci);
}

static void _check_monster_pos(const monster* mons)
{
    int s = mons->mindex();
    ASSERT(mgrd(mons->pos()) == s);

    // [rob] The following in case asserts aren't enabled.
    // [enne] - It's possible that mgrd and mons->x/y are out of
    // sync because they are updated separately.  If we can see this
    // monster, then make sure that the mgrd is set correctly.
    if (mgrd(mons->pos()) != s)
    {
        // If this mprf triggers for you, please note any special
        // circumstances so we can track down where this is coming
        // from.
        mprf(MSGCH_ERROR, "monster %s (%d) at (%d, %d) was "
             "improperly placed.  Updating mgrd.",
             mons->name(DESC_PLAIN, true).c_str(), s,
             mons->pos().x, mons->pos().y);
        mgrd(mons->pos()) = s;
    }
}

/**
 * Determine if a location is valid to present a { glyph.
 *
 * @param where    The location being queried.
 * @param mons     The moster being mimicked.
 * @returns        True if valid, otherwise False.
*/
static bool _valid_invis_spot(const coord_def &where, const monster* mons)
{
    monster *mons_at = monster_at(where);

    if (mons_at && mons_at != mons)
        return false;

    if (monster_habitable_grid(mons, grd(where)))
        return true;

    return false;
}

static int _hashed_rand(const monster* mons, uint32_t id, uint32_t die)
{
    if (die <= 1)
        return 0;

    struct
    {
        uint32_t mid;
        uint32_t id;
        uint32_t seed;
    } data;
    data.mid = mons->mid;
    data.id  = id;
    data.seed = you.attribute[ATTR_SEEN_INVIS_SEED];

    return hash(&data, sizeof(data)) % die;
}

/**
 * Mark the estimated position of an invisible monster.
 *
 * Marks a spot on the map as possibly containing an unseen monster
 * (showing up as a disturbance in the air), and also places the
 * corresponding tile.
 *
 * @param where    The disturbance's map position.
**/
static void _mark_invisible_monster(const coord_def &where)
{
    env.map_knowledge(where).set_invisible_monster();
}

/**
 * Update map knowledge for monsters
 *
 * This function updates the map_knowledge grid with a monster_info if relevant.
 * If the monster is not currently visible to the player, the map knowledge will
 * be upated with a disturbance if necessary.
 *
 * @param mons    The monster at the relevant location.
**/
static void _update_monster(monster* mons)
{
    _check_monster_pos(mons);

    const coord_def gp = mons->pos();

    if (!mons->visible_to(&you))
    {
        // ripple effect?
        if (grd(gp) == DNGN_SHALLOW_WATER
                && !mons_flies(mons)
                && env.cgrid(gp) == EMPTY_CLOUD
            || is_opaque_cloud(env.cgrid(gp))
                && !mons->submerged()
                && !mons->is_insubstantial())
        {
            env.map_knowledge(gp).set_invisible_monster();
        }

        // Being submerged isnot the same as invisibility.
        if (mons->submerged())
            return;

        if (you.attribute[ATTR_SEEN_INVIS_TURN] != you.num_turns)
        {
            you.attribute[ATTR_SEEN_INVIS_TURN] = you.num_turns;
            you.attribute[ATTR_SEEN_INVIS_SEED] = random_int();
        }

        bool show_location = (mons->friendly()
                              || (mons->constricted_by == MID_PLAYER));

        // maybe show unstealthy invis monsters
        if (show_location
            || _hashed_rand(mons, 0, 7) >= mons->stealth() + 4)
        {
            // We cannot use regular randomness here, otherwise redrawing the
            // screen would give out the real position.  We need to save the
            // seed too -- but it needs to be regenerated every turn.

            // Maybe mark their square.
            if (show_location
                || mons->stealth() <= -2
                || mons->stealth() <= 2 && !_hashed_rand(mons, 1, 4))
            {
                env.map_knowledge(gp).set_invisible_monster();
                if (show_location) // Don't add extra fake trails.
                    return;
            }

            // Exceptionally stealthy monsters have a higher chance of
            // not leaving any other trails.
            if (mons->stealth() == 1 && !_hashed_rand(mons, 2, 3)
                || mons->stealth() == 2 && coinflip()
                || mons->stealth() == 3)
            {
                return;
            }

            // Otherwise just indicate that there's a monster nearby
            coord_def new_pos = gp + Compass[_hashed_rand(mons, 3, 8)];
            if (_valid_invis_spot(new_pos, mons) && _hashed_rand(mons, 4, 2))
                _mark_invisible_monster(new_pos);

            new_pos = gp + Compass[_hashed_rand(mons, 5, 8)];
            if (_valid_invis_spot(new_pos, mons) && !_hashed_rand(mons, 6, 3))
                _mark_invisible_monster(new_pos);
        }

        return;
    }

    mons->ensure_has_client_id();
    monster_info mi(mons);
    env.map_knowledge(gp).set_monster(mi);
}

void show_update_at(const coord_def &gp, bool terrain_only)
{
    if (you.see_cell(gp))
        env.map_knowledge(gp).clear_data();
    else if (!env.map_knowledge(gp).known())
        return;
    else
        env.map_knowledge(gp).clear_monster();

    // The sequence is grid, items, clouds, monsters.
    _update_feat_at(gp);

    // If there's items on the boundary (shop inventory),
    // we don't show them.
    if (!terrain_only && in_bounds(gp))
    {
        monster* mons = monster_at(gp);
        if (mons && mons->alive())
            _update_monster(mons);

        const int cloud = env.cgrid(gp);
        if (cloud != EMPTY_CLOUD && env.cloud[cloud].type != CLOUD_NONE
            && env.cloud[cloud].pos == gp)
        {
            _update_cloud(cloud);
        }

        _update_item_at(gp);
    }

#ifdef USE_TILE
    tile_draw_map_cell(gp, true);
#endif
#ifdef USE_TILE_WEB
    tiles.mark_for_redraw(gp);
#endif
}

void show_init(bool terrain_only)
{
    clear_terrain_visibility();
    for (radius_iterator ri(you.get_los()); ri; ++ri)
        show_update_at(*ri, terrain_only);
}

// Emphasis may change while off-level (precisely, after
// taking stairs and saving the level, when we reach
// the next level). This catches up.
// It should be equivalent to looping over the whole map
// and setting MAP_EMPHASIZE for any coordinate with
// emphasise(p) == true, but we optimise a bit.
void show_update_emphasis()
{
   // The only thing that can change is that previously unknown
   // stairs are now known. (see is_unknown_stair(), emphasise())
   LevelInfo& level_info = travel_cache.get_level_info(level_id::current());
   std::vector<stair_info> stairs = level_info.get_stairs();
   for (unsigned i = 0; i < stairs.size(); ++i)
       if (stairs[i].destination.is_valid())
           env.map_knowledge(stairs[i].position).flags &= ~MAP_EMPHASIZE;
}
