#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

#include "srv_func.h"
#include "dht22.h"
#include "get_weather.h"
#include "system_usage.h"

#define LISTEN_PORT     2222

extern struct tm  *tm;

static char *weeks[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static char *months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", 
                          "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

#define RESPONSE_HEADER "HTTP/1.1 200 OK\r\n"                                   \
                        "Server: Raspberry pi 2\r\n"                            \
                        "Date: %s, %02d %s %4d %02d:%02d:%02d GMT\r\n"          \
                        "Content-Type: application/json\r\n"                    \
                        "Last-Modified: %s, %02d %s %4d %02d:%02d:%02d GMT\r\n" \
                        "Transfer-Encoding: chunked\r\n"                        \
                        "Connection: close\r\n"                                 \
                        "\r\n"                                                  \
                        "%x\r\n"                                                \
                        "%s\r\n"                                                \
                        "0\r\n\r\n"

void static fill_resp_buf(char *resp_buf)
{
    char    send_buf[1024] = {};
    int     send_buf_len;

    sprintf(send_buf, "{\n"
                        "\"weather\": \"%s\",\n"
                        "\"temperature\": %lf,\n"
                        "\"humidity\": %lf,\n"
                        "\"cpu_usage\": %lf,\n"
                        "\"mem_usage\": %lf,\n"
                        "\"temp_hum_stat\": \"%c\",\n"
                        "\"weather_stat\": \"%c\"\n"
                        "}", weather, temperature, humidity, 
                        cpu_usage, mem_usage, temp_hum_stat, 
                        weather_stat);

    send_buf_len = strlen(send_buf);

    sprintf(resp_buf, RESPONSE_HEADER, 
        weeks[tm->tm_wday], tm->tm_mday, months[tm->tm_mon], tm->tm_year + 1900, 
        tm->tm_hour, tm->tm_min, tm->tm_sec, 
        weeks[tm->tm_wday], tm->tm_mday, months[tm->tm_mon], tm->tm_year + 1900, 
        tm->tm_hour, tm->tm_min, tm->tm_sec, 
        send_buf_len, send_buf);
}

void *srv_func(void *arg)
{
    int                 srv_fd, rc;
    struct sockaddr_in  addr;
    struct sockaddr_in  cli_addr;
    int                 cli_fd;
    socklen_t           address_len;

    int                 ep_fd;
    struct epoll_event  ev, events[512];
    int                 i;

    char                recv_buf[2048];
    int                 recv_len;
    char                resp_buf[2048];
    int                 send_len;
    
    int                 flag;

    signal(SIGPIPE, SIG_IGN);

    srv_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (srv_fd < 0) {
        fprintf(stderr, "socket() failed\n");

        goto loop;
    }

    fprintf(stderr, "get srv_fd[%d]\n", srv_fd);

    if (fcntl(srv_fd, F_SETFL, fcntl(srv_fd, F_GETFL)|O_NONBLOCK) < 0) {
        fprintf(stderr, "fcntl() failed\n");

        goto loop;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(LISTEN_PORT);
    inet_pton(AF_INET, "0.0.0.0", &addr.sin_addr);

    flag = 1;
    if (setsockopt(srv_fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(int)) < 0) {
        fprintf(stderr, "setsockopt() failed\n");

        goto loop;
    }

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

    fprintf(stderr, "get ep_fd[%d]\n", ep_fd);

    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = srv_fd;
    if (epoll_ctl(ep_fd, EPOLL_CTL_ADD, srv_fd, &ev) == -1) {
        fprintf(stderr, "epoll_ctl() failed\n");

        goto loop1;
    }

    for ( ;; ) {

        fprintf(stderr, "\n\n");

        memset(events, 0, sizeof(events));
        rc = epoll_wait(ep_fd, events, 512, -1);
        
        fprintf(stderr, "epoll_wait()[%d]\n", rc);
        
        if (rc < 0) {
            fprintf(stderr, "epoll_wait() failed\n");

            goto loop1;
        }

        for (i = 0; i < rc; i++) {
            if (events[i].data.fd == srv_fd) {
                
                fprintf(stderr, "get event of srv_fd[%d]\n", srv_fd);
                
                for ( ;; ) {
                    address_len = sizeof(cli_addr);
                    memset(&cli_addr, 0, sizeof(cli_addr));
                    
                    cli_fd = accept(srv_fd, (struct sockaddr *)&cli_addr, &address_len);
                    if (cli_fd < 0) {
                        if (errno != EAGAIN && errno != EINTR) {
                            fprintf(stderr, "accept() failed\n");
                        }

                        break;
                    }
                    
                    fprintf(stderr, "accept client fd[%d]\n", cli_fd);
                    
                    if (fcntl(cli_fd, F_SETFL, fcntl(cli_fd, F_GETFL)|O_NONBLOCK) < 0) {
                        fprintf(stderr, "fcntl() failed\n");
                        
                        close(cli_fd);
                        continue;
                    }

                    ev.events = EPOLLIN|EPOLLET|EPOLLRDHUP;
                    ev.data.fd = cli_fd;
                    if (epoll_ctl(ep_fd, EPOLL_CTL_ADD, cli_fd, &ev) == -1) {
                        fprintf(stderr, "epoll_ctl() failed\n");
                        
                        close(cli_fd);
                        continue;
                    }
                }

            } else {
                cli_fd = events[i].data.fd;

                fprintf(stderr, "get event of cli_fd[%d]\n", cli_fd);
                
                if (events[i].events & EPOLLRDHUP) {
                    fprintf(stderr, "get EPOLLRDHUP\n");
                    
                    goto fail;
                }

                if (events[i].events & EPOLLIN) {
                    fprintf(stderr, "get EPOLLIN\n");

                    for ( ;; ) {
                        memset(recv_buf, 0, sizeof(recv_buf));

                        recv_len = recv(cli_fd, recv_buf, sizeof(recv_buf), 0);
                        if (recv_len <= 0) {
                            if (errno != EAGAIN && errno != EINTR) {
                                fprintf(stderr, "recv() failed\n");
                                
                                goto fail;
                            }
                            
                            ev.events = EPOLLOUT|EPOLLET|EPOLLRDHUP;
                            ev.data.fd = cli_fd;
                            if (epoll_ctl(ep_fd, EPOLL_CTL_MOD, cli_fd, &ev) == -1) {
                                fprintf(stderr, "epoll_ctl() failed\n");
                                
                                goto fail;
                            }

                            break;
                        }
                    }
                }

                if (events[i].events & EPOLLOUT) {
                    fprintf(stderr, "get EPOLLOUT\n");

                    memset(resp_buf, 0, sizeof(resp_buf));

                    fill_resp_buf(resp_buf);
                    //fprintf(stderr, "%s\n", resp_buf);

                    send_len = send(cli_fd, resp_buf, strlen(resp_buf), 0);
                    fprintf(stderr, "send()[%d bytes]\n", send_len);
                    if (send_len <= 0) {
                        fprintf(stderr, "send() failed\n");
                        
                        goto fail;
                    }
                }
                continue;

            fail:
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


