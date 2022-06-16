// compile with:
// $ gcc rtserver.c -lpigpio -o rtserver.o

#include <stdio.h>
#include <stdlib.h>
#include <error.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h> // for close ()
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/epoll.h>

#include <pigpio.h>
#include <dirent.h>
#include <fcntl.h>

// #define PRINT_REQUEST

#define THERMO_PIN 4

#define SERV_PORT 80
#define MAX_LINES 4096
#define MAX_CONN 3

#define MAX_EVENTS 3

#define handle_error(msg) \
    do { perror(msg); exit(-1); } while (0)

int rts_init_gpio();
float rts_read_1w_temp();

int main() {
  int quit = 0;
  int connfd, listenfd;
  struct sockaddr_in server_addr;
  int read_bytes;
  
  char buff[MAX_LINES];
  char recvbuff[MAX_LINES +1];
  
  int epollfd, nfds;
  struct epoll_event sock_ev, in_ev, events[MAX_EVENTS];
  
  printf("[info] initializing 1w sensor..\n");
  if(rts_init_gpio() == -1) {
    fprintf(stderr, "[ERROR] failed to initialize pi GPIOs\n");
    exit(-1);
  }
  sleep(5);
  
  printf("[info] starting the server..\n");
  if((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    handle_error("[ERROR] socket");
  }
  
  // make socket non-blocking
  if((fcntl(listenfd, F_SETFL, fcntl(listenfd, F_GETFL, 0) | O_NONBLOCK)) < 0) {
    handle_error("[ERROR] fcntl set socket non blocking");
  }
  
  bzero(&server_addr, sizeof server_addr);
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  server_addr.sin_port = htons(SERV_PORT);
  
  if((bind(listenfd, (struct sockaddr *)&server_addr, sizeof server_addr)) < 0) {
    handle_error("[ERROR] bind");
  }
  
  if((listen(listenfd, MAX_CONN)) < 0) {
    handle_error("[ERROR] listen");
  }
  
  if( (epollfd = epoll_create1(0)) == -1) {
    handle_error("[ERROR] epoll_create1");
  }
  
  sock_ev.events = EPOLLIN;
  sock_ev.data.fd = listenfd;
  if(epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &sock_ev) == -1) {
    handle_error("[ERROR] epollctl: listenfd");
  }
  
  // make stdin non-blocking
  if((fcntl(0, F_SETFL, fcntl(0, F_GETFL, 0) | O_NONBLOCK)) < 0) {
    handle_error("[ERROR] fcntl set stdin non blocking");
  }
  
  in_ev.events = EPOLLIN | EPOLLET;
  sock_ev.data.fd = 0;
  if(epoll_ctl(epollfd, EPOLL_CTL_ADD, 0, &in_ev) == -1) {
    handle_error("[ERROR] epollctl: stdin fd");
  }
  
  printf("[info] server successfully started\n");
  while(!quit) {
    printf("[info] waiting for connections on port %d\n", SERV_PORT);
    nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
    if(nfds == -1) {
      handle_error("[ERROR] epoll_wait");
    }
    for(int n=0; n<nfds; n++) {
      if(events[n].data.fd == listenfd) {
        if((connfd = accept(listenfd, NULL, NULL)) < 0) {
          perror("[ERROR] accept");
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
        float temp = rts_read_1w_temp();
        if(temp == -100.) {
          snprintf(buff, sizeof buff, "HTTP/1.0 200 OK\r\n\r\ncouldn't get the sensor data.. sorry ..");
        } else {
          printf("[info] room temp is %.3f\n", temp);
          snprintf(buff, sizeof buff, "HTTP/1.0 200 OK\r\n\r\nRoom Temp = %.3f grad Celcius", temp);
        }

        if( (send(connfd, buff, strlen(buff), 0)) < 0) {
          perror("[WARNING] send");
        }
        printf("[info] closing connection..\n");
        close(connfd);
        printf("\n********************************************\n");
      } else if(events[n].data.fd == 0) {
        char dummybuf[8];
        int read_bytes;
        // empty the stdin fd
        while(read(0, dummybuf, sizeof dummybuf) == sizeof dummybuf) {}
        quit = 1;
      }
    }
  }
  printf("[info] cleaning up..\n");
  close(listenfd);
  printf("[info] clean up done, have a great day.\n");
}

int rts_init_gpio() {
  if(gpioInitialise() == PI_INIT_FAILED) return -1;
  if(gpioSetMode(THERMO_PIN, PI_INPUT) != 0) return -1;
  if(gpioSetPullUpDown(THERMO_PIN, PI_PUD_UP) != 0) return -1;
  return 0;
}

float rts_read_1w_temp() {
    DIR *dir;
  struct dirent *dirent;
  char device_path[128];
  char path[] = "/sys/bus/w1/devices";
  int fd;
  
  char *temp_s = NULL;
  char buf[256];
  char data[6];

  dir = opendir (path);
  if (dir != NULL) {
    while ((dirent = readdir (dir))) {
      if (dirent->d_type == DT_LNK && strstr(dirent->d_name, "28-") != NULL) {
        sprintf(device_path, "%s/%s/w1_slave", path, dirent->d_name);
      }
    }
    closedir (dir);
  } else {
    perror ("[ERROR] one wire device directory");
    return -100;
  }
  
  fd = open(device_path, O_RDONLY);
  if(fd == -1) {
    perror ("[ERROR] open dev fd");
    return -100;
  }
  
  if((read(fd, buf, 256)) < 0) {
    perror("[ERROR] read temp");
    return -100;
  }
  
  temp_s = strstr(buf, "t=");
  if(!temp_s) {
    fprintf(stderr, "[ERROR] could not parse temp string\n");
    return -100;
  }
  strncpy(data, temp_s+2, 5);
  data[5] = 0;
  return strtof(data, NULL) / 1000;
}
