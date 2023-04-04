#ifndef _WEBSERVER_H_
#define _WEBSERVER_H_

#include "audio_element.h"
#include "board.h"

typedef enum {
    WEBSERVER_EVENT_NEW_SSID,
	WEBSERVER_EVENT_NEW_PASSWORD,
	WEBSERVER_EVENT_NEW_MQTT_SERVER_IP,
	WEBSERVER_EVENT_NEW_MQTT_TOPIC_1,
	WEBSERVER_EVENT_NEW_MQTT_TOPIC_2
} webserver_event_t;



typedef void (*webserver__event_handle_cb)(int event, char* data, uint32_t data_size);

void start_webserver(webserver__event_handle_cb event_handler);
void stop_webserver();
void webserver_init(audio_element_handle_t eq, audio_board_handle_t bh);
#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif




#endif /* _WIFI_INTERFACE_H_ */
