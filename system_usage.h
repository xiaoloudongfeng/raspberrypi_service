#ifndef _SYSTEM_USAGE_H_INCLUDED_
#define _SYSTEM_USAGE_H_INCLUDED_

extern double cpu_usage;
extern double mem_usage;

void *mem_usage_func(void *arg); 

void *cpu_usage_func(void *arg);

#endif

