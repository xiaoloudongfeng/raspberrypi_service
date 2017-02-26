#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/epoll.h>

#include "srv_func.h"
#include "dht22.h"
#include "get_weather.h"
#include "system_usage.h"

#define LISTEN_PORT		2222

void *srv_func(void *arg)
{
	int					srv_fd, rc;
	struct sockaddr_in	addr;
	struct sockaddr_in	cli_addr;
	int					cli_fd;
	socklen_t			address_len;

	int					ep_fd;
	struct epoll_event	ev, events[512];
	int					i;
	int					send_bytes;
	char				send_buf[1024];

	srv_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (srv_fd < 0) {
		fprintf(stderr, "socket() failed\n");

		goto loop;
	}

	if (fcntl(srv_fd, F_SETFL, fcntl(srv_fd, F_GETFL)|O_NONBLOCK) < 0) {
		fprintf(stderr, "fcntl() failed\n");

		goto loop;
	}

	addr.sin_family = AF_INET;
	addr.sin_port = htons(LISTEN_PORT);
	inet_pton(AF_INET, "0.0.0.0", &addr.sin_addr);

	if (bind(srv_fd, (const struct sockaddr *)&addr, sizeof(addr)) < 0) {
		fprintf(stderr, "bind() failed\n");

		goto loop;
	}

	if (listen(srv_fd, 512) < 0) {
		fprintf(stderr, "listen() failed\n");

		goto loop;
	}

	ep_fd = epoll_create(512);
	if (ep_fd < 0) {
		fprintf(stderr, "epoll_create() failed\n");

		goto loop;
	}

	ev.events = EPOLLIN | EPOLLET;
	ev.data.fd = srv_fd;
	if (epoll_ctl(ep_fd, EPOLL_CTL_ADD, srv_fd, &ev) == -1) {
		fprintf(stderr, "epoll_ctl() failed\n");

		goto loop1;
	}

	while (1) {
		rc = epoll_wait(ep_fd, events, 512, -1);
		
		fprintf(stderr, "epoll_wait()[%d]\n", rc);
		
		if (rc < 0) {
			fprintf(stderr, "epoll_wait() failed\n");

			goto loop1;
		}

		for (i = 0; i < rc; i++) {
			if (events[i].data.fd == srv_fd) {
				address_len = sizeof(cli_addr);
				memset(&cli_addr, 0, sizeof(cli_addr));

				cli_fd = accept(srv_fd, (struct sockaddr *)&cli_addr, &address_len);
				if (cli_fd < 0) {
					fprintf(stderr, "accept() failed\n");

					continue;
				}

				ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
				ev.data.fd = cli_fd;
				if (epoll_ctl(ep_fd, EPOLL_CTL_ADD, cli_fd, &ev) == -1) {
					fprintf(stderr, "epoll_ctl() failed\n");
					
					close(cli_fd);
					continue;
				}

				memset(send_buf, 0, sizeof(send_buf));
				sprintf(send_buf, "天气[%s]\n"
						  "室温[%5.1lf℃]湿度[%5.1lf%%%c]\n"
						   "CPU[%5.1lf%%]内存[%5.1lf%%%c]\n", 
						   weather, temperature, humidity, 
						   temp_hum_stat, cpu_usage, mem_usage, weather_stat);
				
				send_bytes = send(cli_fd, send_buf, strlen(send_buf), 0);
				
				fprintf(stderr, "send()[%d bytes]\n", send_bytes);
				
				if (send_bytes < 0) {
					fprintf(stderr, "send() failed\n");

					close(cli_fd);
					continue;
				}

			} else {
				cli_fd = events[i].data.fd;
				
				if (events[i].events & EPOLLRDHUP) {
					fprintf(stderr, "socket closed by client\n");
				}

				close(cli_fd);
			}
		}
	}

loop1:
	close(ep_fd);
loop:
	close(srv_fd);

	return NULL;
}

/*
int main(void)
{
	srv_func(NULL);

	return 0;
}
*/


