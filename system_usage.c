#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdint.h>

#include "system_usage.h"

double		cpu_usage = 0.0;
double		mem_usage = 0.0;

void *mem_usage_func(void *arg) 
{
	uint32_t	mem_total;
	uint32_t	mem_available;
	int			fd;
	char		mem_buf[128], *curr, *prev;
	int			i;
	uint8_t		flag;

	while (1) {
		fd = open("/proc/meminfo", O_RDONLY);
		memset(mem_buf, 0, sizeof(mem_buf));
		read(fd, mem_buf, sizeof(mem_buf));
		close(fd);
		
		for(i = 0, curr = mem_buf, prev = curr, flag = 0; 
			i < 3; curr++) 
		{
			if ((*curr >= '0') && (*curr <= '9')) {
				if (!flag) {			// first find a number, set a flag
					flag = 1;
					prev = curr;
				}

			} else {
				if (flag) {				// found whole number
					if (i == 0) {
						mem_total = atoi(prev);
					}

					if (i == 2) {
						mem_available = atoi(prev);
					}
					i++;
					flag = 0;
				}
			}
		}

		mem_usage = (mem_total - mem_available) * 100.0 / mem_total;
		fprintf(stderr, "mem_total: %d, mem_available: %d, mem_usage: %lf\n", 
			mem_total, mem_available, mem_usage);

		sleep(1);
	}
}

void *cpu_usage_func(void *arg)
{
	uint32_t	t1_total, t1_idel, t2_total, t2_idel;
	char		stat_buf[512], *curr, *prev;
	uint8_t		word_count;
	int			fd;

	while (1) {
		fd = open("/proc/stat", O_RDONLY);			// open /proc/stat
		memset(stat_buf, 0, sizeof(stat_buf));
		read(fd, stat_buf, sizeof(stat_buf));
		close(fd);

		for (curr = stat_buf, prev = curr, word_count = 0; 
			word_count < 11; curr++)
		{
			if (*curr == ' ' || *curr == '\n') {	// find a word
				*curr = '\0';
				if (curr != prev) {					// if not just a ' '
					word_count++;
					fprintf(stderr, "%s\n", prev);
					if (prev != stat_buf) {			// "cpu" should't be calculated
						t2_total += atoi(prev);		// add to t1_total
						if (word_count == 5) {
							t2_idel = atoi(prev);	// idel
						}
					}
				}
				prev = curr + 1;
			}
		}
	
		cpu_usage = ((t2_total - t1_total) - (t2_idel - t1_idel)) * 100.0 / (t2_total - t1_total);
		fprintf(stderr, "t1_total: %d, t1_idel: %d, t2_total: %d, t2_idel: %d, cpu_usage: %lf\n", 
			t1_total, t1_idel, t2_total, t2_idel, cpu_usage); 

		t1_total = t2_total;
		t2_total = 0;

		t1_idel = t2_idel;
		t2_idel = 0;

		sleep(2);
	}
}

