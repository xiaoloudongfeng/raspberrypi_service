#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "get_weather.h"

#define REQUEST		"GET /weather1d/101191101.shtml HTTP/1.1\r\n"												\
					"Host: www.weather.com.cn\r\n"																\
					"Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\n"	\
					"Upgrade-Insecure-Requests: 1\r\n"															\
					"User-Agent: Mozilla/5.0 (Windows NT 10.0; WOW64) AppleWebKit/537.36 (KHTML, like Gecko)"	\
					"Chrome/48.0.2564.109 Safari/537.36\r\n"													\
					"Connection: keep-alive\r\n"																\
					"Accept-Language: en-US,en;q=0.8,zh-CN;q=0.6,zh;q=0.4\r\n"									\
					"\r\n"
		
#define MATCH_STR	"<input type=\"hidden\" id=\"hidden_title\" value=\""

char weather[512] = {};
char weather_stat = '!';         // get_weather_func stat

void *get_weather_func(void *arg)
{
	int					fd;
	struct sockaddr_in	addr;
	short				dest_port = 80;
	struct timeval		timeout   = {1, 0};

	int					rc;
	
	unsigned int		recv_off, buf_len;
	unsigned char		recv_buf[1024 * 512];

	int					i;
	char			   *prev;
	char			   *curr;
	int					word_count;

	while (1) {
		fd = socket(AF_INET, SOCK_STREAM, 0);
		if (fd < 0) {
			fprintf(stderr, "socket() failed\n");
			goto loop;
		}
	
		// www.weather.com.cn 222.186.17.100
		addr.sin_family = AF_INET;
		addr.sin_port = htons(dest_port);
		inet_pton(AF_INET, "222.186.17.100", &addr.sin_addr);
		
		rc = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
		if (rc < 0) {
			fprintf(stderr, "connect() failed\n");
			goto loop;
		}
		
		rc = setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));
		if (rc < 0) {
			fprintf(stderr, "setsockopt() failed\n");
			goto loop;
		}
	
		rc = send(fd, REQUEST, strlen(REQUEST), 0);
		if (rc < 0) {
			fprintf(stderr, "send() failed\n");
			goto loop;
		}
	
		recv_off = 0;
		buf_len = sizeof(recv_buf);
		memset(recv_buf, 0, sizeof(recv_buf));

		while (buf_len) {
			rc = recv(fd, recv_buf + recv_off, buf_len, 0);
			if (rc <= 0) {
				fprintf(stderr, "recv() failed, errno: %s\n", strerror(errno));
				if (recv_off <= 0) {
					goto loop;
				}
				break;
			}

			recv_off += rc;
			buf_len -= rc;
		}

		// DEGUG INFO
		// fprintf(stderr, "%s", recv_buf);
		// fprintf(stderr, "recv_off = %d\n", recv_off);
		// fprintf(stderr, "strlen(recv_buf): %d\n", strlen((const char *)recv_buf));
	
		prev = strstr((const char *)recv_buf, MATCH_STR);
		if (prev == NULL) {
			fprintf(stderr, "match failed\n");
			goto loop;
		}

		curr = prev;
		while (*curr != '\n' && *curr != '\r' && *curr) {
			curr++;
		}
		*curr = '\0';
		
		// fprintf(stderr, "prev: %s\n", prev);	// DEBUG INFO

		word_count = 0;

		for (curr = prev; *curr; curr++) {
			if (*curr == ' ' || *curr == '\t') {
				if (curr != prev) {					// found a word
					word_count++;
					if (word_count == 6) {
						*curr = '\0';
						
						fprintf(stderr, "********天气[%s]********\n", prev);
						
						memset(weather, 0, sizeof(weather));
						strcpy(weather, prev);
						
						weather_stat = '.';
						
						break;
					}
				}
				prev = curr + 1;
			}
		}
	
	loop:
		if (word_count != 6) {
			for (i = 0; i < 5; i++) {
				weather_stat = weather_stat == '!' ? ' ' : '!';
				sleep(1);
			}

		} else {
			for (i = 0; i < 30; i++) {
				weather_stat = weather_stat == '.' ? ' ' : '.';
				sleep(1);
			}
		}
		
		if (fd >= 0) {
			close(fd);
			fd = -1;
		}
	}
}
