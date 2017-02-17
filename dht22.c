#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <bcm2835.h>

#define DHT			RPI_V2_GPIO_P1_03

#define	DATA_IN		BCM2835_GPIO_FSEL_INPT
#define	DATA_OUT	BCM2835_GPIO_FSEL_OUTP

double		temperature = 0.0;
double		humidity = 0.0;
char		temp_hum_stat = '!';		// temp_hum_func stat

void *temp_hum_func(void *arg)
{
	int wait_low;
	int wait_high;
	int wait_low1;

	int low_time[40];
	int high_time[40];

	int i;
	int success_count = 10;
	unsigned int temperature_sum = 0;
	unsigned int humidity_sum = 0;
	uint8_t	first_run = 1;
	uint8_t	error_count = 10;

	while (1) {
		wait_low = 0;
		wait_high = 0;
		wait_low1 = 0;
		memset(low_time, 0, sizeof(low_time));
		memset(high_time, 0, sizeof(high_time));

	    bcm2835_gpio_fsel(DHT, DATA_OUT);
	
		bcm2835_gpio_write(DHT, LOW);
		usleep(1000);
		bcm2835_gpio_write(DHT, HIGH);
	    
		bcm2835_gpio_fsel(DHT, DATA_IN);

		// 等待拉低信号
		while (bcm2835_gpio_lev(DHT)) {
			if (++wait_low > 300000) {
				fprintf(stderr, "等待拉低信号超时\n");
				goto loop;
			}
		}
	
		// 等带拉高信号
		while (!bcm2835_gpio_lev(DHT)) {
			if (++wait_high > 300000) {
				fprintf(stderr, "等待拉高信号超时\n");
				goto loop;
			}
		}
	
		// 等待低信号
		while (bcm2835_gpio_lev(DHT)) {
			if (++wait_low1 > 300000) {
				fprintf(stderr, "等待拉低信号1超时\n");
				goto loop;
			}
		}
	
		for (i = 0; i < 40; i++) {
			// 低信号延时
			while (!bcm2835_gpio_lev(DHT)) {
				if (++low_time[i] > 30000) {
					fprintf(stderr, "[%d]读取-低信号延时1超时\n", i);
					goto loop;
				}
			}
	
			// 高信号延时1
			while (bcm2835_gpio_lev(DHT)) {
				if (++high_time[i] > 30000) {
					fprintf(stderr, "[%d]读取-高信号延时1超时\n", i);
					goto loop;
				}
			}
		}
	
		/*
		fprintf(stderr, "low_time: ");
		for (i = 0; i < 40; i++) {
			fprintf(stderr, "%d ", low_time[i]);
		}
		fprintf(stderr, "\n");
		*/

		fprintf(stderr, "high_time: ");
		for (i = 0; i < 40; i++) {
			fprintf(stderr, "%d ", high_time[i] > 200 ? 1 : 0);
		}
		fprintf(stderr, "\n");
	
		unsigned int test1 = 0;
		for (i = 0; i < 16; i++) {
			test1 <<= 1;
			if (high_time[i] > 200) {
				test1 |= 0x01;
			}
		}
	
		unsigned int test2 = 0;
		for (i = 16; i < 32; i++) {
			test2 <<= 1;
			if (high_time[i] > 200) {
				test2 |= 0x01;
			}
		}
	
		unsigned char checksum = 0;
		for (i = 32; i < 40; i++) {
			checksum <<= 1;
			if (high_time[i] > 200) {
				checksum |= 0x01;
			}
		}
	
		fprintf(stderr, "test1: %d[%04X], test2: %d[%04X]\n", test1, test1, test2, test2);
		unsigned char test3 = (test1 & 0xFF) + ((test1 >> 8) & 0xFF) + (test2 & 0xFF) + ((test2 >> 8) & 0xFF);
		fprintf(stderr, "test3: %02X, checksum: %02X\n", test3, checksum);
	
		if (test3 == checksum) {
			error_count = 0;

			if (success_count == 10) {
				success_count = 0;

				if (first_run) {
					first_run = 0;
					humidity_sum = test1 * 10.0;
					temperature_sum = (test2 & 0x7FFF) * 10.0;
				}

				humidity = humidity_sum / 100.0;
	
				temperature = temperature_sum / 100.0;
				if (test2 & 0x8000) {
					temperature = -temperature;
				}

				temperature_sum = 0;
				humidity_sum = 0;

			} else {
				success_count++;
				humidity_sum += test1;
				temperature_sum += (test2 & 0x7FFF);
			}

		} else {
			error_count++;
		}

	loop:
		error_count++;
		for (i = 0; i < 2; i++) {
			if (error_count >= 10) {
				error_count = 10;
				temp_hum_stat = (temp_hum_stat == '!') ? ' ' : '!';

			} else {
				temp_hum_stat = (temp_hum_stat == '.') ? ' ' : '.';
			}
			sleep(1);
		}
	}

    bcm2835_close();
    return 0;
}

