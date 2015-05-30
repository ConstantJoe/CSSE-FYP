#include <stdbool.h>

void serial_init(unsigned long baud);
void serial_put(unsigned char c);
void serial_puts(char *text);
bool serial_ready();
char serial_get();
unsigned char serial_is_init();
