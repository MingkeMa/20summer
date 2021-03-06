#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <math.h>

/* client takes two parameters, the server domain name,
   and the server port number */

int main(int argc, char **argv)
{

  // client socket
  int sock;
  // server socket address variables
  struct sockaddr_in sin;
  unsigned short server_port = atoi(argv[2]);

  // variables for identifying the server
  unsigned int server_addr;
  struct addrinfo *getaddrinfo_result, hints;

  // convert server domain name to IP address
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_INET; // IPv4
  if (getaddrinfo(argv[1], NULL, &hints, &getaddrinfo_result) == 0)
  {
    server_addr = (unsigned int)((struct sockaddr_in *)(getaddrinfo_result->ai_addr))->sin_addr.s_addr;
    freeaddrinfo(getaddrinfo_result);
  }

  // allocate buffer
  char *recvbuffer, *sendbuffer;
  int size = 1000;
  recvbuffer = (char *)malloc(size);
  if (!recvbuffer)
  {
    perror("fail to allocated recvbuffer");
    abort();
  }
  sendbuffer = (char *)malloc(size);
  if (!sendbuffer)
  {
    perror("fail to allocated sendbuffer");
    abort();
  }

  // create a socket
  if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
  {
    perror("fail to open TCP socket");
    abort();
  }

  // fill in server's address
  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = server_addr;
  sin.sin_port = htons(server_port);

  // connect to the server
  if (connect(sock, (struct sockaddr *)&sin, sizeof(sin)) < 0)
  {
    perror("connect to server failed");
    abort();
  }

  // send and receive multiple times for measurement
  int offset, temp, i;
  struct timeval send_tv, recv_tv;
  long double cur_latency, avg_latency, total_latency = 0;
  int my_count = 1;
  for (i = 0; i < my_count; i++)
  {
    // set message

    uint64_t uid = 1;
    uint16_t month = 1;
    int send_msg_len = 14;
    *(uint16_t *)(sendbuffer) = (uint16_t)htons((uint16_t)send_msg_len); // request len
    *(uint16_t *)(sendbuffer + 2) = (uint16_t)htons((uint16_t)1);        // request type
    *(uint32_t *)(sendbuffer + 4) = (uint32_t)htonl(uid >> 32);          // upper_uid
    *(uint32_t *)(sendbuffer + 8) = (uint32_t)htonl((uint32_t)uid);      // lower_uid
    *(uint16_t *)(sendbuffer + 12) = (uint16_t)htons(month);             // month

    // int send_msg_len = 4;
    // *(uint16_t *)(sendbuffer) = (uint16_t)htons((uint16_t)send_msg_len); // request len
    // *(uint16_t *)(sendbuffer + 2) = (uint16_t)htons((uint16_t)2);        // request type

    // send
    gettimeofday(&send_tv, NULL);
    offset = 0;
    while (offset < send_msg_len)
    {
      temp = send(sock, sendbuffer + offset, send_msg_len - offset, 0);
      if (temp < 0)
      {
        perror("Error in sending!\n");
        abort();
      }
      offset += temp;
    }

    // recvive
    offset = 0;
    int recv_msg_len = 8;
    // int recv_msg_len = 2 + 4 * 20;
    while (offset < recv_msg_len)
    {
      temp = recv(sock, recvbuffer + offset, recv_msg_len - offset, 0);
      if (temp < 0)
      {
        perror("Error in receiving!\n");
        abort();
      }
      offset += temp;
    }
    gettimeofday(&recv_tv, NULL);

    uint16_t login_times = ntohs(*(uint16_t *)(recvbuffer + 2));
    uint32_t login_duration = ntohl(*(uint32_t *)(recvbuffer + 4));
    printf("login %hu times, loginduration is %u s\n", login_times, login_duration);

    // printf("top 20 teams are:\n");
    // for (int i = 0; i < 20; i++)
    // {
    //   printf("%u ", ntohl(*(uint32_t *)(recvbuffer + 2 + 4 * i)));
    // }
    // printf("\n");
    
    cur_latency = (recv_tv.tv_sec - send_tv.tv_sec) * pow(10., 6) + recv_tv.tv_usec - send_tv.tv_usec;
    total_latency += cur_latency;
  }
  avg_latency = total_latency / my_count;
  printf("average latency is: %Lf millisecond.\n", avg_latency / pow(10., 3));
  // getchar();
  free(recvbuffer);
  free(sendbuffer);
  return 0;
}
