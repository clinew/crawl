/*
 *  File:       mapmark.cc
 *  Summary:    Level markers (annotations).
 *  Created by: dshaligram on Sat Jul 21 12:17:29 2007 UTC
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *  
 */

#include "AppHdr.h"
#include "mapmark.h"

#include "clua.h"
#include "direct.h"
#include "libutil.h"
#include "luadgn.h"
#include "stuff.h"
#include "tags.h"
#include "view.h"

////////////////////////////////////////////////////////////////////////
// Dungeon markers

map_marker::marker_reader map_marker::readers[NUM_MAP_MARKER_TYPES] =
{
    &map_feature_marker::read,
    &map_lua_marker::read,
    &map_corruption_marker::read,
    &map_wiz_props_marker::read,
};

map_marker::marker_parser map_marker::parsers[NUM_MAP_MARKER_TYPES] =
{
    &map_feature_marker::parse,
    &map_lua_marker::parse,
    NULL,
    NULL
};

map_marker::map_marker(map_marker_type t, const coord_def &p)
    : pos(p), type(t)
{
}

map_marker::~map_marker()
{
}

void map_marker::activate(bool)
{
}

void map_marker::write(tagHeader &outf) const
{
    marshallShort(outf, type);
    marshallCoord(outf, pos);
}

void map_marker::read(tagHeader &inf)
{
    // Don't read type! The type has to be read by someone who knows how
    // to look up the unmarshall function.
    unmarshallCoord(inf, pos);
}

std::string map_marker::feature_description() const
{
    return ("");
}

std::string map_marker::property(const std::string &pname) const
{
    return ("");
}

map_marker *map_marker::read_marker(tagHeader &inf)
{
    const map_marker_type type =
        static_cast<map_marker_type>(unmarshallShort(inf));
    return readers[type]? (*readers[type])(inf, type) : NULL;
}

map_marker *map_marker::parse_marker(
    const std::string &s, const std::string &ctx) throw (std::string)
{
    for (int i = 0; i < NUM_MAP_MARKER_TYPES; ++i)
    {
        if (parsers[i])
        {
            if (map_marker *m = parsers[i](s, ctx))
                return (m);
        }
    }
    return (NULL);
}

////////////////////////////////////////////////////////////////////////////
// map_feature_marker

map_feature_marker::map_feature_marker(
    const coord_def &p,
    dungeon_feature_type _feat)
    : map_marker(MAT_FEATURE, p), feat(_feat)
{
}

map_feature_marker::map_feature_marker(
    const map_feature_marker &other)
    : map_marker(MAT_FEATURE, other.pos), feat(other.feat)
{
}

void map_feature_marker::write(tagHeader &outf) const
{
    this->map_marker::write(outf);
    marshallShort(outf, feat);
}

void map_feature_marker::read(tagHeader &inf)
{
    map_marker::read(inf);
    feat = static_cast<dungeon_feature_type>(unmarshallShort(inf));
}

map_marker *map_feature_marker::clone() const
{
    return new map_feature_marker(pos, feat);
}

map_marker *map_feature_marker::read(tagHeader &inf, map_marker_type)
{
    map_marker *mapf = new map_feature_marker();
    mapf->read(inf);
    return (mapf);
}

map_marker *map_feature_marker::parse(
    const std::string &s, const std::string &) throw (std::string)
{
    if (s.find("feat:") != 0)
        return (NULL);
    std::string raw = s;
    strip_tag(raw, "feat:", true);

    const dungeon_feature_type ft = dungeon_feature_by_name(raw);
    if (ft == DNGN_UNSEEN)
        throw make_stringf("Bad feature marker: %s (unknown feature '%s')",
                           s.c_str(), raw.c_str());
    return new map_feature_marker(coord_def(0, 0), ft);
}

std::string map_feature_marker::debug_describe() const
{
    return make_stringf("feature (%s)", dungeon_feature_name(feat));
}

////////////////////////////////////////////////////////////////////////////
// map_lua_marker

map_lua_marker::map_lua_marker()
    : map_marker(MAT_LUA_MARKER, coord_def()), initialised(false)
{
}

map_lua_marker::map_lua_marker(const std::string &s, const std::string &,
                               bool mapdef_marker)
    : map_marker(MAT_LUA_MARKER, coord_def()), initialised(false)
{
    lua_stack_cleaner clean(dlua);
    if (mapdef_marker)
    {
        if (dlua.loadstring(("return " + s).c_str(), "lua_marker"))
            mprf(MSGCH_WARN, "lua_marker load error: %s", dlua.error.c_str());
        if (!dlua.callfn("dgn_run_map", 1, 1))
            mprf(MSGCH_WARN, "lua_marker exec error: %s", dlua.error.c_str());
    }
    else
    {
        if (dlua.execstring(("return " + s).c_str(), "lua_marker_mapless", 1))
            mprf(MSGCH_WARN, "lua_marker_mapless exec error: %s",
                 dlua.error.c_str());
    }
    check_register_table();
}

map_lua_marker::~map_lua_marker()
{
    // Remove the Lua marker table from the registry.
    if (initialised)
    {
        lua_pushlightuserdata(dlua, this);
        lua_pushnil(dlua);
        lua_settable(dlua, LUA_REGISTRYINDEX);
    }
}

map_marker *map_lua_marker::clone() const
{
    map_lua_marker *copy = new map_lua_marker();
    copy->pos = pos;
    if (get_table())
        copy->check_register_table();
    return copy;
}

void map_lua_marker::check_register_table()
{
    if (!lua_istable(dlua, -1))
    {
        mprf(MSGCH_WARN, "lua_marker: Expected table, didn't get it.");
        initialised = false;
        return;
    }

    // Got a table. Save it in the registry.

    // Key is this.
    lua_pushlightuserdata(dlua, this);
    // Move key before value.
    lua_insert(dlua, -2);
    lua_settable(dlua, LUA_REGISTRYINDEX);

    initialised = true;
}

bool map_lua_marker::get_table() const
{
    // First save the unmarshall Lua function.
    lua_pushlightuserdata(dlua, const_cast<map_lua_marker*>(this));
    lua_gettable(dlua, LUA_REGISTRYINDEX);
    return (lua_istable(dlua, -1));
}

void map_lua_marker::write(tagHeader &th) const
{
    map_marker::write(th);
    
    lua_stack_cleaner clean(dlua);
    bool init = initialised;
    if (!get_table())
    {
        mprf(MSGCH_WARN, "Couldn't find table.");
        init = false;
    }
    
    marshallByte(th, init);
    if (!init)
        return;

    // Call dlua_marker_function(table, 'read')
    lua_pushstring(dlua, "read");
    if (!dlua.callfn("dlua_marker_function", 2, 1))
        end(1, false, "lua_marker: write error: %s", dlua.error.c_str());

    // Right, what's on top should be a function. Save it.
    dlua_chunk reader(dlua);
    if (!reader.error.empty())
        end(1, false, "lua_marker: couldn't save read function: %s",
            reader.error.c_str());

    marshallString(th, reader.compiled_chunk());

    // Okay, saved the reader. Now ask the writer to do its thing.

    // Call: dlua_marker_method(table, fname, marker)
    get_table();
    lua_pushstring(dlua, "write");
    lua_pushlightuserdata(dlua, const_cast<map_lua_marker*>(this));
    lua_pushlightuserdata(dlua, &th);

    if (!dlua.callfn("dlua_marker_method", 4))
        end(1, false, "lua_marker::write error: %s", dlua.error.c_str());
}

void map_lua_marker::read(tagHeader &th)
{
    map_marker::read(th);
    
    if (!(initialised = unmarshallByte(th)))
        return;

    lua_stack_cleaner cln(dlua);
    // Read the Lua chunk we saved.
    const std::string compiled = unmarshallString(th, LUA_CHUNK_MAX_SIZE);

    dlua_chunk chunk = dlua_chunk::precompiled(compiled);
    if (chunk.load(dlua))
        end(1, false, "lua_marker::read error: %s", chunk.error.c_str());
    dlua_push_userdata(dlua, this, MAPMARK_METATABLE);
    lua_pushlightuserdata(dlua, &th);
    if (!dlua.callfn("dlua_marker_read", 3, 1))
        end(1, false, "lua_marker::read error: %s", dlua.error.c_str());

    // Right, what's on top had better be a table.
    check_register_table();
}

map_marker *map_lua_marker::read(tagHeader &th, map_marker_type)
{
    map_marker *marker = new map_lua_marker;
    marker->read(th);
    return (marker);
}

void map_lua_marker::push_fn_args(const char *fn) const
{
    get_table();
    lua_pushstring(dlua, fn);
    dlua_push_userdata(dlua, this, MAPMARK_METATABLE);
}

bool map_lua_marker::callfn(const char *fn, bool warn_err, int args) const
{
    if (args == -1)
    {
        const int top = lua_gettop(dlua);
        push_fn_args(fn);
        args = lua_gettop(dlua) - top;
    }
    const bool res = dlua.callfn("dlua_marker_method", args, 1);
    if (!res && warn_err)
        mprf(MSGCH_WARN, "mlua error: %s", dlua.error.c_str());
    return (res);
}

void map_lua_marker::activate(bool verbose)
{
    lua_stack_cleaner clean(dlua);
    push_fn_args("activate");
    lua_pushboolean(dlua, verbose);
    callfn("activate", true, 4);
}

void map_lua_marker::notify_dgn_event(const dgn_event &e)
{
    lua_stack_cleaner clean(dlua);
    push_fn_args("event");
    clua_push_dgn_event(dlua, &e);
    if (!dlua.callfn("dlua_marker_method", 4, 0))
        mprf(MSGCH_WARN, "notify_dgn_event: Lua error: %s",
             dlua.error.c_str());
}

std::string map_lua_marker::call_str_fn(const char *fn) const
{
    lua_stack_cleaner cln(dlua);
    if (!callfn(fn))
        return make_stringf("error (%s): %s", fn, dlua.error.c_str());

    std::string result;
    if (lua_isstring(dlua, -1))
        result = lua_tostring(dlua, -1);
    return (result);
}

std::string map_lua_marker::debug_describe() const
{
    return (call_str_fn("describe"));
}

std::string map_lua_marker::feature_description() const
{
    return (call_str_fn("feature_description"));
}

std::string map_lua_marker::property(const std::string &pname) const
{
    lua_stack_cleaner cln(dlua);
    push_fn_args("property");
    lua_pushstring(dlua, pname.c_str());
    if (!callfn("property", false, 4))
        return make_stringf("error (prop:%s): %s",
                            pname.c_str(), dlua.error.c_str());
    std::string result;
    if (lua_isstring(dlua, -1))
        result = lua_tostring(dlua, -1);
    return (result);    
}

map_marker *map_lua_marker::parse(
    const std::string &s, const std::string &ctx) throw (std::string)
{
    std::string raw           = s;
    bool        mapdef_marker = true;

    if (s.find("lua:") == 0)
        strip_tag(raw, "lua:", true);
    else if (s.find("lua_mapless:") == 0)
    {
        strip_tag(raw, "lua_mapless:", true);
        mapdef_marker = false;
    }
    else
        return (NULL);

    map_lua_marker *mark = new map_lua_marker(raw, ctx, mapdef_marker);
    if (!mark->initialised)
    {
        delete mark;
        throw make_stringf("Unable to initialise Lua marker from '%s'",
                           raw.c_str());
    }
    return (mark);
}

//////////////////////////////////////////////////////////////////////////
// map_corruption_marker

map_corruption_marker::map_corruption_marker(const coord_def &p,
                                             int dur)
    : map_marker(MAT_CORRUPTION_NEXUS, p), duration(dur), radius(0)
{
}

void map_corruption_marker::write(tagHeader &out) const
{
    map_marker::write(out);
    marshallShort(out, duration);
    marshallShort(out, radius);
}

void map_corruption_marker::read(tagHeader &in)
{
    map_marker::read(in);
    duration = unmarshallShort(in);
    radius   = unmarshallShort(in);
}

map_marker *map_corruption_marker::read(tagHeader &th, map_marker_type)
{
    map_corruption_marker *mc = new map_corruption_marker();
    mc->read(th);
    return (mc);
}

map_marker *map_corruption_marker::clone() const
{
    map_corruption_marker *mark = new map_corruption_marker(pos, duration);
    mark->radius = radius;
    return (mark);
}

std::string map_corruption_marker::debug_describe() const
{
    return make_stringf("Lugonu corrupt (%d)", duration);
}

////////////////////////////////////////////////////////////////////////////
// map_feature_marker

map_wiz_props_marker::map_wiz_props_marker(
    const coord_def &p)
    : map_marker(MAT_WIZ_PROPS, p)
{
}

map_wiz_props_marker::map_wiz_props_marker(
    const map_wiz_props_marker &other)
    : map_marker(MAT_WIZ_PROPS, other.pos), properties(other.properties)
{
}

void map_wiz_props_marker::write(tagHeader &outf) const
{
    this->map_marker::write(outf);
    marshallShort(outf, properties.size());
    for (std::map<std::string, std::string>::const_iterator i =
             properties.begin(); i != properties.end(); ++i)
    {
        marshallString(outf, i->first);
        marshallString(outf, i->second);
    }
}

void map_wiz_props_marker::read(tagHeader &inf)
{
    map_marker::read(inf);

    short numPairs = unmarshallShort(inf);
    for (short i = 0; i < numPairs; i++)
    {
        const std::string key = unmarshallString(inf);
        const std::string val = unmarshallString(inf);

        set_property(key, val);
    }
}

std::string map_wiz_props_marker::feature_description() const
{
    return property("desc");
}

std::string map_wiz_props_marker::property(const std::string &pname) const
{
    std::map<std::string, std::string>::const_iterator
        i = properties.find(pname);

    if (i != properties.end())
        return (i->second);
    else
        return ("");
}

std::string map_wiz_props_marker::set_property(const std::string &key,
                                               const std::string &val)
{
    std::string old_val = properties[key];
    properties[key] = val;
    return (old_val);
}

map_marker *map_wiz_props_marker::clone() const
{
    return new map_wiz_props_marker(*this);
}

map_marker *map_wiz_props_marker::read(tagHeader &inf, map_marker_type)
{
    map_marker *mapf = new map_wiz_props_marker();
    mapf->read(inf);
    return (mapf);
}

map_marker *map_wiz_props_marker::parse(
    const std::string &s, const std::string &) throw (std::string)
{
    throw make_stringf("map_wiz_props_marker::parse() not implemented");
}

std::string map_wiz_props_marker::debug_describe() const
{
    return make_stringf("wizard props: ") + feature_description();
}

//////////////////////////////////////////////////////////////////////////
// Map markers in env.

map_markers::map_markers() : markers()
{
}

map_markers::map_markers(const map_markers &c) : markers()
{
    init_from(c);
}

map_markers &map_markers::operator = (const map_markers &c)
{
    if (this != &c)
    {
        clear();
        init_from(c);
    }
    return (*this);
}

map_markers::~map_markers()
{
    clear();
}

void map_markers::init_from(const map_markers &c)
{
    for (dgn_marker_map::const_iterator i = c.markers.begin();
         i != c.markers.end(); ++i)
    {
        add( i->second->clone() );
    }
}

void map_markers::activate_all(bool verbose)
{
    for (dgn_marker_map::iterator i = markers.begin();
         i != markers.end(); ++i)
    {
        i->second->activate(verbose);
    }
}

void map_markers::add(map_marker *marker)
{
    markers.insert(dgn_pos_marker(marker->pos, marker));
}

void map_markers::remove(map_marker *marker)
{
    std::pair<dgn_marker_map::iterator, dgn_marker_map::iterator>
        els = markers.equal_range(marker->pos);
    for (dgn_marker_map::iterator i = els.first; i != els.second; ++i)
    {
        if (i->second == marker)
        {
            markers.erase(i);
            break;
        }
    }
    delete marker;
}

void map_markers::remove_markers_at(const coord_def &c,
                                    map_marker_type type)
{
    std::pair<dgn_marker_map::iterator, dgn_marker_map::iterator>
        els = markers.equal_range(c);
    for (dgn_marker_map::iterator i = els.first; i != els.second; )
    {
        dgn_marker_map::iterator todel = i++;
        if (type == MAT_ANY || todel->second->get_type() == type)
        {
            delete todel->second;
            markers.erase(todel);
        }
    }
}

map_marker *map_markers::find(const coord_def &c, map_marker_type type)
{
    std::pair<dgn_marker_map::const_iterator, dgn_marker_map::const_iterator>
        els = markers.equal_range(c);
    for (dgn_marker_map::const_iterator i = els.first; i != els.second; ++i)
        if (type == MAT_ANY || i->second->get_type() == type)
            return (i->second);
    return (NULL);
}

map_marker *map_markers::find(map_marker_type type)
{
    for (dgn_marker_map::const_iterator i = markers.begin();
         i != markers.end(); ++i)
    {
        if (type == MAT_ANY || i->second->get_type() == type)
            return (i->second);
    }
    return (NULL);
}

void map_markers::move(const coord_def &from, const coord_def &to)
{
    std::pair<dgn_marker_map::iterator, dgn_marker_map::iterator>
        els = markers.equal_range(from);

    std::list<map_marker*> tmarkers;
    for (dgn_marker_map::iterator i = els.first; i != els.second; )
    {
        dgn_marker_map::iterator curr = i++;
        tmarkers.push_back(curr->second);
        markers.erase(curr);
    }

    for (std::list<map_marker*>::iterator i = tmarkers.begin();
         i != tmarkers.end(); ++i)
    {
        (*i)->pos = to;
        add(*i);
    }
}

std::vector<map_marker*> map_markers::get_all(map_marker_type mat)
{
    std::vector<map_marker*> rmarkers;
    for (dgn_marker_map::const_iterator i = markers.begin();
         i != markers.end(); ++i)
    {
        if (mat == MAT_ANY || i->second->get_type() == mat)
            rmarkers.push_back(i->second);
    }
    return (rmarkers);    
}

std::vector<map_marker*> map_markers::get_all(const std::string &key,
                                              const std::string &val)
{
    std::vector<map_marker*> rmarkers;

    for (dgn_marker_map::const_iterator i = markers.begin();
         i != markers.end(); ++i)
    {
        map_marker*       marker = i->second;
        const std::string prop   = marker->property(key);

        if ((val == "" && !prop.empty()) || (val != "" && val == prop))
            rmarkers.push_back(marker);
    }

    return (rmarkers);    
}

std::vector<map_marker*> map_markers::get_markers_at(const coord_def &c)
{
    std::pair<dgn_marker_map::const_iterator, dgn_marker_map::const_iterator>
        els = markers.equal_range(c);
    std::vector<map_marker*> rmarkers;
    for (dgn_marker_map::const_iterator i = els.first; i != els.second; ++i)
        rmarkers.push_back(i->second);
    return (rmarkers);
}

std::string map_markers::property_at(const coord_def &c, map_marker_type type,
                                     const std::string &key)
{
    std::pair<dgn_marker_map::const_iterator, dgn_marker_map::const_iterator>
        els = markers.equal_range(c);
    for (dgn_marker_map::const_iterator i = els.first; i != els.second; ++i)
    {
        const std::string prop = i->second->property(key);
        if (!prop.empty())
            return (prop);
    }
    return ("");
}

void map_markers::clear()
{
    for (dgn_marker_map::iterator i = markers.begin();
         i != markers.end(); ++i)
        delete i->second;
    markers.clear();
}

void map_markers::write(tagHeader &th) const
{
    // how many markers
    marshallShort(th, markers.size());
    for (dgn_marker_map::const_iterator i = markers.begin();
         i != markers.end(); ++i)
    {
        i->second->write(th);
    }
}

void map_markers::read(tagHeader &th)
{
    clear();
    const int nmarkers = unmarshallShort(th);
    for (int i = 0; i < nmarkers; ++i)
        if (map_marker *mark = map_marker::read_marker(th))
            add(mark);
}
