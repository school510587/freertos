#ifndef SERIAL_IO_H
#define SERIAL_IO_H

void init_rs232_io();
void send_byte(char ch);
char recv_byte();

#endif
