#include <bcm2835.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <iconv.h>
#include <time.h>
#include <pthread.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "get_weather.h"

/*
	PIN		GPIO			LCD

	08		GPIO14	--->	RST

	10		GPIO15	--->	DB7
	12		GPIO18	--->	DB6
	16		GPIO23	--->	DB5
	18		GPIO24	--->	DB4
	22		GPIO25	--->	DB3
	24		GPIO8	--->	DB2
	26		GPIO7	--->	DB1
	32		GPIO12	--->	DB0

	36		GPIO16	--->	EN
	38		GPIO20	--->	R/W
	40		GPIO21	--->	RS
*/

#define DATA7		RPI_V2_GPIO_P1_10
#define	DATA6		RPI_V2_GPIO_P1_12
#define	DATA5		RPI_V2_GPIO_P1_16
#define	DATA4		RPI_V2_GPIO_P1_18
#define	DATA3		RPI_V2_GPIO_P1_22
#define	DATA2		RPI_V2_GPIO_P1_24
#define	DATA1		RPI_V2_GPIO_P1_26
#define	DATA0		RPI_V2_GPIO_P1_32

#define	RST			RPI_V2_GPIO_P1_08
#define EN			RPI_V2_GPIO_P1_36
#define	RW			RPI_V2_GPIO_P1_38
#define RS			RPI_V2_GPIO_P1_40

#define NUM			8

#define DHT			RPI_V2_GPIO_P1_03

#define LIGHT		RPI_V2_GPIO_P1_05

#define	DATA_IN		BCM2835_GPIO_FSEL_INPT
#define	DATA_OUT	BCM2835_GPIO_FSEL_OUTP

double		temperature = 0.0;
double		humidity = 0.0;
char		temp_hum_stat = '!';		// temp_hum_func stat
double		cpu_usage = 0.0;
double		mem_usage = 0.0;

void *mem_usage_func(void *arg) 
{
	uint32_t	mem_total;
	uint32_t	mem_available;
	int			fd = -1;
	char		mem_buf[128] = {}, *curr, *prev;
	int			i = 0;
	uint8_t		flag = 0;

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
				if (flag) {				// find whole number
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
	char		stat_buf[512] = {}, *curr, *prev;
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

void *temp_hum_func(void *arg)
{
	int wait_low = 0;
	int wait_high = 0;
	int wait_low1 = 0;

	int low_time[40] = { 0 };
	int high_time[40] = { 0 };
	int i = 0;
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

int code_convert(char *from, char *to, char *in, size_t *inlen, char *out, size_t *outlen)
{
	iconv_t cd;

	cd = iconv_open(to, from);
	if (cd == 0) {
		return -1;
	}

	if (iconv(cd, &in, inlen, &out, outlen) == -1) {
		return -1;
	}

	iconv_close(cd);
	return 0;
}

int u2g(char *in, size_t inlen, char *out, size_t outlen)
{
	return code_convert("utf-8", "gb2312", in, &inlen, out, &outlen);
}

int gpio_init(void)
{
	if (!bcm2835_init()) {
		fprintf(stderr, "bcm2835_init() failed\n");
		return -1;
	}

    bcm2835_gpio_fsel(DATA0, DATA_OUT);
    bcm2835_gpio_fsel(DATA1, DATA_OUT);
    bcm2835_gpio_fsel(DATA2, DATA_OUT);
    bcm2835_gpio_fsel(DATA3, DATA_OUT);
    bcm2835_gpio_fsel(DATA4, DATA_OUT);
    bcm2835_gpio_fsel(DATA5, DATA_OUT);
    bcm2835_gpio_fsel(DATA6, DATA_OUT);
    bcm2835_gpio_fsel(DATA7, DATA_OUT);

    bcm2835_gpio_fsel(EN, DATA_OUT);
    bcm2835_gpio_fsel(RW, DATA_OUT);
    bcm2835_gpio_fsel(RS, DATA_OUT);
    bcm2835_gpio_fsel(RST, DATA_OUT);

	bcm2835_gpio_fsel(LIGHT, DATA_OUT);

	return 0;
}

void check_busy(void)
{
	bcm2835_gpio_set_pud(DATA7, BCM2835_GPIO_PUD_UP);
	bcm2835_gpio_fsel(DATA7, DATA_IN);
	// cmd
	bcm2835_gpio_write(RS, LOW);
	// read
	bcm2835_gpio_write(RW, HIGH);
	// enable
	bcm2835_gpio_write(EN, HIGH);

	while (bcm2835_gpio_lev(DATA7));
	// disable
	bcm2835_gpio_write(EN, LOW);
	
	bcm2835_gpio_set_pud(DATA7, BCM2835_GPIO_PUD_OFF);
	bcm2835_gpio_fsel(DATA7, DATA_OUT);
}

void lcd_write_data(unsigned char data)
{
	int				i = 0;
	unsigned char	flags[NUM] = {};

	check_busy();

	// data
	bcm2835_gpio_write(RS, HIGH);
	// write
	bcm2835_gpio_write(RW, LOW);
	// enable
	bcm2835_gpio_write(EN, HIGH);

	for (i = 0; i < NUM; i++) {
		flags[i] = ((data >> i) & 0x01);
	}
/*
	fprintf(stderr, "data: ");
	for (i = NUM - 1; i >= 0; i--) {
		fprintf(stderr, "%d ", flags[i]);
	}
	fprintf(stderr, "\n");
*/
	bcm2835_gpio_write(DATA0, flags[0]);
	bcm2835_gpio_write(DATA1, flags[1]);
	bcm2835_gpio_write(DATA2, flags[2]);
	bcm2835_gpio_write(DATA3, flags[3]);
	bcm2835_gpio_write(DATA4, flags[4]);
	bcm2835_gpio_write(DATA5, flags[5]);
	bcm2835_gpio_write(DATA6, flags[6]);
	bcm2835_gpio_write(DATA7, flags[7]);
	
	// delay 50us
	usleep(50);
	// disable
	bcm2835_gpio_write(EN, LOW);
}

void lcd_write_cmd(unsigned char cmd)
{
	int				i = 0;
	unsigned char	flags[NUM] = {};

	check_busy();

	// cmd 
	bcm2835_gpio_write(RS, LOW);
	// write
	bcm2835_gpio_write(RW, LOW);
	// enable
	bcm2835_gpio_write(EN, HIGH);

	for (i = 0; i < NUM; i++) {
		flags[i] = ((cmd >> i) & 0x01);
	}
/*
	fprintf(stderr, "cmd: ");
	for (i = NUM - 1; i >= 0; i--) {
		fprintf(stderr, "%d ", flags[i]);
	}
	fprintf(stderr, "\n");
*/
	bcm2835_gpio_write(DATA0, flags[0]);
	bcm2835_gpio_write(DATA1, flags[1]);
	bcm2835_gpio_write(DATA2, flags[2]);
	bcm2835_gpio_write(DATA3, flags[3]);
	bcm2835_gpio_write(DATA4, flags[4]);
	bcm2835_gpio_write(DATA5, flags[5]);
	bcm2835_gpio_write(DATA6, flags[6]);
	bcm2835_gpio_write(DATA7, flags[7]);

	// delay 50us
	usleep(50);
	// disable
	bcm2835_gpio_write(EN, LOW);
}

void lcd_init(void)
{
	usleep(20);
	bcm2835_gpio_write(RST, HIGH);
	usleep(20);
	bcm2835_gpio_write(RST, LOW);
	usleep(20);
	bcm2835_gpio_write(RST, HIGH);
	
	lcd_write_cmd(0x38);		// 8bit 2line 5*7
	usleep(20);
	lcd_write_cmd(0x01);
	usleep(20);
	lcd_write_cmd(0x0C);
	usleep(500);
}

void lcd_print(char *data)
{
	char	out[256] = { 0 }, *p = out;
	size_t	inlen = strlen(data);
	size_t	outlen = 256;
	int		i = 0;
	unsigned char lines[4] = { 0x80, 0x90, 0x88, 0x98 };


	u2g(data, inlen, out, outlen);

	lcd_write_cmd(0x80);

	while (*p) {
		if (*p == '\n') {
			++i;
			++p;
			if (i >= 4) {
				break;
			}
			lcd_write_cmd(lines[i]);
			continue;
		}
		lcd_write_data(*p);
		++p;
	}
}

int main(int argc, char **argv)
{
	char		print_buf[256] = { 0 };
	char	   *weeks[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
	time_t		timevalue;
	struct tm  *tm;
	int			rc;
	pthread_t	temp_hum_pt, cpu_usage_pt, mem_usage_pt, weather_pt;
	pthread_attr_t attr;
	char		roll_weather[512];
	int			off = 0;
	char	   *curr_dis = roll_weather + 6;
	char	   *curr_wea = weather;

	if (argc >= 2 && !strcmp(argv[1], "-d")) {
		daemon(1, 0);
	}

	rc = gpio_init();
	if (rc < 0) {
		return -1;
	}

	lcd_init();

	rc = pthread_attr_init(&attr);
	if (rc < 0) {
		fprintf(stderr, "phtread_attr_init() failed\n");
		return -1;
	}

	rc = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	if (rc < 0) {
		fprintf(stderr, "pthread_attr_setdetachstate() failed\n");
		return -1;
	}

	rc = pthread_create(&temp_hum_pt, &attr, temp_hum_func, NULL);
	if (rc < 0) {
		fprintf(stderr, "pthread_create() failed\n");
		return -1;
	}

	rc = pthread_create(&cpu_usage_pt, &attr, cpu_usage_func, NULL);
	if (rc < 0) {
		fprintf(stderr, "pthread_create() failed\n");
		return -1;
	}

	rc = pthread_create(&mem_usage_pt, &attr, mem_usage_func, NULL);
	if (rc < 0) {
		fprintf(stderr, "pthread_create() failed\n");
		return -1;
	}

	rc = pthread_create(&weather_pt, &attr, get_weather_func, NULL);
	if (rc < 0) {
		fprintf(stderr, "pthread_create() failed\n");
		return -1;
	}

	rc = pthread_attr_destroy(&attr);
	if (rc < 0) {
		fprintf(stderr, "pthread_attr_destroy() failed\n");
		return -1;
	}

	//strcpy(weather, "阴转小");	//测试用

	while (1) {
		timevalue = time(NULL);

		tm = localtime(&timevalue);

		if (tm->tm_hour >= 22 || tm->tm_hour < 7) {
			bcm2835_gpio_write(LIGHT, LOW);

		} else {
			bcm2835_gpio_write(LIGHT, HIGH);
		}

		memset(roll_weather, 0x20, sizeof(roll_weather));
		if (strlen(weather) / 3 >= 4) {
			if (curr_dis > roll_weather) {				// 未显示完全, 前面的空格必须是偶数个
				strcpy(curr_dis, curr_wea);
				curr_dis -= 2;
			} else {									// 显示完全
				strcpy(roll_weather, curr_wea);
				off = strlen(roll_weather);
				if (off < 12) {
					strcpy(curr_dis + off, "    ");
				}
				curr_wea += 3;
				if (*curr_wea == '\0') {
					curr_dis = roll_weather + 6;
					curr_wea = weather;
				}
			}

		} else {
			curr_dis = roll_weather + 6;
			curr_wea = weather;
			
			off = strlen(weather);
			
			strcpy(roll_weather + (4 - off / 3) * 2, weather);
		}

		sprintf(print_buf, 
				"[%04d/%02d/%02d %s]\n"
				"%02d:%02d:%02d%s\n"
				"T/H%5.1lfC/%4.1lf%%%c\n"
				"C/M%5.1lf%%/%4.1lf%%%c\n", 
				tm->tm_year + 1900, tm->tm_mon + 1, 
				tm->tm_mday, weeks[tm->tm_wday], 
				tm->tm_hour, tm->tm_min, tm->tm_sec, 
				roll_weather, temperature, humidity, 
				temp_hum_stat, cpu_usage, mem_usage, weather_stat);
		lcd_print(print_buf);

		fprintf(stderr, "%s", print_buf);
		sleep(1);
	}

    bcm2835_close();
    return 0;
}

