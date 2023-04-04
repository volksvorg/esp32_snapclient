/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2020 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <stdio.h>
#include <string.h>

#include "lwip/sockets.h"
#include "esp_transport_tcp.h"
#include "esp_log.h"
#include "esp_err.h"
#include "audio_mem.h"
#include "snapcast_stream.h"


#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/task.h"

static const char *TAG = "SNAPCAST_STREAM";


#define TIMEOUT_SEC 5
/*
typedef struct snapcast_stream_ringbuffer_bits{
	unsigned new_wire_chunk:1;
	unsigned enabled:1;
	unsigned sync:1;
	unsigned :13;
}snapcast_stream_ringbuffer_bits_t;
*/

typedef struct snapcast_stream_ringbuffer_node{
	int position;
	int data_size;
	int ringbuffer_size;
	int64_t timestamp;
	//char data[SNAPCAST_STREAM_RINGBUFFER_SIZE];
	char data[SNAPCAST_STREAM_RINGBUFFER_SIZE ];
	struct snapcast_stream_ringbuffer_node *last;
	struct snapcast_stream_ringbuffer_node *next;
}snapcast_stream_ringbuffer_node_t;


typedef struct snapcast_stream {
    esp_transport_handle_t        		t;
    audio_stream_type_t           		type;
    int                           		sock;
    int                           		port;
    char                          		*host;
    bool                          		is_open;
    int                           		timeout_ms;
    snapcast_stream_event_handle_cb     hook;
    void                                *ctx;
    bool  								received_header;
   	struct timeval 						last_sync;
   	int 								id_counter;
	struct timeval 						server_uptime;
   	char 								*audio_buffer;
   	base_message_t 						base_message;
   	codec_header_message_t 				codec_header_message;
   	wire_chunk_message_t 				wire_chunk_message;
   	server_settings_message_t 			server_settings_message;
   	time_message_t 						time_message;
   	snapcast_stream_status_t            state;
   	snapcast_stream_ringbuffer_node_t   *rb;
   	snapcast_stream_ringbuffer_node_t   *write_rb;
   	snapcast_stream_ringbuffer_node_t   *read_rb;
 /*
   	union{
   		uint16_t bits_buffer;
   		snapcast_stream_ringbuffer_bits_t bits;
   	};*/
} snapcast_stream_t;

char mac_address[18];
unsigned char base_mac[6];
char base_message_serialized[BASE_MESSAGE_SIZE];
char time_message_serialized[TIME_MESSAGE_SIZE];

int create_socket(void) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        printf("Could not create socket: %s\n", strerror(errno));
        exit(1);
    }
    return sock;
}

void set_socket_non_blocking(int sock) {
    fcntl(sock, F_SETFL, O_NONBLOCK);
}

void connect_to_server(int sock, const char *ip_addr, int port) {
    struct sockaddr_in server;
    server.sin_addr.s_addr = inet_addr(ip_addr);
    server.sin_family = AF_INET;
    server.sin_port = htons(port);

    int result = connect(sock, (struct sockaddr *)&server, sizeof(server));
    if (result == -1 && errno != EINPROGRESS) {
        printf("Connect failed: %s\n", strerror(errno));
        exit(1);
    }
}

int receive_data(int sock, char* buffer, int buffer_size) {
    int bytes_received = 0;
    int total_bytes_received = 0;
    int ret;
    struct timeval tv;
    fd_set readfds;

    while (total_bytes_received < buffer_size - 1) {
        // Set the timeout value
        tv.tv_sec = TIMEOUT_SEC;
        tv.tv_usec = 0;

        // Set up the file descriptor set for select()
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);

        // Wait until there is data to read or timeout occurs
        ret = select(sock+1, &readfds, NULL, NULL, &tv);
        if (ret == -1) {
            // Error occurred while waiting for data
            fprintf(stderr, "Error occurred while waiting for data: %s\n", strerror(errno));
            return -1;
        } else if (ret == 0) {
            // Timeout occurred
            fprintf(stderr, "Timeout occurred while waiting for data\n");
            return bytes_received;
        }

        // Try to receive some data
        int len = recv(sock, buffer + total_bytes_received , buffer_size - total_bytes_received - 1, 0);
        if (len == -1) {
            fprintf(stderr, "Error receiving data: %s\n", strerror(errno));
            return -1;
        } else if (len == 0) {
            // Connection closed by remote host
            fprintf(stderr, "Connection closed by remote host\n");
            return bytes_received;
        } else {
            // Data received
            total_bytes_received += len;
            bytes_received = total_bytes_received;
        }
    }
    return bytes_received;
}

void wait_for_connection(int sock) {
    time_t start_time, current_time;
    time(&start_time);

    while (1) {
        fd_set fdset;
        struct timeval tv;
        int res;

        FD_ZERO(&fdset);
        FD_SET(sock, &fdset);

        tv.tv_sec = TIMEOUT_SEC;
        tv.tv_usec = 0;

        res = select(sock + 1, NULL, &fdset, NULL, &tv);
        if (res < 0 && errno != EINTR) {
            printf("Error waiting for socket to connect: %s\n", strerror(errno));
            exit(1);
        } else if (res > 0) {
            int error;
            socklen_t errlen = sizeof(error);
            if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &errlen) == 0 && error == 0) {
                // Connection succeeded
                break;
            }
        } else if (res == 0) {
            // Timeout occurred
            printf("Connection timed out\n");
            exit(1);
        }

        time(&current_time);
        if (current_time - start_time >= TIMEOUT_SEC) {
            // Timeout occurred
            printf("Connection timed out\n");
            exit(1);
        }
    }
}

static int _get_socket_error_code_reason(const char *str, int sockfd)
{
    uint32_t optlen = sizeof(int);
    int result;
    int err;

    err = getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &result, &optlen);
    if (err == -1) {
        ESP_LOGE(TAG, "%s, getsockopt failed", str);
        return -1;
    }
    if (result != 0) {
        ESP_LOGW(TAG, "%s error, error code: %d, reason: %s", str, err, strerror(result));
    }
    return result;
}


static TimerHandle_t        send_time_tm_handle;
snapcast_stream_t           *sntp_snapcast;


static void log_socket_error(const char *tag, const int sock, const int err, const char *message){
    ESP_LOGE(tag, "[sock=%d]: %s\n error=%d: %s", sock, message, err, strerror(err));
}


static int _snapcast_stream_socket_send(const char *tag, const int sock, const char * data, const size_t len){
	int to_write = len;
	while (to_write > 0) {
		int written = send(sock, data + (len - to_write), to_write, 0);
		if (written < 0 && errno != EINPROGRESS && errno != EAGAIN && errno != EWOULDBLOCK) {
			log_socket_error(tag, sock, errno, "Error occurred during sending");
			return -1;
		}
		to_write -= written;
	}
	return len;
}

/*
static int _snapcast_stream_socket_receive(const char *tag, const int sock, char * data, size_t max_len){
    int len =0;
    int result=0;
    while ( result < max_len){
		len = recv(sock, data, max_len - result, 0);
	   // ESP_LOGI(tag, "[sock=%d]: MAX_LEN: %d", sock, max_len);
		if (len < 0) {
			if (errno == EINPROGRESS || errno == EAGAIN || errno == EWOULDBLOCK) {
				return 0;   // Not an error
			}
			if (errno == ENOTCONN) {
				ESP_LOGW(tag, "[sock=%d]: Connection closed", sock);
				return -2;  // Socket has been disconnected
			}
			log_socket_error(tag, sock, errno, "Error occurred during receiving");
			return -1;
		}
		result+=len;
    }
    return result;
}
*/
static void send_time_timer_cb(TimerHandle_t xTimer){
   // ESP_LOGI(TAG, "Send time cb");
    struct timeval now;
	if (sntp_snapcast == NULL) {
		ESP_LOGW(TAG, "snapclient not initialized, ignoring");
		return;
	}
	if (gettimeofday(&now, NULL)) {
		ESP_LOGI(TAG, "Failed to gettimeofday\r\n");
		return;
	}

	sntp_snapcast->last_sync.tv_sec = now.tv_sec;
	sntp_snapcast->last_sync.tv_usec = now.tv_usec;

	base_message_t base_message = {
		SNAPCAST_MESSAGE_TIME,      // type
		sntp_snapcast->id_counter++,   // id
		0x0,                         // refersTo
		{ now.tv_sec - sntp_snapcast->server_uptime.tv_sec, now.tv_usec }, // sent
		{ now.tv_sec - sntp_snapcast->server_uptime.tv_usec, now.tv_usec }, // received
		TIME_MESSAGE_SIZE           // size
	};
	if (base_message_serialize( &base_message, base_message_serialized, BASE_MESSAGE_SIZE)) {
		ESP_LOGE(TAG, "Failed to serialize base message for time\r\n");
		return;
	}
	if (time_message_serialize( &(sntp_snapcast->time_message), time_message_serialized, TIME_MESSAGE_SIZE)) {
		ESP_LOGI(TAG, "Failed to serialize time message\r\b");
		return;
	}
	_snapcast_stream_socket_send(TAG, sntp_snapcast->sock, base_message_serialized, BASE_MESSAGE_SIZE);
	_snapcast_stream_socket_send(TAG, sntp_snapcast->sock, time_message_serialized, TIME_MESSAGE_SIZE);
}

snapcast_stream_ringbuffer_node_t *snapcast_stream_new_ringbuffer(){
	snapcast_stream_ringbuffer_node_t *node = (snapcast_stream_ringbuffer_node_t*) audio_malloc(sizeof(snapcast_stream_ringbuffer_node_t));
	AUDIO_MEM_CHECK(TAG, node, return NULL);
	node->position = 0;
	node->ringbuffer_size = 1;
	node->last  = node;
	node->next  = node;
	return node;
}

snapcast_stream_ringbuffer_node_t *snapcast_stream_ringbuffer_add_element(snapcast_stream_ringbuffer_node_t *last_node, int pos){
	snapcast_stream_ringbuffer_node_t *new_node = (snapcast_stream_ringbuffer_node_t*) audio_malloc(sizeof(snapcast_stream_ringbuffer_node_t));
	AUDIO_MEM_CHECK(TAG, new_node, return NULL);
//	new_node->data        = data;
	new_node->position	  = pos;
	new_node->last        = last_node;
	new_node->next        = last_node->next;
	new_node->next->ringbuffer_size += 1;
	last_node->next       = new_node;
	new_node->next->last  = new_node;
	ESP_LOGW("SNAPCAST_STREAM_LIST","### New Element created ###");
	ESP_LOGW("SNAPCAST_STREAM_LIST","### New Element Position=%d ###", new_node->position);
	ESP_LOGW("SNAPCAST_STREAM_LIST","### New List Size=%d ###", new_node->next->ringbuffer_size);

	return new_node;
}

snapcast_stream_ringbuffer_node_t *snapcast_stream_ringbuffer_get_element(snapcast_stream_ringbuffer_node_t *rb, int pos){
	while(rb->position!=pos){
		rb = rb->next;
	}
	//ESP_LOGI("SNAPCAST_STREAM_LIST","Element Position=%d", rb->position);
	return rb;
}



snapcast_stream_ringbuffer_node_t *snapcast_stream_ringbuffer_delete_element(snapcast_stream_ringbuffer_node_t  *node, int pos){
	snapcast_stream_ringbuffer_node_t *tmp = node;
	snapcast_stream_ringbuffer_node_t *el = snapcast_stream_ringbuffer_get_element(node, pos);
	snapcast_stream_ringbuffer_node_t *first = snapcast_stream_ringbuffer_get_element(tmp, SNAPCAST_STREAM_RINGBUFFER_FIRST);
	snapcast_stream_ringbuffer_node_t *rb = el->next;
	int size = first->ringbuffer_size;
	ESP_LOGI("SNAPCAST_STREAM_LIST","### First Element Position=%d, Size=%d ###", first->position, first->ringbuffer_size);
	ESP_LOGI("SNAPCAST_STREAM_LIST","!!! Delete Element %d !!!! ", el->position);
	first->ringbuffer_size -= 1;
	el->next->last = el->last;
	el->last->next = el->next;
	int y = rb->position;
	for(int x = y;x<size;x++){
		rb->position-=1;
		ESP_LOGI("SNAPCAST_STREAM_LIST", "###  Next Element Position=%d, Size=%d ###", rb->position, rb->ringbuffer_size);
		rb=rb->next;
	}
	ESP_LOGI("SNAPCAST_STREAM_LIST","!!! New List Size  %d !!!! ", first->ringbuffer_size);
	size=first->ringbuffer_size;
	rb = first;
	for(int x=0;x<size;x++){
		ESP_LOGI("SNAPCAST_STREAM_LIST", "###  New List Element Position=%d, Size=%d ###", rb->position, rb->ringbuffer_size);
		rb=rb->next;
	}
	free(el);
	return node->last;
}

/*
static esp_err_t snapcast_stream_rinbuffer_reset(audio_element_handle_t self){
	snapcast_stream_t *tcp = (snapcast_stream_t *)audio_element_getdata(self);
	snapcast_stream_ringbuffer_node_t *node = snapcast_stream_ringbuffer_get_element(tcp->rb, SNAPCAST_STREAM_RINGBUFFER_FIRST);
	tcp->write_rb=node;
	tcp->read_rb=node;
	int size=node->ringbuffer_size;
	for(int x=0;x<size;x++){
		node->timestamp=0;
		node->data_size=0;
		memset(node->data, 0x00, SNAPCAST_STREAM_RINGBUFFER_SIZE);
		node=node->next;
	}
	return ESP_OK;
}*/


void print_all(snapcast_stream_ringbuffer_node_t* rb){
	snapcast_stream_ringbuffer_node_t *el = snapcast_stream_ringbuffer_get_element(rb, SNAPCAST_STREAM_RINGBUFFER_FIRST);
	int size = el->ringbuffer_size;
	for(int x=0;x<size;x++){
		ESP_LOGI("SNAPCAST_STREAM_LIST", "Position=%d",el->position);
		el=el->next;
	}
}

snapcast_stream_ringbuffer_node_t *snapcast_stream_create_ringbuffer(int size){
	snapcast_stream_ringbuffer_node_t *head    = snapcast_stream_new_ringbuffer();
	snapcast_stream_ringbuffer_node_t *current = head;
	for(int i = 1; i < size; i++){
		current=snapcast_stream_ringbuffer_add_element(current, i);
	}
	return head;
}

static esp_err_t _snapcast_dispatch_event(audio_element_handle_t el, snapcast_stream_t *tcp, void *data, int len, snapcast_stream_status_t state)
{
    if (el && tcp && tcp->hook) {
        snapcast_stream_event_msg_t msg = { 0 };
        msg.data = data;
        msg.data_len = len;
        msg.sock_fd = tcp->t;
        msg.source = el;
        return tcp->hook(&msg, state, tcp->ctx);
    }
    return ESP_FAIL;
}


static esp_err_t _snapcast_open(audio_element_handle_t self)
{
    AUDIO_NULL_CHECK(TAG, self, return ESP_FAIL);
    snapcast_stream_t *tcp = (snapcast_stream_t *)audio_element_getdata(self);
    char base_message_serialized[BASE_MESSAGE_SIZE];
    char *hello_message_serialized;
    int result;
    struct timeval now;
    int sockfd;
    struct sockaddr_in servaddr;

    if (tcp->is_open) {
        ESP_LOGE(TAG, "Already opened");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Host is %s, port is %d\n", tcp->host, tcp->port);

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(tcp->host);
    servaddr.sin_port = htons(tcp->port);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (connect(sockfd, (SA*)&servaddr, sizeof(servaddr))!= 0) {
		ESP_LOGE(TAG,"connection with the server failed...\n");
		return ESP_FAIL;
	}
    tcp->sock=sockfd;
    // esp_transport_handle_t t = esp_transport_tcp_init();
   // esp_transport_list_handle_t transport_list = esp_transport_list_init();
   // esp_transport_list_add(transport_list, t, "http");
   // AUDIO_NULL_CHECK(TAG, t, return ESP_FAIL);
    //tcp->sock = esp_transport_connect(t, tcp->host, tcp->port, SNAPCAST_CONNECT_TIMEOUT_MS);
    if (tcp->sock < 0) {
        _get_socket_error_code_reason(__func__,  tcp->sock);
        return ESP_FAIL;
    }
    tcp->is_open = true;
   // tcp->t = t;
    tcp->base_message.type = SNAPCAST_MESSAGE_BASE;  // default state, no current message
    tcp->base_message.sent.sec = 0;
    tcp->base_message.sent.usec = 0;
    tcp->base_message.received.sec = 0;
    tcp->base_message.received.usec = 0;
    tcp->received_header = false;
    tcp->last_sync.tv_sec = 0;
    tcp->last_sync.tv_usec = 0;
    tcp->id_counter = 0;
    tcp->time_message.latency.sec = 0;
    tcp->time_message.latency.usec = 0;

    result = gettimeofday(&now, NULL);
	if (result) {
		ESP_LOGI(TAG, "Failed to gettimeofday\r\n");
		return ESP_FAIL;
	}
	esp_read_mac(base_mac, ESP_MAC_WIFI_STA);
    sprintf(mac_address, "%02X:%02X:%02X:%02X:%02X:%02X", base_mac[0], base_mac[1], base_mac[2], base_mac[3], base_mac[4], base_mac[5]);
    ESP_LOGI("SNAPCAST_TCP", "%02X:%02X:%02X:%02X:%02X:%02X", base_mac[0], base_mac[1], base_mac[2], base_mac[3], base_mac[4], base_mac[5]);

	base_message_t base_message = {
		SNAPCAST_MESSAGE_HELLO,      // type
		0x0,                         // id
		0x0,                         // refersTo
		{ now.tv_sec, now.tv_usec }, // sent
		{ 0x0, 0x0 },                // received
		0x0,                         // size
	};

	hello_message_t hello_message = {
		mac_address,
		SNAPCAST_STREAM_CLIENT_NAME,  // hostname
		"0.0.2",               // client version
		"libsnapcast",         // client name
		"esp32",               // os name
		"xtensa",              // arch
		1,                     // instance
		mac_address,           // id
		2,                     // protocol version
	};
	hello_message_serialized = hello_message_serialize(&hello_message, (size_t *)&(base_message.size));
	if (!hello_message_serialized) {
			ESP_LOGI(TAG, "Failed to serialize hello message\r\b");
			return ESP_FAIL;
	}
	if (result) {
		ESP_LOGI(TAG, "Failed to serialize base message\r\n");
	    return ESP_FAIL;
	}
	result=base_message_serialize(&base_message, base_message_serialized, BASE_MESSAGE_SIZE);
	if (result) {
		ESP_LOGI(TAG, "Failed to serialize base message\r\n");
	    return ESP_FAIL;
	}
	if(_snapcast_stream_socket_send(TAG, sockfd, base_message_serialized, BASE_MESSAGE_SIZE)<0){
       free(hello_message_serialized);
	   return ESP_FAIL;
    }
    if(_snapcast_stream_socket_send(TAG, sockfd, hello_message_serialized, base_message.size)<0){
       free(hello_message_serialized);
	   return ESP_FAIL;
	}
    //_snapcast_dispatch_event(self, tcp, NULL, 0, SNAPCAST_STREAM_STATE_CONNECTED);
    free(hello_message_serialized);
    sntp_snapcast=tcp;
    send_time_tm_handle = xTimerCreate( "snapclient_timer", 5000 / portTICK_RATE_MS, pdTRUE, NULL, send_time_timer_cb);
    xTimerStart(send_time_tm_handle, 0);
    return ESP_OK;
}

static esp_err_t _snapcast_read(audio_element_handle_t self, char *buffer, int len, TickType_t ticks_to_wait, void *context){
	int rlen = 0;
	int result = 0;
	char base_buffer[BASE_MESSAGE_SIZE];
	snapcast_stream_t *tcp = (snapcast_stream_t *)audio_element_getdata(self);

//_start:
	//rlen = esp_transport_read(tcp->t, buffer, BASE_MESSAGE_SIZE, tcp->timeout_ms);
	//rlen = _snapcast_stream_socket_receive(TAG, tcp->sock, base_buffer, BASE_MESSAGE_SIZE);
	receive_data(tcp->sock, base_buffer, BASE_MESSAGE_SIZE);
	result = base_message_deserialize(&(tcp->base_message), base_buffer, BASE_MESSAGE_SIZE);
	if(result < 0){
		ESP_LOGW(TAG, "Failed to read base message: %d\r\n", result);
	}



	//rlen = _snapcast_stream_socket_receive(TAG, tcp->sock, buffer, tcp->base_message.size);
	rlen = receive_data(tcp->sock, buffer, tcp->base_message.size+2);
	if (result) {
		ESP_LOGW(TAG, "Failed to read base message: %d\r\n", result);
		return ESP_FAIL;
	}
	return rlen;
}

/*
static esp_err_t _snapcast_write(audio_element_handle_t self, char *buffer, int len, TickType_t ticks_to_wait, void *context){
	int r_len=0;
	int64_t now_ms=0;
	struct timeval now, tv1;
	char *audio_buffer = NULL;
	//uint16_t base_message_type = *((uint16_t*)context);
	snapcast_stream_t *tcp = (snapcast_stream_t *)audio_element_getdata(self);
	int result = gettimeofday(&now, NULL);
	if (result) {
		ESP_LOGI(TAG, "Failed to gettimeofday\r\n");
		return ESP_FAIL;
	}
	timersub(&now,(&tcp->server_uptime) ,&tv1);
	now_ms = tv1.tv_sec * 1000;
	now_ms +=tv1.tv_usec / 1000;
	//if(tcp->base_message.type == SNAPCAST_MESSAGE_WIRE_CHUNK){

	tcp->write_rb->timestamp = tcp->wire_chunk_message.timestamp.sec *  1000;
	tcp->write_rb->timestamp+= tcp->wire_chunk_message.timestamp.usec / 1000;
	tcp->write_rb->data_size=len;
	memcpy(tcp->write_rb->data, buffer, tcp->write_rb->data_size);
	r_len=tcp->read_rb->data_size;
	audio_buffer = tcp->read_rb->data;
	audio_element_output(self, audio_buffer, r_len);
	return 1;
}
*/


static esp_err_t _snapcast_process(audio_element_handle_t self, char *in_buffer, int in_len){

	struct timeval now, tv1, tv2; //, last_time_sync;
	int result;
	int r_len = 1;
	int tmp=0;
	int volume[] ={0, 0};
	int64_t now_ms=0, diff_ms=0;
	//char *buffer;
	if (gettimeofday(&now, NULL)) {
		ESP_LOGI(TAG, "Failed to gettimeofday\r\n");
	}
	snapcast_stream_t *tcp = (snapcast_stream_t *)audio_element_getdata(self);
	r_len=_snapcast_read(self, in_buffer, BASE_MESSAGE_SIZE, tcp->timeout_ms, NULL);
	if(r_len > 0) {
		if(r_len > 4096){
			//ESP_LOGI(TAG, "Failed Write Size  %d", tcp->wire_chunk_message.size);
			r_len=4096;
		}else if(r_len < 0){
			return ESP_FAIL;
		}
		switch(tcp->base_message.type){
			case SNAPCAST_MESSAGE_CODEC_HEADER:
				ESP_LOGI(TAG, "SNAPCAST_MESSAGE_CODEC_HEADER");
				//tcp->bits.enabled=0;
				break;
			case SNAPCAST_MESSAGE_WIRE_CHUNK:
					result = wire_chunk_message_deserialize(&(tcp->wire_chunk_message), in_buffer+1, tcp->base_message.size);
					if(result < 0){
						ESP_LOGI(TAG, "Failed to read wire chunk message: %d\r\n", result);
					}
					int result = gettimeofday(&now, NULL);
					if (result) {
						ESP_LOGI(TAG, "Failed to gettimeofday\r\n");
						return ESP_FAIL;
					}
					timersub(&now,(&tcp->server_uptime) ,&tv1);
					now_ms = tv1.tv_sec * 1000;
					now_ms +=tv1.tv_usec / 1000;
					tcp->write_rb->timestamp  = tcp->wire_chunk_message.timestamp.sec *  1000;
					tcp->write_rb->timestamp += tcp->wire_chunk_message.timestamp.usec / 1000;
					tcp->write_rb->data_size  = tcp->wire_chunk_message.size;
					memcpy(tcp->write_rb->data, in_buffer+13, tcp->write_rb->data_size);
					diff_ms = now_ms - tcp->read_rb->timestamp;
					if(diff_ms > SNAPCAST_STREAM_LOWER_SYNC && diff_ms < SNAPCAST_STREAM_UPPER_SYNC){
						memcpy(tcp->audio_buffer, tcp->read_rb->data, tcp->read_rb->data_size);
						tmp=tcp->read_rb->data_size;
						audio_element_output(self, tcp->audio_buffer, tmp);
						tcp->read_rb=tcp->read_rb->next;
					}
					else{
						ESP_LOGI(TAG, "sync..........");
						ESP_LOGI(TAG, "time diff =%lld\r\n", diff_ms);
						if(diff_ms < SNAPCAST_STREAM_LOWER_SYNC){
							ESP_LOGI(TAG, "sync down.........\r\n");
							tcp->read_rb=tcp->read_rb->last;
						}else if(diff_ms > SNAPCAST_STREAM_UPPER_SYNC){
							ESP_LOGI(TAG, "sync up........\r\n");
							tcp->read_rb=tcp->read_rb->next->next;
						}
					}
					tcp->write_rb=tcp->write_rb->next;

				//	ESP_LOGI(TAG, "DIFF_MS   =%lld", diff_ms);
				//	ESP_LOGI(TAG, "Data Size =%d\r\n", tcp->wire_chunk_message.size - r_len);

				break;
			case SNAPCAST_MESSAGE_SERVER_SETTINGS:
					result = server_settings_message_deserialize(&(tcp->server_settings_message), in_buffer+5);
					if (result) {
						ESP_LOGI(TAG, "Failed to read server settings: %d\r\n", result);
					}
					ESP_LOGI(TAG, "Buffer length:  %d", tcp->server_settings_message.buffer_ms);
					ESP_LOGI(TAG, "Ringbuffer size:%d", tcp->server_settings_message.buffer_ms*48*4);
					ESP_LOGI(TAG, "Latency:        %d", tcp->server_settings_message.latency);
					ESP_LOGI(TAG, "Mute:           %d", tcp->server_settings_message.muted);
					ESP_LOGI(TAG, "Setting volume: %d", tcp->server_settings_message.volume);
					volume[0]=tcp->server_settings_message.volume;
					volume[1]=tcp->server_settings_message.muted;
				//_snapcast_dispatch_event(self, tcp, (void*)volume, 4, SNAPCAST_STREAM_STATE_SERVER_SETTINGS_MESAGE);
				break;
			case SNAPCAST_MESSAGE_TIME:
					//ESP_LOGI(TAG, "SNAPCAST_MESSAGE_TIME: Buffer Read=%d", tcp->base_message.size);
					result = time_message_deserialize(&(tcp->time_message), in_buffer+2, TIME_MESSAGE_SIZE);
					if (result) {
						ESP_LOGI(TAG, "Failed to deserialize time message\r\n");
						break;
					}
					tv2.tv_sec = tcp->base_message.received.sec;
					tv2.tv_usec= tcp->base_message.received.usec;
					timersub(&now,&tv2,&tcp->server_uptime);
					uint64_t server_uptime_ms = tcp->base_message.received.sec * 1000;
					server_uptime_ms += tcp->server_uptime.tv_usec / 1000;
					//ESP_LOGI(TAG,"Server Up Time[ms]=%lld", server_uptime_ms);
				/*	timersub(&now, &tcp->server_uptime, &tv1);

					time_ms=tv1.tv_sec * 1000;
					time_ms+=tv1.tv_usec/1000;
					ESP_LOGI(TAG_TIME, "Base   Uptime %d.%3d", tcp->base_message.received.sec, tcp->base_message.received.usec/1000);
					ESP_LOGI(TAG_TIME, "Server Uptime %ld.%3ld", tv1.tv_sec, tv1.tv_usec/1000);
					ESP_LOGI(TAG_TIME, "Server Uptime %lldms", time_ms);*/

					//ESP_LOGI(TAG, "Setting volume: %d", tcp->server_settings_message.volume);
				break;
			case SNAPCAST_MESSAGE_STREAM_TAGS:
				ESP_LOGI(TAG, "SNAPCAST_MESSAGE_STREAM_TAGS ");
				break;
			}

		/*if(size>1){

		}*/
	}/* else {
		w_size = r_len;
	}*/
	memset(in_buffer, 0x00, 4096);
	return 1;
}

static esp_err_t _snapcast_close(audio_element_handle_t self)
{
    AUDIO_NULL_CHECK(TAG, self, return ESP_FAIL);

    snapcast_stream_t *tcp = (snapcast_stream_t *)audio_element_getdata(self);
    AUDIO_NULL_CHECK(TAG, tcp, return ESP_FAIL);
    if (!tcp->is_open) {
        ESP_LOGE(TAG, "Already closed");
        return ESP_FAIL;
    }
   /* if (-1 == esp_transport_close(tcp->t)) {
        ESP_LOGE(TAG, "TCP stream close failed");
        return ESP_FAIL;
    }*/
    tcp->is_open = false;
    if (AEL_STATE_PAUSED != audio_element_get_state(self)) {
        audio_element_set_byte_pos(self, 0);
    }
    return ESP_OK;
}

static esp_err_t _snapcast_destroy(audio_element_handle_t self)
{
    AUDIO_NULL_CHECK(TAG, self, return ESP_FAIL);

    snapcast_stream_t *tcp = (snapcast_stream_t *)audio_element_getdata(self);
    AUDIO_NULL_CHECK(TAG, tcp, return ESP_FAIL);
    if (tcp->t) {
        esp_transport_destroy(tcp->t);
        tcp->t = NULL;
    }
    audio_free(tcp);
    return ESP_OK;
}

audio_element_handle_t snapcast_stream_init(snapcast_stream_cfg_t *config)
{
    AUDIO_NULL_CHECK(TAG, config, return NULL);

    audio_element_cfg_t cfg = DEFAULT_AUDIO_ELEMENT_CONFIG();
    audio_element_handle_t el;
    cfg.open = _snapcast_open;
    cfg.close   = _snapcast_close;
    cfg.process = _snapcast_process;
    cfg.destroy = _snapcast_destroy;
    cfg.task_stack = config->task_stack;
    cfg.task_prio = config->task_prio;
    cfg.task_core = config->task_core;
    cfg.stack_in_ext = config->ext_stack;
    cfg.tag = "snapcast_client";
    if (cfg.buffer_len == 0) {
        cfg.buffer_len = SNAPCAST_STREAM_BUF_SIZE;
    }

    snapcast_stream_t *tcp = audio_calloc(1, sizeof(snapcast_stream_t));
    AUDIO_MEM_CHECK(TAG, tcp, return NULL);
    char *audio_buffer = audio_malloc(SNAPCAST_STREAM_RINGBUFFER_SIZE);
    AUDIO_MEM_CHECK(TAG, audio_buffer, return NULL);
    tcp->audio_buffer=audio_buffer;
    tcp->state= config->state;
    tcp->type = config->type;
    tcp->port = config->port;
    tcp->host = config->host;
    tcp->timeout_ms = config->timeout_ms;
    if (config->event_handler) {
        tcp->hook = config->event_handler;
        if (config->event_ctx) {
            tcp->ctx = config->event_ctx;
        }
    }
    tcp->rb=snapcast_stream_create_ringbuffer(SNAPCAST_STREAM_CUSTOM_RINGBUFFER_ELEMENTS);
    tcp->write_rb=snapcast_stream_ringbuffer_get_element(tcp->rb, SNAPCAST_STREAM_RINGBUFFER_FIRST);
    tcp->read_rb= snapcast_stream_ringbuffer_get_element(tcp->rb, SNAPCAST_STREAM_RINGBUFFER_FIRST);
    cfg.read = _snapcast_read;
    cfg.write = NULL;
    el = audio_element_init(&cfg);

    AUDIO_MEM_CHECK(TAG, el, goto _snapcast_init_exit);
    audio_element_setdata(el, tcp);

    return el;
_snapcast_init_exit:
    audio_free(tcp);
    return NULL;
}
