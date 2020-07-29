#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <string>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include <sys/stat.h>
#include <dirent.h>
#include <signal.h>

#include <mysql.h>
#include <mysqld_error.h>

extern "C"
{
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}
#include "bind.h"

//sudo mkdir -p /var/run/mysqld
//sudo chown mysql:mysql /var/run/mysqld
//sudo service mysql start
//./server 1000
//mysql -u root -p
//myPW

#define BUF_LEN 1000

/**************************************************/
/* simple linked list and helper functions        */
/**************************************************/

/* linked list node to maintain information related to a connected socket */
struct node
{
  int socket;
  struct sockaddr_in client_addr;
  struct node *next;
  char *buffer;

  int offset;
  int msg_len;
  int pending_data; // 0->not ready; n>=1-> nth query being processed; -1->ready to send back

  // each client must have its own mysql connection to use async sql query
  MYSQL *con;
  net_async_status status;
  char **query_array;
  int query_num;
  MYSQL_RES **results;
  int request_type;
};

// handle mysql error
void handle_mysql_error(MYSQL *con)
{
  fprintf(stderr, "%s\n", mysql_error(con));
  mysql_close(con);
  exit(1);
}

/* tear down the connection of a specific socket */
void dump(struct node *head, int socket)
{
  struct node *current, *temp;

  current = head;

  while (current->next)
  {
    if (current->next->socket == socket)
    {
      temp = current->next;
      current->next = temp->next;
      free(temp->buffer);
      mysql_close(temp->con);
      free(temp);
      return;
    }
    else
    {
      current = current->next;
    }
  }
}

/* create a node associated with a connected socket */
void add(struct node *head, int socket, struct sockaddr_in addr)
{
  struct node *new_node;

  new_node = (struct node *)malloc(sizeof(struct node));
  new_node->socket = socket;
  new_node->client_addr = addr;
  new_node->pending_data = 0;
  new_node->buffer = (char *)malloc(BUF_LEN);
  new_node->offset = 0;
  new_node->msg_len = 0;
  new_node->next = head->next;
  new_node->con = mysql_init(NULL);
  if (new_node->con == NULL)
  {
    fprintf(stderr, "%s\n", mysql_error(new_node->con));
    exit(1);
  }
  if (mysql_real_connect(new_node->con, "localhost", "root", "myPW",
                         "testdb", 0, NULL, 0) == NULL)
    handle_mysql_error(new_node->con);
  head->next = new_node;
}

/* build test database */
void build_testdb()
{
  // connect to mysql
  MYSQL *con = mysql_init(NULL);
  if (con == NULL)
  {
    fprintf(stderr, "%s\n", mysql_error(con));
    exit(1);
  }
  if (mysql_real_connect(con, "localhost", "root", "myPW",
                         "testdb", 0, NULL, 0) == NULL)
    handle_mysql_error(con);

  // generate PlayerLogin and PlayerLogout
  if (mysql_query(con, "DROP TABLE IF EXISTS PlayerLogin"))
    handle_mysql_error(con);
  if (mysql_query(con, "CREATE TABLE PlayerLogin(UID BIGINT, dtEventTime DATETIME)"))
    handle_mysql_error(con);
  if (mysql_query(con, "DROP TABLE IF EXISTS PlayerLogout"))
    handle_mysql_error(con);
  if (mysql_query(con, "CREATE TABLE PlayerLogout(UID BIGINT, dtEventTime DATETIME)"))
    handle_mysql_error(con);
  for (int i = 1; i < 30; i++)
  {
    std::string padding = i < 10 ? "0" : "";
    std::string query = "INSERT INTO PlayerLogin VALUES(1,'2020-01-" + padding + std::to_string(i) + " 00:00:01')";
    if (mysql_query(con, query.c_str()))
      handle_mysql_error(con);
    query = "INSERT INTO PlayerLogout VALUES(1,'2020-01-" + padding + std::to_string(i) + " 01:10:11')";
    if (mysql_query(con, query.c_str()))
      handle_mysql_error(con);
  }

  // generate MatchRegister
  if (mysql_query(con, "DROP TABLE IF EXISTS MatchRegister"))
    handle_mysql_error(con);
  if (mysql_query(con, "CREATE TABLE MatchRegister(TeamID CHAR(32), OpenID1 CHAR(64), OpenID2 CHAR(64),OpenID3 CHAR(64),OpenID4 CHAR(64))"))
    handle_mysql_error(con);
  for (int i = 1; i < 200; i++)
  {
    int base=i*10;
    std::string query = "INSERT INTO MatchRegister VALUES(" + std::to_string(i) + ","
    + std::to_string(base+1) + ","+ std::to_string(base+2) + ","+ std::to_string(base+3) + ","+ std::to_string(base+4) +")";
    if (mysql_query(con, query.c_str()))
      handle_mysql_error(con);
  }

  // generate GameRecord
  if (mysql_query(con, "DROP TABLE IF EXISTS GameRecord"))
    handle_mysql_error(con);
  if (mysql_query(con, "CREATE TABLE GameRecord(dtEventTime DATETIME, OpenID1 CHAR(64), OpenID2 CHAR(64),OpenID3 CHAR(64),OpenID4 CHAR(64), TeamKill INT, TeamRank INT)"))
    handle_mysql_error(con);
  for (int i = 1; i < 200; i++)
  {
      for (int j = 1; j < 50; j++)   // each team has 49 game records
    {
      int base=i*10;
      std::string padding = (j%30+1) < 10 ? "0" : "";
      std::string query = "INSERT INTO GameRecord VALUES('2020-0"+std::to_string(j/30+1)+"-" + padding + std::to_string(j%30+1) + " 00:00:01'," + 
      std::to_string(base+1) + ","+ std::to_string(base+2) + ","+ std::to_string(base+3) + ","+ std::to_string(base+4) 
      +","+ std::to_string(j) +","+ std::to_string(i%25+1) +")";
      if (mysql_query(con, query.c_str()))
        handle_mysql_error(con);
    }
  }

  // close connection
  mysql_close(con);
}

/**************************************************/
/* global variables and exit function             */
/**************************************************/

// linked list for keeping track of connected sockets
struct node head;

/* catch ctrl+c and exit */
void my_handler(sig_atomic_t s)
{
  struct node *temp, *current;
  current = head.next;
  while (current)
  {
    temp = current;
    current = current->next;
    close(temp->socket);
    free(temp->buffer);
    mysql_close(temp->con);
    free(temp);
  }

  printf("server closed\n");
  exit(0);
}

/*****************************************/
/* main program                          */
/*****************************************/

/* takes one parameter, the server port number */
int main(int argc, char **argv)
{
  // register ctrl+c
  signal(SIGINT, my_handler);
  // //initial mysql
  // build_testdb();
  // initial lua
  lua_State *L = luaL_newstate();
  luaL_openlibs(L);
  luaL_dofile(L, "register.lua");
  luaL_dofile(L, "tree.lua");
  lunar<Buffer>::regist(L);

  // initialize dummy head node of linked list
  head.socket = -1;
  head.next = 0;
  struct node *current;

  // socket and option variables
  int sock, new_sock;
  int optval = 1;

  // server socket address variables
  struct sockaddr_in sin;
  unsigned short server_port = atoi(argv[1]);
  // fill in the address of the server socket
  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = INADDR_ANY;
  sin.sin_port = htons(server_port);

  // socket address variables for incoming connected client
  struct sockaddr_in addr;
  socklen_t addr_len = sizeof(struct sockaddr_in);

  // maximum number of pending connection requests
  int BACKLOG = 5;

  // variables for select
  fd_set read_set, write_set;
  struct timeval time_out;
  int select_retval, max;

  // number of bytes sent/received
  int count;

  // create a server socket to listen for TCP connection requests
  if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) //why PF???
  {
    perror("opening TCP socket");
    abort();
  }

  // set option so we can reuse the port number after a restart
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0)
  {
    perror("setting TCP socket option");
    abort();
  }

  // bind server socket to the address
  if (bind(sock, (struct sockaddr *)&sin, sizeof(sin)) < 0)
  {
    perror("binding socket to address");
    abort();
  }

  // put the server socket in listen mode
  if (listen(sock, BACKLOG) < 0)
  {
    perror("listen on socket failed");
    abort();
  }

  while (1)
  {
    // set up the file descriptor bit map
    FD_ZERO(&read_set);
    FD_ZERO(&write_set);
    FD_SET(sock, &read_set); // put the listening socket in

    max = sock; // initialize max

    // put connected sockets into the read and write sets to monitor them
    for (current = head.next; current; current = current->next)
    {
      FD_SET(current->socket, &read_set);

      // printf("query is %s\n", current->query_array[current->pending_data-1]);

      // check query status
      if (current->pending_data > 0)
      {
        // query not finished
        if (current->status == NET_ASYNC_NOT_READY)
        {
          current->status = mysql_real_query_nonblocking(current->con, current->query_array[current->pending_data - 1],
                                                         (unsigned long)strlen(current->query_array[current->pending_data - 1]));
        }
        // error happens
        if (current->status == NET_ASYNC_ERROR)
        {
          handle_mysql_error(current->con);
        }
        // query completed
        if (current->status == NET_ASYNC_COMPLETE)
        {
          current->results[current->pending_data - 1] = mysql_store_result(current->con);
          if (current->results[current->pending_data - 1] == NULL)
            handle_mysql_error(current->con);

          current->pending_data += 1;
          if (current->pending_data > current->query_num)
          { // all queries finished
            // extract results, free query array and results
            for (int i = 0; i < current->query_num; i++)
            {
              delete[] current->query_array[i];
              int num_fields = mysql_num_fields(current->results[i]);
              MYSQL_ROW row;
              std::string res;

              while ((row = mysql_fetch_row(current->results[i])))
              {
                for (int i = 0; i < num_fields; i++)
                {
                  // printf("%s ", row[i] ? row[i] : "NULL");
                  if (i > 0)
                    res.append("&");
                  res.append(row[i] ? row[i] : "NULL");
                }
                // printf("\n");
                res.append("|");
              }
              res.pop_back(); //remove the last appended "|"
              mysql_free_result(current->results[i]);
              lua_pushstring(L, res.c_str());
              std::string variable = "res" + std::to_string(i);
              lua_setglobal(L, variable.c_str());
            }
            delete[] current->query_array;
            delete[] current->results;

            // call lua scripts
            lua_pushlightuserdata(L, current->buffer);
            lua_setglobal(L, "buffer_pointer");
            lua_getglobal(L, "lua_scripts");
            lua_pushnumber(L, current->request_type);
            lua_gettable(L, -2);
            const char *script = lua_tostring(L, -1);
            auto ret = luaL_dofile(L, script);
            if (ret != 0)
            {
              printf("Error occurs when calling luaL_dofile() Hint Machine 0x%x\n", ret);
              printf("Error: %s\n", lua_tostring(L, -1));
            }

            // // set message
            // lua_getglobal(L, "cnt");
            // lua_getglobal(L, "dur");
            // if (!lua_isnumber(L, -2))
            //   printf("Error! `cnt' should be a number\n");
            // if (!lua_isnumber(L, -1))
            //   printf("Error! `dur' should be a number\n");
            // *(uint16_t *)(current->buffer + 2) = (uint16_t)htons((uint16_t)lua_tonumber(L, -2));
            // *(uint32_t *)(current->buffer + 4) = (uint32_t)htonl((uint32_t)lua_tonumber(L, -1));
            current->pending_data = -1;
            lua_getglobal(L, "msg_len");
            if (!lua_isinteger(L, -1))
              printf("Error! `msg_len' should be a number\n");
            current->msg_len = lua_tointeger(L,-1);
            lua_settop(L, 0);
          }
          else
          { // do the next query
            current->status = mysql_real_query_nonblocking(current->con, current->query_array[current->pending_data - 1],
                                                           (unsigned long)strlen(current->query_array[current->pending_data - 1]));
          }
        }
      }

      if (current->pending_data == -1)
      {
        FD_SET(current->socket, &write_set);
      }

      // update max if necessary
      if (current->socket > max)
      {
        max = current->socket;
      }
    }

    time_out.tv_usec = 100000; // 0.1 s timeout
    time_out.tv_sec = 0;

    // invoke select
    // printf("max is %d\n",max);
    select_retval = select(max + 1, &read_set, &write_set, NULL, &time_out);
    if (select_retval < 0)
    {
      perror("select failed");
      abort();
    }

    if (select_retval == 0)
    {
      // printf("skip\n");
      continue;
    }

    if (select_retval > 0)
    {
      // check the server socket
      if (FD_ISSET(sock, &read_set))
      {
        new_sock = accept(sock, (struct sockaddr *)&addr, &addr_len);
        if (new_sock < 0)
        {
          perror("error accepting connection");
          abort();
        }

        // make it non-blocking
        if (fcntl(new_sock, F_SETFL, O_NONBLOCK) < 0)
        {
          perror("making socket non-blocking");
          abort();
        }

        printf("Accepted connection. Client IP address is: %s\n", inet_ntoa(addr.sin_addr));

        // add into linked list
        add(&head, new_sock, addr);
      }

      // check other connected sockets
      for (current = head.next; current; current = current->next)
      {
        //      printf("begin loop\n");

        // check previously unsuccessful writes
        if (FD_ISSET(current->socket, &write_set))
        {
          count = send(current->socket, current->buffer + current->offset, current->msg_len - current->offset, MSG_DONTWAIT);
          if (count > 0)
          {
            current->offset += count;
            //if finish sending, reset the node
            if (current->offset == current->msg_len)
            {
              current->msg_len = 0;
              current->offset = 0;
              current->pending_data = 0;
            }
          }
          if (count < 0)
          {
            if (errno == EAGAIN)
            {
              // too much data waiting for the socket
            }
            else
            {
              // something else is wrong
            }
          }
        }

        if (FD_ISSET(current->socket, &read_set))
        {
          // we have data from a client
          count = recv(current->socket, current->buffer + current->offset, BUF_LEN - current->offset, MSG_DONTWAIT);
          if (count <= 0)
          {
            if (count == 0)
            {
              printf("Client closed connection. Client IP address is: %s\n", inet_ntoa(current->client_addr.sin_addr));
            }
            else
            {
              perror("error receiving from a client");
            }

            // clean up
            close(current->socket);
            dump(&head, current->socket);
          }
          else
          {
            // initialize massage length
            if (current->msg_len == 0)
            {
              current->msg_len = ntohs(*(uint32_t *)(current->buffer));
              printf("Received a request, length is %d.\n", current->msg_len);
            }
            current->offset += count;
            // if finish receiving, construct queries
            if (current->offset == current->msg_len)
            {
              // call lua scripts
              lua_pushlightuserdata(L, current->buffer);
              lua_setglobal(L, "buffer_pointer");
              auto ret = luaL_dofile(L, "read_buffer.lua");
              if (ret != 0)
              {
                printf("Error occurs when calling luaL_dofile() Hint Machine 0x%x\n", ret);
                printf("Error: %s\n", lua_tostring(L, -1));
              }
              // retrieve queries
              lua_getglobal(L, "request_type");
              if (!lua_isnumber(L, -1))
                printf("Error! `request_type' should be a number\n");
              current->request_type = lua_tonumber(L, -1);
              lua_getglobal(L, "query_num");
              if (!lua_isnumber(L, -1))
                printf("Error! `query_num' should be a number\n");
              current->query_num = lua_tonumber(L, -1);
              current->query_array = new char *[current->query_num];
              current->results = new MYSQL_RES *[current->query_num];
              for (int i = 0; i < current->query_num; i++)
              {
                std::string variable = "query" + std::to_string(i + 1);
                lua_getglobal(L, variable.c_str());
                if (!lua_isstring(L, -1))
                  printf("Error! `query' should be a string\n");
                const char *query = lua_tostring(L, -1);
                // make a copy for each query
                int len = strlen(query);
                current->query_array[i] = new char[len + 1];
                strncpy(current->query_array[i], query, len);
                current->query_array[i][len] = '\0';
              }
              lua_settop(L, 0);

              // run the first query
              current->status = mysql_real_query_nonblocking(current->con, current->query_array[0],
                                                             (unsigned long)strlen(current->query_array[0]));

              current->pending_data = 1;
              current->offset = 0;
            }
          }
        }
      }
    }
  }
  return 0;
}
