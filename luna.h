#include <iostream>
#include <cstring>
extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

using namespace std;

#define DECLARE_LUNA_CLASS(obj) \
    static const char *name;\
    static luna<obj>::TMethod methods[];

#define EXPORT_LUNA_FUNCTION_BEGIN(obj) \
    const char* obj::name = #obj;\
    luna<obj>::TMethod obj::methods[] = {

#define EXPORT_LUNA_MEMBER_INT(obj, member) \
    {#member, nullptr},

#define EXPORT_LUNA_FUNCTION(obj, func) \
    {#func, &obj::func},

#define EXPORT_LUNA_FUNCTION_END(obj) \
    {nullptr, nullptr}\
    };

template<typename T>
class luna
{
    public:
        typedef struct {T* _u;} TObject;
        typedef int (T::*TPfn)(lua_State* L); 
        typedef struct {const char* name; TPfn pf;} TMethod;
    public:
        static int regist(lua_State* L); 
        static int create(lua_State* L); 
        static int call(lua_State* L); 
        static int gc(lua_State* L); 
};

template<typename T>
int luna<T>::regist(lua_State* L)
{
    //原表Shape
    if (luaL_newmetatable(L, T::name))
    {   
        //注册Shape到全局
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setglobal(L, T::name);

        //设置Shape的原表，主要是__call，使其看起来更像C++初始化
        lua_newtable(L);
        lua_pushcfunction(L, luna<T>::create);
        lua_setfield(L, -2, "__call");
        lua_setmetatable(L, -2);
        lua_pop(L, 1); //这时候栈只剩下元表

        //设置元表Shape index指向自己
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, luna<T>::gc);
        lua_setfield(L, -2, "__gc");
    }
    return 0;
}

template<typename T>
int luna<T>::create(lua_State* L)
{
    lua_remove(L, 1);
    TObject* p = (TObject*)lua_newuserdata(L, sizeof(TObject));
    p->_u = new T();

    luaL_getmetatable(L, T::name);
    lua_setmetatable(L, -2);

    luaL_getmetatable(L, T::name);
    for (auto* l = T::methods; l->name; l++)
    {
        lua_pushlightuserdata(L,(void*)l);
        lua_pushlightuserdata(L,(void*)p);
        lua_pushcclosure(L, luna<T>::call, 2);
        lua_setfield(L, -2, l->name);
    }

    lua_pop(L, 1);

    return 1;
}

template<typename T>
int luna<T>::call(lua_State* L)
{
    TMethod* v = (TMethod*)lua_topointer(L, lua_upvalueindex(1));
    cout<<"luna<T>::call:"<<v->name<<endl;

    TObject* p = (TObject*)lua_topointer(L, lua_upvalueindex(2));
    return ((p->_u)->*(v->pf))(L);
}

template<typename T>
int luna<T>::gc(lua_State* L)
{
    TObject* p = (TObject*)lua_touserdata(L, 1);
    (p->_u)->~T();
    return 0;
}
