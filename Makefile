all:
	gcc 12864_display.c dht22.c get_weather.c lcd.c system_usage.c srv_func.c cJSON.c -lbcm2835 -lpthread -lm -o lcd -Wall

clean:
	rm lcd
