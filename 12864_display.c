#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <iconv.h>

#include <bcm2835.h>

#include "12864_display.h"

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

#define LIGHT		RPI_V2_GPIO_P1_05

#define	DATA_IN		BCM2835_GPIO_FSEL_INPT
#define	DATA_OUT	BCM2835_GPIO_FSEL_OUTP

static int code_convert(char *from, char *to, char *in, size_t *inlen, char *out, size_t *outlen)
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

static int u2g(char *in, size_t inlen, char *out, size_t outlen)
{
	return code_convert("utf-8", "gb2312", in, &inlen, out, &outlen);
}

static void check_busy(void)
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

static void lcd_write_data(unsigned char data)
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

static void lcd_write_cmd(unsigned char cmd)
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

void lcd_light_ctl(int ctl)
{
	if (ctl) {
		bcm2835_gpio_write(LIGHT, HIGH);
	
	} else {
		bcm2835_gpio_write(LIGHT, LOW);
	}
}

void lcd_init(void)
{
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
