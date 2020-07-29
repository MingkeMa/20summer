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

#include <arpa/inet.h>

using namespace std;

class Buffer
{
    public:
        Buffer(){
        }
        ~Buffer(){cout<<"Delete Buffer"<<endl;}
        int setBuffer(lua_State *L){
            void *p = lua_touserdata(L, -1);
            buffer=(char *)p;
            return 0;
        }
        int getL(lua_State *L){
            int pos = lua_tointeger(L, -1);
            uint32_t res = ntohl(*(uint32_t *)(buffer + pos));
            lua_pushinteger(L, (lua_Integer) res);
            return 1;
        }   
        int getS(lua_State *L){
            int pos = lua_tointeger(L, -1);
            uint16_t res = ntohs(*(uint16_t *)(buffer + pos));
            lua_pushinteger(L, (lua_Integer) res);
            return 1;
        }   
        int setL(lua_State *L){
            int pos = lua_tointeger(L, -1);
            int number = lua_tointeger(L, -2);
            *(uint32_t *)(buffer + pos) = (uint32_t)htonl((uint32_t)number);
            return 0;
        }   
        int setS(lua_State *L){
            int pos = lua_tointeger(L, -1);
            int number = lua_tointeger(L, -2);
            *(uint16_t *)(buffer + pos) = (uint16_t)htons((uint16_t)number);
            return 0;
        }           
        int print(lua_State *L) 
        {   
            printf("what in buffer is: %s\n",buffer);
            return 0;
        }   
    public:
        DECLARE_LUNAR_CLASS(Buffer);
    public:
        char *buffer;
};

EXPORT_LUNAR_FUNCTION_BEGIN(Buffer)
EXPORT_LUNAR_FUNCTION(Buffer, setBuffer)
EXPORT_LUNAR_FUNCTION(Buffer, getL)
EXPORT_LUNAR_FUNCTION(Buffer, getS)
EXPORT_LUNAR_FUNCTION(Buffer, setL)
EXPORT_LUNAR_FUNCTION(Buffer, setS)
EXPORT_LUNAR_FUNCTION(Buffer, print)
EXPORT_LUNAR_FUNCTION_END(Buffer)
