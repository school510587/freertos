#ifndef RS232_IO_H
#define RS232_IO_H

void init_rs232_io();
void send_byte(char ch);
char recv_byte();

#endif
