#include <iostream>
#include <cstring>
extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

using namespace std;

enum class lua_member_type
{
    member_none,
    member_func,
    member_int
};

#define DECLARE_LUNAR_CLASS(obj) \
    static const char *name;\
    static lunar<obj>::TMethod methods[];

#define EXPORT_LUNAR_FUNCTION_BEGIN(obj) \
    const char* obj::name = #obj;\
    lunar<obj>::TMethod obj::methods[] = {

#define EXPORT_LUNAR_MEMBER_INT(obj, member) \
    {#member, nullptr, lua_member_type::member_int, offsetof(obj, member)},

#define EXPORT_LUNAR_FUNCTION(obj, func) \
    {#func, &obj::func, lua_member_type::member_func, 0},

#define EXPORT_LUNAR_FUNCTION_END(obj) \
    {nullptr, nullptr, lua_member_type::member_none, 0}\
    };

template<typename T>
class lunar
{
    public:
        typedef struct {T* _u;} TObject;
        typedef int (T::*TPfn)(lua_State* L); 
        typedef struct {const char* name; TPfn pf; lua_member_type type; int offset;} TMethod;
    public:
        static int regist(lua_State* L); 
        static int create(lua_State* L); 
        static int call(lua_State* L); 
        static int gc(lua_State* L); 
        static int member_index(lua_State* L); 
        static int member_new_index(lua_State* L); 
};

template<typename T>
int lunar<T>::member_index(lua_State* L)
{
    int top = lua_gettop(L);
    lua_getmetatable(L, 1);
    lua_insert(L, -2);
    lua_rawget(L, -2);
    if (!lua_islightuserdata(L, -1))
    {
        lua_settop(L, top);
        return 0;
    }

    TMethod* l = (TMethod*)lua_topointer(L, -1);
    TObject* p = (TObject*)lua_topointer(L, 1);

    switch (l->type)
    {
        case lua_member_type::member_func:
            {
                lua_settop(L, top);
                lua_pushlightuserdata(L, (void*)l);
                lua_pushlightuserdata(L, (void*)p);
                lua_pushcclosure(L, &lunar<T>::call, 2);
                break;
            }
        case lua_member_type::member_int:
            {
                int val = *(int *)((char*)(p->_u) + l->offset);
                lua_settop(L, top);
                lua_pushinteger(L, val);
                break;
            }
        default:
            {
                cout<<"member index type error"<<endl;
                break;
            }
    }

    return 1;
}

template<typename T>
int lunar<T>::member_new_index(lua_State* L)
{
    int top = lua_gettop(L);
    lua_getmetatable(L, 1);
    lua_pushvalue(L, 2);
    lua_rawget(L, -2);
    if (!lua_islightuserdata(L, -1))
    {
        lua_settop(L, top);
        return 0;
    }

    TMethod* l = (TMethod*)lua_topointer(L, -1);
    TObject* p = (TObject*)lua_topointer(L, 1);

    switch (l->type)
    {
        case lua_member_type::member_func:
            {
                break;
            }
        case lua_member_type::member_int:
            {
                int val = lua_tointeger(L, 3);
                *(int *)((char*)(p->_u) + l->offset) = val;
                lua_settop(L, top);
                break;
            }
        default:
            {
                cout <<"member new index type error"<<endl;
                break;
            }
    }

    return 0;
}

template<typename T>
int lunar<T>::regist(lua_State* L)
{
    if (luaL_newmetatable(L, T::name))
    {
        lua_pushcfunction(L, &lunar<T>::member_index);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, &lunar<T>::member_new_index);
        lua_setfield(L, -2, "__newindex");
        lua_pushcfunction(L, lunar<T>::gc);
        lua_setfield(L, -2, "__gc");
    }

    //设置方法和成员
    for (auto* l = T::methods; l->name; l++)
    {
        lua_pushstring(L, l->name);
        lua_pushlightuserdata(L, (void*)l);
        lua_rawset(L, -3);
    }

    lua_getglobal(L, "lunar");
    if (!lua_istable(L, -1))
    {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setglobal(L, "lunar");
    }

    lua_pushcfunction(L, &lunar<T>::create);
    lua_setfield(L, -2, T::name);
    lua_pop(L, 2);

    return 0;
}

template<typename T>
int lunar<T>::create(lua_State* L)
{
    TObject* p = (TObject*)lua_newuserdata(L, sizeof(TObject));
    p->_u  = new T();

    luaL_getmetatable(L, T::name);
    lua_setmetatable(L, -2);

    return 1;
}

template<typename T>
int lunar<T>::call(lua_State* L)
{
    TMethod* v = (TMethod*)lua_topointer(L, lua_upvalueindex(1));
    cout<<"lunar<T>::call:"<<v->name<<endl;

    TObject* p = (TObject*)lua_topointer(L, lua_upvalueindex(2));

    return ((p->_u)->*(v->pf))(L);
}

template<typename T>
int lunar<T>::gc(lua_State* L)
{
    if (!lua_isuserdata(L, -1))
    {
        cout<<"gc cause error."<<endl;
    }

    TObject* p = (TObject*)lua_topointer(L, -1);
    delete p->_u;
    return 0;
}
