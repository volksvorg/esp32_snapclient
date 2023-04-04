#ifndef _MQTT_H_
#define _MQTT_H_

#define MQTT_CONFIG_DATA_BUFFER_SIZE 33




typedef void (*mqtt__event_handle_cb)(int event, char* data, uint32_t data_size);

typedef enum {
    MQTT_EVENT_CMD,
	MQTT_EVENT_NAME
} mqtt_event_t;

typedef struct mqtt_data{
	char 				  *name;
	int 				  volume;
	int 				  muted;
	int 				  eq_gain[20];
}mqtt_data_t;

typedef struct mqtt_config{
	char 				  name[MQTT_CONFIG_DATA_BUFFER_SIZE];
	char 				  server_uri[MQTT_CONFIG_DATA_BUFFER_SIZE];
	char 				  topic_status[MQTT_CONFIG_DATA_BUFFER_SIZE];
	char 				  topic_cmd[MQTT_CONFIG_DATA_BUFFER_SIZE];
	mqtt__event_handle_cb event_handler;
	char 				  *state_data;
	mqtt_data_t           data;
}mqtt_handle_t;



void mqtt_app_start(mqtt_handle_t *ptr, mqtt__event_handle_cb evh);
char *mqtt_cmd_message_serialize(mqtt_handle_t *mqtt);
int mqtt_cmd_message_deserialize(mqtt_handle_t *mqtt, const char *json_str);

#endif /* _MQTT_H_ */
