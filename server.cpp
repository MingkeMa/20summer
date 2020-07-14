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

MYSQL *con;
net_async_status status;

// catch ctrl+c
void my_handler(sig_atomic_t s)
{
  printf("server closed\n");
  mysql_close(con);
  exit(0);
}

// handle mysql error
void handle_mysql_error(MYSQL *con)
{
  fprintf(stderr, "%s\n", mysql_error(con));
  mysql_close(con);
  exit(1);
}

/**************************************************/
/* a few simple linked list functions             */
/**************************************************/

/* A linked list node data structure to maintain application
   information related to a connected socket */
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
};

/* remove the data structure associated with a connected socket
   used when tearing down the connection */
void dump(struct node *head, int socket)
{
  struct node *current, *temp;

  current = head;

  while (current->next)
  {
    if (current->next->socket == socket)
    {
      /* remove */
      temp = current->next;
      current->next = temp->next;
      free(temp); /* don't forget to free memory */
      return;
    }
    else
    {
      current = current->next;
    }
  }
}

/* create the data structure associated with a connected socket */
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
  head->next = new_node;
}

/*****************************************/
/* main program                          */
/*****************************************/

/* simple server, takes one parameter, the server port number */
int main(int argc, char **argv)
{
  signal(SIGINT, my_handler);

  con = mysql_init(NULL);
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

  /* socket and option variables */
  int sock, new_sock;
  int optval = 1;

  /* server socket address variables */
  struct sockaddr_in sin;
  unsigned short server_port = atoi(argv[1]);
  /* fill in the address of the server socket */
  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = INADDR_ANY;
  sin.sin_port = htons(server_port);

  /* socket address variables for incoming connected client */
  struct sockaddr_in addr;
  socklen_t addr_len = sizeof(struct sockaddr_in);

  /* maximum number of pending connection requests */
  int BACKLOG = 5;

  /* variables for select */
  fd_set read_set, write_set;
  struct timeval time_out;
  int select_retval, max;

  /* number of bytes sent/received */
  int count;

  /* linked list for keeping track of connected sockets */
  struct node head;
  struct node *current;

  /* initialize dummy head node of linked list */
  head.socket = -1;
  head.next = 0;

  /* create a server socket to listen for TCP connection requests */
  if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) //why PF???
  {
    perror("opening TCP socket");
    abort();
  }

  /* set option so we can reuse the port number quickly after a restart */
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0)
  {
    perror("setting TCP socket option");
    abort();
  }

  /* bind server socket to the address */
  if (bind(sock, (struct sockaddr *)&sin, sizeof(sin)) < 0)
  {
    perror("binding socket to address");
    abort();
  }

  /* put the server socket in listen mode */
  if (listen(sock, BACKLOG) < 0)
  {
    perror("listen on socket failed");
    abort();
  }

  while (1)
  {
    /* set up the file descriptor bit map that select should be watching */
    FD_ZERO(&read_set);  /* clear everything */
    FD_ZERO(&write_set); /* clear everything */

    FD_SET(sock, &read_set); /* put the listening socket in */
    max = sock;              /* initialize max */

    /* put connected sockets into the read and write sets to monitor them */
    for (current = head.next; current; current = current->next)
    {
      FD_SET(current->socket, &read_set);

      // check query status
      if (current->pending_data==1)
      {
        if (status == NET_ASYNC_NOT_READY)
        {
          status = mysql_real_query_nonblocking(con, current->query_text, (unsigned long)strlen(current->query_text));
        }
        if (status == NET_ASYNC_ERROR)
        {
          handle_mysql_error(con);
        }
        if (status == NET_ASYNC_COMPLETE)
        {
          MYSQL_RES *result = mysql_store_result(con);
          if (result == NULL)
            handle_mysql_error(con);
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

    time_out.tv_usec = 100000; /* 1-tenth of a second timeout */
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

    if (select_retval > 0) /* at least one file descriptor is ready */
    {
      /* check the server socket */
      if (FD_ISSET(sock, &read_set)) 
      {
        new_sock = accept(sock, (struct sockaddr *)&addr, &addr_len);
        if (new_sock < 0)
        {
          perror("error accepting connection");
          abort();
        }

        /* make the socket non-blocking */
        if (fcntl(new_sock, F_SETFL, O_NONBLOCK) < 0)
        {
          perror("making socket non-blocking");
          abort();
        }

        printf("Accepted connection. Client IP address is: %s\n", inet_ntoa(addr.sin_addr));

        /* remember this client connection in linked list */
        add(&head, new_sock, addr);
      }

      /* check other connected sockets */
      for (current = head.next; current; current = current->next)
      {
        //      printf("begin loop\n");

        /* check previously unsuccessful writes */
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
              /* we are trying to dump too much data down the socket,
		     it cannot take more for the time being
		     will have to go back to select and wait til select
		     tells us the socket is ready for writing
		  */
            }
            else
            {
              /* something else is wrong */
            }
          }
        }

        if (FD_ISSET(current->socket, &read_set))
        {
          /* we have data from a client */
          //	      printf("try to receive\n");
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

            /* clean up */
            close(current->socket);
            dump(&head, current->socket);
          }
          else
          {
            /* we got count bytes of data from the client */
            //initialize massage length
            if (current->msg_len == 0)
            {
              current->msg_len = 10;
              printf("Received size is %d.\n", count);
            }
            current->offset += count;
            //if finish receiving, prepare for sending
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
              status = mysql_real_query_nonblocking(con, current->query_text,
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
