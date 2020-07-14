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

class Shape;
typedef int (Shape::*ShapePF)(lua_State* L);

struct method
{
    const char *name;
    ShapePF pf;
};

class Shape{
    public:
        Shape(int i, int j):x(i),y(j){}
        int setX(lua_State* L){
            int val = lua_tointeger(L, -1);
            x = val;
            return 0;
        }
        int setY(lua_State* L){
            int val = lua_tointeger(L, -1);
            y = val;
            return 0;
        }
        int getX(lua_State* L){
            lua_pushinteger(L, x);
            return 1;
        }
        int getY(lua_State* L){
            lua_pushinteger(L, y);
            return 1;
        }
        int print(lua_State* L){
            cout<<"x: " << x << " y: " << y << endl;
            return 0;
        }
        int autoDelete(lua_State* L){
            cout<<"auto gc:" << x <<", "<< y << endl;
            return 0;
        }
    public:
        static char name[32];
        static method methods[100];
    public:
        int x = 10;
        int y = 11;
};

char Shape::name[] = "Shape";
method Shape::methods[] = {
    {"setX", &Shape::setX},
    {"setY", &Shape::setY},
    {"getX", &Shape::getX},
    {"getY", &Shape::getY},
    {"print", &Shape::print},
    {"delete", &Shape::autoDelete},
    {NULL, NULL},
};

int delete_shape(lua_State *L)
{
    Shape** p = (Shape**)lua_touserdata(L, 1);
    (*p)->autoDelete(L);
    return 0;
}

int call_shape(lua_State *L)
{
    Shape** p = (Shape**)lua_touserdata(L, 1);
    lua_remove(L, 1);

    method* v = (method*)lua_topointer(L, lua_upvalueindex(1));
    cout<<"call_shape:"<<v->name<<endl;

    return ((*p)->*(v->pf))(L);
}

int n_call_shape(lua_State *L)
{
    method* v = (method*)lua_topointer(L, lua_upvalueindex(1));
    cout<<"n_call_shape:"<<v->name<<endl;

    Shape* p = (Shape*)lua_topointer(L, lua_upvalueindex(2));

    return ((p)->*(v->pf))(L);
}

int create_shape(lua_State *L)
{
    lua_remove(L, 1);
    int x = lua_tointeger(L, 1);
    int y = lua_tointeger(L, 2);
    Shape** p = (Shape**)lua_newuserdata(L, sizeof(Shape*));
    *p = new Shape(x, y);

    luaL_getmetatable(L, "MetaShape");
    lua_setmetatable(L, -2);

    luaL_getmetatable(L, "MetaShape");
    for (method* l = Shape::methods; l->name; l++)
    {
        char s[32] = "n_";
        lua_pushlightuserdata(L,(void*)l);
        lua_pushlightuserdata(L,(void*)*p);
        lua_pushcclosure(L, n_call_shape, 2); 
        lua_setfield(L, -2, strcat(s, l->name));
    }   

    lua_pop(L, 1); 

    return 1;
}

int lua_openShape(lua_State *L) 
{
    //原表Shape
    if (luaL_newmetatable(L, "MetaShape"))
    {   
        //注册Shape到全局
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setglobal(L, "Shape");

        //设置Shape的原表，主要是__call，使其看起来更像C++初始化
        lua_newtable(L);
        lua_pushcfunction(L, create_shape);
        lua_setfield(L, -2, "__call");
        lua_setmetatable(L, -2);
        lua_pop(L, 1); //把全局Shape pop 出去，这时stack只有MetaShape

        //设置Shape的方法列表
        for(method* l = Shape::methods; l->name; l++)
        {   
            lua_pushlightuserdata(L,(void*)l);
            lua_pushcclosure(L, call_shape, 1); 
            lua_setfield(L, -2, l->name);
        }   

        //设置MetaShape指向自己
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, delete_shape);
        lua_setfield(L, -2, "__gc");
    }   
}

int main(int argc, char* argv[])
{
    cout<<"hello world"<<endl;
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    luaL_dofile(L, “tree.lua”);

    lua_openShape(L);
    print_stack(L);

    luaL_dofile(L, "r_oo2.lua");
    print_stack(L);
    lua_settop(L, 0); 
    cout << endl;
}
