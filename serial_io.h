#ifndef SERIAL_IO_H
#define SERIAL_IO_H

__attribute__((constructor)) void init_serial_io();
void send_byte(char ch);
char recv_byte(int *log);

#endif
