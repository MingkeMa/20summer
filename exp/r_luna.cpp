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
// #include "lunar.h"

using namespace std;

class Car 
{
    public:
        Car(){}
        ~Car(){cout<<"Delete Car,Length:"<<length<<" Width:"<<width<<endl;}

        int getLength(lua_State *L){
            lua_pushinteger(L, length);
            return 1;
        }   
        int getWidth(lua_State *L){
            lua_pushinteger(L, width);
            return 1;
        }   
        int setLength(lua_State *L) 
        {   
            int val = lua_tointeger(L, -1);
            length = val;
            return 0;
        }   
        int setWidth(lua_State *L) 
        {   
            int val = lua_tointeger(L, -1);
            width = val;
            return 0;
        }   
        int print(lua_State *L) 
        {   
            cout <<"print length:"<<length<<" width:"<<width<<endl;
        }   
    public:
        DECLARE_LUNA_CLASS(Car);
    public:
        int length = 100;
        int width = 200;
};

EXPORT_LUNA_FUNCTION_BEGIN(Car)
EXPORT_LUNA_FUNCTION(Car, getLength)
EXPORT_LUNA_FUNCTION(Car, getWidth)
EXPORT_LUNA_FUNCTION(Car, setLength)
EXPORT_LUNA_FUNCTION(Car, setWidth)
EXPORT_LUNA_FUNCTION(Car, print)
EXPORT_LUNA_FUNCTION_END(Car)

int main(int argc, char* argv[])
{
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    luaL_dofile(L, "tree.lua");

    //use luna template and bind to object
    luna<Car>::regist(L);
    // print_stack(L);

    luaL_dofile(L, "r_luna.lua");
    print_stack(L);
    lua_settop(L, 0);
    cout << endl;

    lua_close(L);
    return 0;
}