#include <iostream>
#include <cstring>
#include <stdlib.h>
extern "C"
{
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}
#include <pthread.h>
#include <stdio.h>
#include <queue>
#include <tuple>
#include <vector>
#include <functional>
#include <map>
#include <algorithm>

static int l_split(lua_State *L)
{
    const char *s = luaL_checkstring(L, 1);
    const char *sep = luaL_checkstring(L, 2);
    const char *e;
    int i = 1;

    lua_newtable(L); /* result */

    while ((e = strchr(s, *sep)) != NULL)
    {
        lua_pushlstring(L, s, e - s); /* push substring */
        lua_rawseti(L, -2, i++);
        s = e + 1; /* skip separator */
    }

    /* push last substring */
    lua_pushstring(L, s);
    lua_rawseti(L, -2, i);

    return 1;
}

static int l_time_gap(lua_State *L)
{
    const char *l = luaL_checkstring(L, 1);
    const char *r = luaL_checkstring(L, 2);
    char *el, *er;
    int templ, tempr;

    int res = 0; /* result */

    templ = strtol(l, &el, 10);
    tempr = strtol(r, &er, 10);
    res += 0; /* omit year */

    templ = strtol(el + 1, &el, 10);
    tempr = strtol(er + 1, &er, 10);
    res += 0; /* omit month */

    templ = strtol(el + 1, &el, 10);
    tempr = strtol(er + 1, &er, 10);
    res += (tempr - templ) * 86400; /* day */

    templ = strtol(el + 1, &el, 10);
    tempr = strtol(er + 1, &er, 10);
    res += (tempr - templ) * 3600; /* hour */

    templ = strtol(el + 1, &el, 10);
    tempr = strtol(er + 1, &er, 10);
    res += (tempr - templ) * 60; /* min */

    templ = strtol(el + 1, &el, 10);
    tempr = strtol(er + 1, &er, 10);
    res += tempr - templ; /* sec */

    lua_pushnumber(L, res);

    return 1;
}

// hold data for each thread, passed into pthread_create
struct thread_data
{
    std::map<std::string, std::vector<std::tuple<std::string, int, int>>> *data;
    std::vector<std::string> *my_thread;
    int self; //thread index
};

// initialize thread data
static void init_threadData(thread_data *p, std::map<std::string, std::vector<std::tuple<std::string, int, int>>> *data,
                            std::vector<std::string> *my_thread, int self)
{
    p->data = data;
    p->my_thread = my_thread;
    p->self = self;
}

static void *get_top30(void *t_data)
{
    thread_data *tdata = (thread_data *)t_data;
    int self = tdata->self;
    for (int i = 0; i < tdata->my_thread[self].size(); i++)
    {
        std::priority_queue<std::tuple<std::string, int, int>, std::vector<std::tuple<std::string, int, int>>, std::greater<std::tuple<std::string, int, int>>> pq;
        std::string teamname = tdata->my_thread[self].at(i);
        for (auto &element : tdata->data->operator[](teamname))
        {
            pq.push(element);
            if (pq.size() > 30)
            {
                pq.pop();
            }
        }
        int j = 1;
        while (!pq.empty())
        {
            tdata->data->operator[](teamname).at(j) = pq.top();
            pq.pop();
            j++;
        }
    }
    pthread_exit(0);
}

static int pq_pthread_lua(lua_State *L)
{
    // lua_pushnumber(L, start_mythread());
    // return 1;

    if (!lua_istable(L, -1))
    {
        printf("need to pass a table");
        return 0;
    }

    const int nworkers = 4;        // number of threads
    pthread_t p_threads[nworkers]; // store all threads to reuse
    int work_per_thread;           //decide whether to truncate to serial

    // prepare thread data container
    std::map<std::string, std::vector<std::tuple<std::string, int, int>>> data;
    std::vector<std::string> my_thread[nworkers];
    int cnt = 0;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    thread_data **all_t_data = new thread_data *[nworkers]; // hold thread data to reuse

    // get data from lua table
    lua_pushnil(L); /* first key */
    while (lua_next(L, -2) != 0)
    {
        /* uses 'key' (at index -2) and 'value' (at index -1) */
        if (!lua_istable(L, -1))
        {
            printf("value is not a table");
            return 0;
        }
        // std::priority_queue<std::tuple<std::string, int, int>, std::vector<std::tuple<std::string, int, int>>, std::greater<std::tuple<std::string, int, int>>> pq;
        std::string teamname(lua_tostring(L, -2));
        data[teamname] = std::vector<std::tuple<std::string, int, int>>();
        my_thread[cnt].push_back(teamname);
        cnt = (cnt + 1) % nworkers;

        lua_pushnil(L);
        while (lua_next(L, -2) != 0)
        {
            lua_rawgeti(L, -1, 1);
            std::string ts(lua_tostring(L, -1));
            lua_rawgeti(L, -2, 2);
            int kill = lua_tointeger(L, -1);
            lua_rawgeti(L, -3, 3);
            int rank = lua_tointeger(L, -1);
            // pq.push(std::make_tuple(ts, kill, rank));
            // if (pq.size() > 30)
            // {
            //     pq.pop();
            // }
            data[teamname].push_back(std::make_tuple(ts, kill, rank));

            lua_pop(L, 4);
        }

        // int i = 1;
        // while (!pq.empty())
        // {
        //     lua_rawgeti(L, -1, i);
        //     lua_pushstring(L, std::get<0>(pq.top()).c_str());
        //     lua_rawseti(L, -2, 1);
        //     lua_pushinteger(L, std::get<1>(pq.top()));
        //     lua_rawseti(L, -2, 2);
        //     lua_pushinteger(L, std::get<2>(pq.top()));
        //     lua_rawseti(L, -2, 3);
        //     pq.pop();
        //     lua_pop(L, 1); //pop the table v
        //     i++;
        // }
        /* removes 'value'; keeps 'key' for next iteration */
        lua_pop(L, 1);
    }

    for (int i = 0; i < nworkers; i++)
    {
        all_t_data[i] = new thread_data();
        init_threadData(all_t_data[i], &data, my_thread, i);
        pthread_create(&p_threads[i], &attr, get_top30, (void *)all_t_data[i]);
    }
    for (int i = 0; i < nworkers; i++)
    {
        pthread_join(p_threads[i], NULL);
    }

    // traverse lua table again to write data
    lua_pushnil(L);
    while (lua_next(L, -2) != 0)
    {
        std::string teamname(lua_tostring(L, -2));
        for (int i = 1; i < std::min((size_t)31, data[teamname].size()); i++)
        {
            lua_rawgeti(L, -1, i);
            lua_pushstring(L, std::get<0>(data[teamname].at(i)).c_str());
            lua_rawseti(L, -2, 1);
            lua_pushinteger(L, std::get<1>(data[teamname].at(i)));
            lua_rawseti(L, -2, 2);
            lua_pushinteger(L, std::get<2>(data[teamname].at(i)));
            lua_rawseti(L, -2, 3);
            lua_pop(L, 1); //pop the table v
        }
        /* removes 'value'; keeps 'key' for next iteration */
        lua_pop(L, 1);
    }

    for (int i = 0; i < nworkers; ++i)
    {
        delete[] all_t_data[i];
    }
    delete[] all_t_data;

    return 0;
}

static const struct luaL_Reg mylib[] = {
    {"split", l_split},
    {"time_gap", l_time_gap},
    {"pq_pthread", pq_pthread_lua},
    {NULL, NULL} /* sentinel */
};

extern "C" int luaopen_mylib(lua_State *L)
{
    luaL_newlib(L, mylib);
    return 1;
}
