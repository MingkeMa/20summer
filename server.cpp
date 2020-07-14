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
  int pending_data; // 0->not ready; 1->query being processed; 2->ready to send back

  char *buffer;
  int offset;
  int msg_len;
  struct node *next;

  const char *query_text;
  // each client must have its own mysql connection to use async sql query
  MYSQL *con;
  net_async_status status;
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
  new_node->query_text = NULL;
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
void build_testdb(){
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

  // generate test tables
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
  while (current){
    temp=current;
    current=current->next;
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
  signal(SIGINT, my_handler);

  build_testdb();

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

    max = sock;              // initialize max

    // put connected sockets into the read and write sets to monitor them
    for (current = head.next; current; current = current->next)
    {
      FD_SET(current->socket, &read_set);

      // check query status
      if (current->pending_data==1)
      {
        if (current->status == NET_ASYNC_NOT_READY)
        {
          current->status = mysql_real_query_nonblocking(current->con, current->query_text, (unsigned long)strlen(current->query_text));
        }
        if (current->status == NET_ASYNC_ERROR)
        {
          handle_mysql_error(current->con);
        }
        if (current->status == NET_ASYNC_COMPLETE)
        {
          MYSQL_RES *result = mysql_store_result(current->con);
          if (result == NULL)
            handle_mysql_error(current->con);
          int num_fields = mysql_num_fields(result);
          MYSQL_ROW row;
          while ((row = mysql_fetch_row(result)))
          {
            for (int i = 0; i < num_fields; i++)
            {
              printf("%s ", row[i] ? row[i] : "NULL");
            }
            printf("\n");
          }
          mysql_free_result(result);
          current->pending_data=2;
        }

      }
      if (current->pending_data==2){
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
          // printf("try to send\n");
          // set message
          *(uint32_t *)(current->buffer) = (uint32_t)htonl((uint32_t)10);
          *(uint32_t *)(current->buffer + 4) = (uint32_t)htonl((uint32_t)1000);
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
          // printf("try to receive\n");
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
              current->msg_len = 10;
              printf("Received size is %d.\n", count);
            }
            current->offset += count;
            // if finish receiving, prepare for sending
            if (current->offset == current->msg_len)
            {
              uint32_t uid = ntohl(*(uint32_t *)(current->buffer + 2));
              uint32_t month = ntohl(*(uint32_t *)(current->buffer + 6));
              printf("uid is %u, month is %u\n", uid, month);
              // construct query
              std::string padding = month < 10 ? "0" : "";
              std::string date = "'2020-" + padding + std::to_string(month) + "-01'";
              // use BETWEEN to allow mysql to use index
              std::string query = "SELECT * FROM PlayerLogin WHERE (UID = " + std::to_string(uid) + ") AND (dtEventTime BETWEEN " + date +
                                  " AND LAST_DAY(" + date + "))";
              current->query_text = query.c_str();
              current->status = mysql_real_query_nonblocking(current->con, current->query_text,
                                                    (unsigned long)strlen(current->query_text));

              // if (mysql_query(con, query.c_str()))
              //   handle_mysql_error(con);

              // MYSQL_RES *result = mysql_store_result(con);
              // if (result == NULL)
              //   handle_mysql_error(con);
              // int num_fields = mysql_num_fields(result);
              // MYSQL_ROW row;
              // while ((row = mysql_fetch_row(result)))
              // {
              //   for (int i = 0; i < num_fields; i++)
              //   {
              //     printf("%s ", row[i] ? row[i] : "NULL");
              //   }
              //   printf("\n");
              // }
              // mysql_free_result(result);

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
