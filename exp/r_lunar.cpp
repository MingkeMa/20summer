#include <iostream>
#include <cstring>
#include <stdlib.h>
extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}
#include "comm.h"
#include "luna.h"
#include "lunar.h"

using namespace std;

class Mobile
{
    public:
        Mobile(){}
        ~Mobile(){cout<<"Delete Mobile,Ver:"<<version<<" Price:"<<price<<endl;}

        int getVersion(lua_State *L){
            lua_pushinteger(L, version);
            return 1;
        }   
        int getPrice(lua_State *L){
            lua_pushinteger(L, price);
            return 1;
        }   
        int setVersion(lua_State *L) 
        {   
            int val = lua_tointeger(L, -1);
            version = val;
            return 0;
        }   
        int setPrice(lua_State *L) 
        {   
            int val = lua_tointeger(L, -1);
            price = val;
            return 0;
        }   
        int print(lua_State *L) 
        {   
            cout <<"print version:"<<version<<" price:"<<price<<endl;
        }   
    public:
        DECLARE_LUNAR_CLASS(Mobile);
    public:
        int version = 100;
        int price = 200;
};

EXPORT_LUNAR_FUNCTION_BEGIN(Mobile)
EXPORT_LUNAR_FUNCTION(Mobile, getVersion)
EXPORT_LUNAR_FUNCTION(Mobile, getPrice)
EXPORT_LUNAR_FUNCTION(Mobile, setVersion)
EXPORT_LUNAR_FUNCTION(Mobile, setPrice)
EXPORT_LUNAR_FUNCTION(Mobile, print)
EXPORT_LUNAR_MEMBER_INT(Mobile, version)
EXPORT_LUNAR_MEMBER_INT(Mobile, price)
EXPORT_LUNAR_FUNCTION_END(Mobile)

int main(int argc, char* argv[])
{
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    luaL_dofile(L, "tree.lua");

    //use lunar template and bind to object 
    lunar<Mobile>::regist(L);

    luaL_dofile(L, "r_lunar.lua");
    print_stack(L);
    lua_settop(L, 0);
    cout<<endl;

    lua_close(L);
    return 0;
}
