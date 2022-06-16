#include "rts_sensor.h"

#include <pigpio.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#define THERMO_PIN 4

int rts_init_gpio() {
  int status = gpioInitialise();
  gpioSetMode(THERMO_PIN, PI_INPUT);
  gpioSetPullUpDown(THERMO_PIN, PI_PUD_UP);
  
  return 0;
}

int rts_open_1w_slave_fd() {
  DIR *dir;
  struct dirent *dirent;
  char device_path[128];
  char path[] = "/sys/bus/w1/devices";
  int fd;

  dir = opendir (path);
  if (dir != NULL) {
    while ((dirent = readdir (dir))) {
      if (dirent->d_type == DT_LNK && strstr(dirent->d_name, "28-") != NULL) {
        sprintf(device_path, "%s/%s/w1_slave", path, dirent->d_name);
      }
    }
    closedir (dir);
  } else {
    perror ("one wire device directory");
    return -1;
  }
  
  fd = open(device_path, O_RDONLY);
  if(fd == -1) {
    perror ("open dev fd");
    return -1;
  }
  return fd;
}

float rts_read_temp(int fd) {
  char *temp_s = NULL;
  char buf[256];
  char data[6];
  if((read(fd, buf, 256)) < 0) {
    perror("read temp");
    return -100;
  }
  
  temp_s = strstr(buf, "t=");
  if(!temp_s) {
    fprintf(stderr, "could not parse temp string\n");
    return -100;
  }
  strncpy(data, temp_s+2, 5);
  data[5] = 0;
  return strtof(data, NULL) / 1000;

  return 1;
}


