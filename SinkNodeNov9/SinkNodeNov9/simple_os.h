#ifndef SIMPLE_OS_H_
#define SIMPLE_OS_H_

typedef struct _os_timer {
	unsigned char active;
	unsigned char pending;
	unsigned char type;
	unsigned char expired;
	unsigned short initial;
	unsigned short period;
	unsigned short remaining;
} timer;

//
// Timer granularity
//
#define TIMER_SECONDS 0
#define TIMER_MILLISECONDS 1

//
// Note currently there is a limit of 10 timers - any extra timers will just be ignored
//
void timer_init(timer *t, unsigned char t_type, unsigned short t_delay, unsigned short t_period);
void timer_start(timer *t);
void timer_stop(timer *t);

#endif /* SIMPLE_OS_H */
