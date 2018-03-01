#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/rand.h>

#include "cJSON.h"

#include "get_weather.h"

#define REQUEST     "GET /s6/weather/now?location=CN101191101&key=565ab5c62bb54a4a9e6d37104aa9136b HTTP/1.1\r\n"    \
                    "Host: free-api.heweather.com\r\n"                                                              \
                    "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\n"        \
                    "Upgrade-Insecure-Requests: 1\r\n"                                                              \
                    "User-Agent: Mozilla/5.0 (Windows NT 10.0; WOW64) AppleWebKit/537.36 (KHTML, like Gecko)"       \
                    "Chrome/48.0.2564.109 Safari/537.36\r\n"                                                        \
                    "Connection: keep-alive\r\n"                                                                    \
                    "Accept-Language: en-US,en;q=0.8,zh-CN;q=0.6,zh;q=0.4\r\n"                                      \
                    "\r\n"

char json_weather[1024] = { 0 };
char weather[512] = { 0 };
char weather_stat = '!';         // get_weather_func stat

/*
{
	"HeWeather6": [{
		"basic": {
			"cid": "CN101191101",
			"location": "常州",
			"parent_city": "常州",
			"admin_area": "江苏",
			"cnty": "中国",
			"lat": "31.77275276",
			"lon": "119.94697571",
			"tz": "+8.0"
		},
		"update": {
			"loc": "2018-02-27 19:50",
			"utc": "2018-02-27 11:50"
		},
		"status": "ok",
		"now": {
			"cloud": "8",
			"cond_code": "104",
			"cond_txt": "阴",
			"fl": "6",
			"hum": "61",
			"pcpn": "0.0",
			"pres": "1014",
			"tmp": "14",
			"vis": "10",
			"wind_deg": "136",
			"wind_dir": "东南风",
			"wind_sc": "3-4",
			"wind_spd": "12"
		}
	}]
}
*/

typedef struct {
	char *name;
	char *alias;
	char value[64];
} kv_t;

void iterate_json(cJSON *root, kv_t *kv_array, int kv_array_len)
{
	int i, j;

	for (i = 0; i < cJSON_GetArraySize(root); i++) {
		cJSON *item = cJSON_GetArrayItem(root, i);
		if (item->type & (cJSON_Object|cJSON_Array)) {
			iterate_json(item, kv_array, kv_array_len);

		} else {
			for (j = 0; j < kv_array_len; j++) {
				if (!strcmp(item->string, kv_array[j].name)) {
					strcpy(kv_array[j].value, item->valuestring);
				}
			}
		}
	}
}

int parse_json(const char *json_str)
{
	char tmp_str[1024];
	int i;

	kv_t kv_array[] = { {"location", "", { 0 }}, 
			   {"cond_txt", "", { 0 }}, 
			   {"tmp", "温度: ", { 0 }}, 
			   {"fl", "体感: ", { 0 }}, 
			   {"hum", "湿度: ", { 0 }}, 
			   {"wind_dir", "风向: ", { 0 }}, 
			   {"wind_sc", "风力: ", { 0 }}};
	
	cJSON *root = cJSON_Parse(json_str);
	if (root == NULL) {
		fprintf(stderr, "cJSON_Parse() failed\n");
		return -1;
	}

	iterate_json(root, kv_array, sizeof(kv_array) / sizeof(kv_t));

	memset(tmp_str, 0, sizeof(tmp_str));
	for (i = 0; i < sizeof(kv_array) / sizeof(kv_t); i++) {
		if (!strlen(kv_array[i].value)) {
			fprintf(stderr, "didn't get %s, iterate_json() failed\n", kv_array[i].name);
			return -1;
		}
		strcat(tmp_str, kv_array[i].alias);
		strcat(tmp_str, kv_array[i].value);
		strcat(tmp_str, ", ");
	}

	// location
	strcpy(weather, kv_array[0].value);
	// cond
	strcat(weather, kv_array[1].value);
	fprintf(stderr, "weather:\n%s\n", weather);
	
	return 0;
}

void *get_weather_func(void *arg)
{
    int                 fd;
    struct sockaddr_in  addr;
    short               dest_port = 443;
    struct timeval      timeout   = {1, 0};
    SSL_CTX            *ctx;
    SSL                *connection;

    int                 rc, i;
    
    size_t				recv_off, buf_len;
    char				recv_buf[1024 * 512];


    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    ctx = SSL_CTX_new(SSLv23_method());
    if (ctx == NULL) {
        fprintf(stderr, "SSL_CTX_new() failed\n");
		return NULL;
    }

    connection = SSL_new(ctx);
    if (connection == NULL) {
        fprintf(stderr, "SSL_new() failed\n");
		SSL_CTX_free(ctx);
		return NULL;
    }

    for ( ;; ) {
        if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            fprintf(stderr, "socket() failed\n");
			rc = -1;
            goto loop;
        }
   
		// free-api.heweather.com -> 116.196.90.224
        addr.sin_family = AF_INET;
        addr.sin_port = htons(dest_port);
        inet_pton(AF_INET, "116.196.90.224", &addr.sin_addr);
        
        rc = connect(fd, (struct sockaddr *) &addr, sizeof(addr));
        if (rc < 0) {
            fprintf(stderr, "connect() failed\n");
            goto loop;
        }
        
        rc = setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char *) &timeout, sizeof(timeout));
        if (rc < 0) {
            fprintf(stderr, "setsockopt() failed\n");
            goto loop;
        }

        rc = SSL_set_fd(connection, fd);
        if (rc < 0) {
            fprintf(stderr, "SSL_set_fd() failed\n");
            goto loop;
        }

		SSL_set_connect_state(connection);

        rc = SSL_connect(connection);
        if (rc < 0) {
            fprintf(stderr, "SSL_connect() failed\n");
            goto loop;
        }

        rc = SSL_write(connection, REQUEST, strlen(REQUEST));
        if (rc < 0) {
            fprintf(stderr, "SSL_write() failed\n");
            goto loop;
        }
    
        recv_off = 0;
        buf_len = sizeof(recv_buf);
        memset(recv_buf, 0, sizeof(recv_buf));
        while (buf_len) {
            rc = SSL_read(connection, recv_buf + recv_off, buf_len);
			if (rc <= 0) {
                if (recv_off == 0) {
					rc = -1;
                    fprintf(stderr, "SSL_read() failed, errno: %s\n", strerror(errno));
                    goto loop;
                } else {
					rc = 0;
				}
                break;
            }

            recv_off += rc;
            buf_len -= rc;
        }
        fprintf(stderr, "recv_off: %zu\n%s\n", recv_off, recv_buf);
		
		char *json_str = strstr(recv_buf, "\r\n\r\n");
		if (json_str == NULL) {
			fprintf(stderr, "get json str failed\n");
			rc = -1;
			goto loop;
		}
		// for web
		strcpy(json_weather, json_str + 4);
		// for lcd
		parse_json(json_str + 4);
    
    loop:
        if (rc < 0) {
            for (i = 0; i < 30; i++) {
                weather_stat = weather_stat == '!' ? ' ' : '!';
                sleep(1);
            }

        } else {
            for (i = 0; i < 60 * 2; i++) {
                weather_stat = weather_stat == '.' ? ' ' : '.';
                sleep(1);
            }
        }
        
        if (fd >= 0) {
            close(fd);
            fd = -1;
        }
    }

	SSL_free(connection);
	SSL_CTX_free(ctx);

	return NULL;
}

/*
int main(int argc, char *argv[])
{
    get_weather_func(NULL);

    return 0;
}
*/

