#include <stdio.h>
#include <stdlib.h>
#include <error.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h> // for close ()
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>

#include "rts_sensor.h"

// #define PRINT_REQUEST

#define SERV_PORT 80
#define MAX_LINES 4096
#define MAX_CONN 3

#define handle_error(msg) \
    do { perror(msg); exit(-1); } while (0)
    
int quit = 0;
int listenfd;

void cleanup() {
  quit = 1;
  close(listenfd);
  printf("\ncleanup done\n");
}

int main() {
  // signal(SIGINT, cleanup); // TODO :sighandling is handled by pigpio so this isnt possible here 
  
  int connfd;
  struct sockaddr_in server_addr;
  int read_bytes;
  
  char buff[MAX_LINES];
  char recvbuff[MAX_LINES +1];
  
  printf("initializing 1w sensor..\n");
  rts_init_gpio();
  sleep(5);
  
  printf("starting the server..\n");
  if((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    handle_error("socket");
  }
  
  bzero(&server_addr, sizeof server_addr);
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  server_addr.sin_port = htons(SERV_PORT);
  
  if((bind(listenfd, (struct sockaddr *)&server_addr, sizeof server_addr)) < 0) {
    handle_error("bind");
  }
  
  if((listen(listenfd, 10)) < 0) {
    handle_error("bind");
  }

  printf("server successfully started\n");
  while(!quit) {
    printf("waiting for connections on port %d\n", SERV_PORT);
    if((connfd = accept(listenfd, NULL, NULL)) < 0) {
      if(quit) {
        break;
      }
      perror("accept");
    }
    
    memset(recvbuff, 0, MAX_LINES);
    
    while ( (read_bytes = read(connfd, recvbuff, MAX_LINES-1)) > 0 ){
#ifdef PRINT_REQUEST
      printf("[info] recved buff is:\n%s\n", recvbuff);
#endif
      if( recvbuff[read_bytes-4] == 0xd &&
          recvbuff[read_bytes-3] == 0xa &&
          recvbuff[read_bytes-2] == 0xd &&
          recvbuff[read_bytes-1] == 0xa) {
        break;
      }
    }
    float temp = rts_read_temp(rts_open_1w_slave_fd());
    printf("room temp is %.3f\n", temp);
    snprintf(buff, sizeof buff, "HTTP/1.0 200 OK\r\n\r\nRoom Temp = %.3f grad Celcius\r\n\r\n", temp);
    if( (send(connfd, buff, strlen(buff), 0)) < 0) {
      perror("send");
    }
    printf("closing connection..\n");
    close(connfd);
    printf("\n********************************************\n");
  }
}
