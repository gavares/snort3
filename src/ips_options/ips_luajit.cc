/*
** Copyright (C) 2014 Cisco and/or its affiliates. All rights reserved.
** Copyright (C) 2013-2013 Sourcefire, Inc.
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License Version 2 as
** published by the Free Software Foundation.  You may not use, modify or
** distribute this program under any other version of the GNU General
** Public License.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "ips_luajit.h"

#include <lua.hpp>

#include "managers/ips_manager.h"
#include "hash/sfhashfcn.h"
#include "parser/parser.h"
#include "detection/detection_util.h"

using namespace std;

static const char* opt_init = "init";
static const char* opt_eval = "eval";

// FIXIT add profiling - note will be aggregate for
// all luajit scripts

//-------------------------------------------------------------------------
// luajit ffi stuff
//-------------------------------------------------------------------------

enum BufferType
{
    BT_PAYLOAD,
    BT_URI
};

struct Buffer
{
    enum BufferType type;
    const uint8_t* data;
    uint32_t len;
};

extern "C" {
// ensure Lua can link with this
const Buffer* get_buffer(BufferType);
}

static THREAD_LOCAL Packet* packet;

//namespace snort_ffi
//{
const Buffer* get_buffer(BufferType type)
{
    static Buffer buf;
    buf.type = type;

    if ( type == BT_PAYLOAD && packet )
    {
        buf.data = packet->data;
        buf.len = packet->dsize;
        return &buf;
    }
    else if ( type != BT_PAYLOAD )
    {
        const HttpBuffer* p = GetHttpBuffer((HTTP_BUFFER)type);

        if ( p )
        {
            buf.data = p->buf;
            buf.len = p->length;
            return &buf;
        }
    }
    buf.data = (uint8_t*)"";
    buf.len = 0;

    return &buf;
}
//};

//-------------------------------------------------------------------------
// lua implementation stuff
//-------------------------------------------------------------------------

struct Loader
{
    Loader(string& s) : chunk(s) { done = false; };
    string& chunk;
    bool done;
};

const char* load(lua_State*, void* ud, size_t* size)
{
    Loader* ldr = (Loader*)ud;

    if ( ldr->done )
    {
        *size = 0;
        return nullptr;
    }
    ldr->done = true;
    *size = ldr->chunk.size();
    return ldr->chunk.c_str();
}

static void init_lua(
    lua_State*& L, string& chunk, const char* name, const char* args)
{
    L = luaL_newstate();
    luaL_openlibs(L);

    Loader ldr(chunk);

    // first load the chunk
    if ( lua_load(L, load, &ldr, name) )
        ParseError("%s luajit failed to load chunk %s", 
            name, lua_tostring(L, -1));

    // now exec the chunk to define functions etc in L
    if ( lua_pcall(L, 0, 0, 0) )
        ParseError("%s luajit failed to init chunk %s", 
            name, lua_tostring(L, -1));

    // create an args table with any rule options
    string table("args = {");
    //if ( args && strlen(args) > 2)
    //    table.append(args+1, strlen(args)-2);
    if ( args )
        table += args;
    table += "}";

    // load the args table 
    if ( luaL_loadstring(L, table.c_str()) )
        ParseError("%s luajit failed to load args %s", 
            name, lua_tostring(L, -1));

    // exec the args table to define it in L
    if ( lua_pcall(L, 0, 0, 0) )
        ParseError("%s luajit failed to init args %s", 
            name, lua_tostring(L, -1));

    // exec the init func if defined
    lua_getglobal(L, opt_init);

    if ( !lua_isfunction(L, -1) )
        return;

    if ( lua_pcall(L, 0, 1, 0) )
    {
        const char* err = lua_tostring(L, -1);
        ParseError("%s %s", name, err);
    }
    // string is an error message
    if ( lua_isstring(L, -1) )
    {
        const char* err = lua_tostring(L, -1);
        ParseError("%s %s", name, err);
    }
    // bool is the result
    if ( !lua_toboolean(L, -1) )
    {
        ParseError("%s init() returned false", name);
    }
    // initialization complete
    lua_pop(L, 1);
}

static void term_lua(lua_State*& L)
{
    lua_close(L);
}

//-------------------------------------------------------------------------
// class stuff
//-------------------------------------------------------------------------

LuaJITOption::LuaJITOption(
    const char* name, string& chunk, const char* args)
    : IpsOption(name)
{
    unsigned max = get_instance_max();

    lua = new lua_State*[max];

    for ( unsigned i = 0; i < max; ++i )
        init_lua(lua[i], chunk, name, args);

    config = args ? args : "";
}

LuaJITOption::~LuaJITOption()
{
    unsigned max = get_instance_max();

    for ( unsigned i = 0; i < max; ++i )
        term_lua(lua[i]);

    delete[] lua;
}

uint32_t LuaJITOption::hash() const
{
    uint32_t a = 0, b = 0, c = 0;
    mix_str(a,b,c,get_name());
    mix_str(a,b,c,config.c_str());
    final(a,b,c);
    return c;
}

bool LuaJITOption::operator==(const IpsOption& ips) const
{
    LuaJITOption& rhs = (LuaJITOption&)ips;

    if ( strcmp(get_name(), rhs.get_name()) )
        return false;

    if ( config != rhs.config )
        return false;

    return true;
}

int LuaJITOption::eval(Packet* p)
{
    packet = p;

    lua_State* L = lua[get_instance_id()];
    lua_getglobal(L, opt_eval);

    if ( lua_pcall(L, 0, 1, 0) )
    {
        const char* err = lua_tostring(L, -1);
        ErrorMessage("%s\n", err);
        return DETECTION_OPTION_NO_MATCH;
    }
    bool result = lua_toboolean(L, -1);
    lua_pop(L, 1);

    return result ? DETECTION_OPTION_MATCH : DETECTION_OPTION_NO_MATCH;
}
