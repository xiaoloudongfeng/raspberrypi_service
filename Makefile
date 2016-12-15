all:
	gcc lcd.c get_weather.c -lbcm2835 -lpthread -o lcd -Wall
