#ifndef _SENSOR_H_

int rts_init_gpio();
int rts_open_1w_slave_fd();
float rts_read_temp(int fd);

#define _SENSOR_H
#endif
