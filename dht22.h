#ifndef _DHT22_H_INCLUDED_
#define _DHT22_H_INCLUDED_

extern double temperature;
extern double humidity;
extern char	  temp_hum_stat;		// temp_hum_func stat

void *temp_hum_func(void *arg);

#endif

