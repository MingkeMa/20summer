#include <iostream>
#include <cstring>
#include <stdlib.h>
extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

static int l_split (lua_State *L) {
    const char *s = luaL_checkstring(L, 1);
    const char *sep = luaL_checkstring(L, 2);
    const char *e;
    int i = 1;

    lua_newtable(L);  /* result */

    while ((e = strchr(s, *sep)) != NULL) {
        lua_pushlstring(L, s, e-s);  /* push substring */
        lua_rawseti(L, -2, i++);
        s = e + 1;  /* skip separator */
    }

    /* push last substring */
    lua_pushstring(L, s);
    lua_rawseti(L, -2, i);

    return 1; 
}

static int l_time_gap (lua_State *L) {
    const char *l = luaL_checkstring(L, 1);
    const char *r = luaL_checkstring(L, 2);
    char *el,*er;
    int templ, tempr;

    int res=0; /* result */

    templ = strtol (l,&el,10);
    tempr = strtol (r,&er,10);
    res+=0; /* omit year */

    templ = strtol (el+1,&el,10);
    tempr = strtol (er+1,&er,10);
    res+=0; /* omit month */

    templ = strtol (el+1,&el,10);
    tempr = strtol (er+1,&er,10);
    res+=(tempr-templ)*86400; /* day */

    templ = strtol (el+1,&el,10);
    tempr = strtol (er+1,&er,10);
    res+=(tempr-templ)*3600; /* hour */

    templ = strtol (el+1,&el,10);
    tempr = strtol (er+1,&er,10);
    res+=(tempr-templ)*60; /* min */

    templ = strtol (el+1,&el,10);
    tempr = strtol (er+1,&er,10);
    res+=tempr-templ; /* sec */   

    lua_pushnumber(L,res); 

    return 1;  
}

static const struct luaL_Reg mylib [] = {
    {"split", l_split},
    {"time_gap", l_time_gap},
    {NULL, NULL}  /* sentinel */
};

extern "C" 
int luaopen_mylib (lua_State *L) {
    luaL_newlib(L, mylib);
    return 1;
}

