#ifndef _GET_WEATHER_H_INCLUDED_
#define _GET_WEATHER_H_INCLUDED_

extern char weather[512];
extern char weather_stat;         // get_weather_func stat

void *get_weather_func(void *arg);

#endif

