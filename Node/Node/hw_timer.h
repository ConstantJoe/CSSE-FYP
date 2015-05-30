#ifndef TIMER_H_
#define TIMER_H_

#include <stdbool.h>

// set default CPU clock frequency to 8 MHz
/*#ifndef F_CPU
#define F_CPU 8000000
#endif*/ /* F_CPU */

unsigned long hw_timer_cpuf();
void hw_timer_init(void);
void hw_timer_start(unsigned short value);
bool hw_timer_overflow();
unsigned short hw_timer_now();
unsigned long hw_timer_now_us();
void hw_timer_wait_micro(unsigned short delay);
void hw_timer_wait_milli(unsigned short delay);

#endif /* TIMER_H_ */
