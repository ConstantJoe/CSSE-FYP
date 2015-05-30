#include <stdlib.h>
#include "simple_os.h"

unsigned char id;

void application_start();
void application_timer_tick(timer *t);
void application_radio_rx_msg(unsigned short dst, unsigned short src, int len, unsigned char *data);
void application_radio_tx_done();
void application_button_pressed();
void application_button_released();

