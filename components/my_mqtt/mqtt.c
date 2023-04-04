
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/timers.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "mqtt.h"
#include <cJSON.h>
static const char *TAG = "MY_MQTT";

static mqtt_handle_t            *mqtt_handle;
static TimerHandle_t            publish_state_tm_handle;
static esp_mqtt_client_handle_t client;

char *mqtt_cmd_message_serialize(mqtt_handle_t *mqtt){
	char *string = NULL;
	cJSON *name = NULL;
	cJSON *volume = NULL;
	cJSON *muted = NULL;
	cJSON *freq1 = NULL;
	cJSON *freq2 = NULL;
	cJSON *freq3 = NULL;
	cJSON *freq4 = NULL;
	cJSON *freq5 = NULL;
	cJSON *freq6 = NULL;
	cJSON *freq7 = NULL;
	cJSON *freq8 = NULL;
	cJSON *freq9 = NULL;
	cJSON *freq10 = NULL;
	cJSON *json = cJSON_CreateObject();
	if (json == NULL)
	{
		goto end;
	}

	name = cJSON_CreateString(mqtt->name);
	if (name == NULL)
	{
		goto end;
	}
	/* after creation was successful, immediately add it to the monitor,
	 * thereby transferring ownership of the pointer to it */
	cJSON_AddItemToObject(json, "name", name);

	volume = cJSON_CreateNumber(mqtt->data.volume);
	if(volume == NULL){
		goto end;
	}
	cJSON_AddItemToObject(json, "volume", volume);

	muted = cJSON_CreateNumber(mqtt->data.muted);
	if (muted == NULL){
		goto end;
	}
	cJSON_AddItemToObject(json, "muted", muted);

	freq1 = cJSON_CreateNumber(mqtt->data.eq_gain[0]);
	if (freq1 == NULL){
		goto end;
	}
	cJSON_AddItemToObject(json, "31,25Hz", freq1);


	freq2 = cJSON_CreateNumber(mqtt->data.eq_gain[1]);
	if (freq2 == NULL){
		goto end;
	}
	cJSON_AddItemToObject(json, "62,5Hz", freq2);

	freq3 = cJSON_CreateNumber(mqtt->data.eq_gain[2]);
	if (freq3 == NULL){
		goto end;
	}
	cJSON_AddItemToObject(json, "125Hz", freq3);

	freq4 = cJSON_CreateNumber(mqtt->data.eq_gain[3]);
	if (freq4 == NULL){
		goto end;
	}
	cJSON_AddItemToObject(json, "250Hz", freq4);

	freq5 = cJSON_CreateNumber(mqtt->data.eq_gain[4]);
	if (freq5 == NULL){
		goto end;
	}
	cJSON_AddItemToObject(json, "500Hz", freq5);

	freq6 = cJSON_CreateNumber(mqtt->data.eq_gain[5]);
	if (freq6 == NULL){
		goto end;
	}
	cJSON_AddItemToObject(json, "1KHz", freq6);

	freq7 = cJSON_CreateNumber(mqtt->data.eq_gain[6]);
	if (freq7 == NULL){
	goto end;
	}
	cJSON_AddItemToObject(json, "2KHz", freq7);

	freq8 = cJSON_CreateNumber(mqtt->data.eq_gain[7]);
	if (freq8 == NULL){
	goto end;
	}
	cJSON_AddItemToObject(json, "4KHz", freq8);

	freq9 = cJSON_CreateNumber(mqtt->data.eq_gain[8]);
	if (freq9 == NULL){
		goto end;
	}
	cJSON_AddItemToObject(json, "8KHz", freq9);

	freq10 = cJSON_CreateNumber(mqtt->data.eq_gain[9]);
	if (freq10 == NULL){
		goto end;
	}
	cJSON_AddItemToObject(json, "16KHz", freq10);

	string = cJSON_Print(json);
	if (string == NULL)
	{
		fprintf(stderr, "Failed to print json .\n");
	}

end:
	cJSON_Delete(json);
	return string;
}


int mqtt_cmd_message_deserialize(mqtt_handle_t *mqtt, const char *json_str){
	int status = 1;
	cJSON *value = NULL;
	cJSON *json = cJSON_Parse(json_str);
	if (!json){
		const char *error_ptr = cJSON_GetErrorPtr();
		if (error_ptr) {
			// TODO change to a macro that can be disabled
			fprintf(stderr, "Error before: %s\n", error_ptr);
			goto end;
		}
	}

	if (mqtt == NULL) {
		status = 2;
		goto end;
	}
	value = cJSON_GetObjectItemCaseSensitive(json, "name");
	if (cJSON_IsString(value)) {
		memset(mqtt->name, 0x00, MQTT_CONFIG_DATA_BUFFER_SIZE);
		strcpy(mqtt->name, value->string);
	}
	value = cJSON_GetObjectItemCaseSensitive(json, "volume");
	if (cJSON_IsNumber(value)) {
		mqtt->data.volume = value->valueint;
	}
	value = cJSON_GetObjectItemCaseSensitive(json, "muted");
	if (cJSON_IsNumber(value)) {
		mqtt->data.muted = value->valueint;
	}
	value = cJSON_GetObjectItemCaseSensitive(json, "31,25Hz");
	if (cJSON_IsNumber(value)) {
		mqtt->data.eq_gain[0] = value->valueint;
		mqtt->data.eq_gain[10] = value->valueint;
	}
	value = cJSON_GetObjectItemCaseSensitive(json, "62,5Hz");
	if (cJSON_IsNumber(value)) {
		mqtt->data.eq_gain[1] = value->valueint;
		mqtt->data.eq_gain[11] = value->valueint;
	}
	value = cJSON_GetObjectItemCaseSensitive(json, "125Hz");
	if (cJSON_IsNumber(value)) {
		mqtt->data.eq_gain[2] = value->valueint;
		mqtt->data.eq_gain[12] = value->valueint;
	}
	value = cJSON_GetObjectItemCaseSensitive(json, "250Hz");
	if (cJSON_IsNumber(value)) {
		mqtt->data.eq_gain[3] = value->valueint;
		mqtt->data.eq_gain[13] = value->valueint;
	}
	value = cJSON_GetObjectItemCaseSensitive(json, "500Hz");
	if (cJSON_IsNumber(value)) {
		mqtt->data.eq_gain[4] = value->valueint;
		mqtt->data.eq_gain[14] = value->valueint;
	}
	value = cJSON_GetObjectItemCaseSensitive(json, "1KHz");
	if (cJSON_IsNumber(value)) {
		mqtt->data.eq_gain[5] = value->valueint;
		mqtt->data.eq_gain[15] = value->valueint;
	}
	value = cJSON_GetObjectItemCaseSensitive(json, "2KHz");
	if (cJSON_IsNumber(value)) {
		mqtt->data.eq_gain[6] = value->valueint;
		mqtt->data.eq_gain[16] = value->valueint;
	}
	value = cJSON_GetObjectItemCaseSensitive(json, "4KHz");
	if (cJSON_IsNumber(value)) {
		mqtt->data.eq_gain[7] = value->valueint;
		mqtt->data.eq_gain[17] = value->valueint;
	}
	value = cJSON_GetObjectItemCaseSensitive(json, "8KHz");
	if (cJSON_IsNumber(value)) {
		mqtt->data.eq_gain[8] = value->valueint;
		mqtt->data.eq_gain[18] = value->valueint;
	}
	value = cJSON_GetObjectItemCaseSensitive(json, "16KHz");
	if (cJSON_IsNumber(value)) {
		mqtt->data.eq_gain[9] = value->valueint;
		mqtt->data.eq_gain[19] = value->valueint;
	}
	status = 0;
	end:
	cJSON_Delete(json);
		return status;
	}

static void publish_state_timer_cb(TimerHandle_t xTimer){

	esp_mqtt_client_publish(client, mqtt_handle->topic_status, mqtt_handle->state_data, 0, 0, 0);
}

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void _mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
      //  msg_id = esp_mqtt_client_subscribe(client, mqtt_handle->topic_status, 1);
      //  ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
        msg_id = esp_mqtt_client_subscribe(client, mqtt_handle->topic_cmd, 1);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        msg_id = esp_mqtt_client_unsubscribe(client, mqtt_handle->topic_status);
        msg_id = esp_mqtt_client_unsubscribe(client, mqtt_handle->topic_cmd);
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        msg_id = esp_mqtt_client_publish(client, mqtt_handle->topic_status, mqtt_handle->state_data, 0, 0, 0);
        msg_id = esp_mqtt_client_publish(client, mqtt_handle->topic_cmd, mqtt_handle->state_data, 0, 0, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        publish_state_tm_handle = xTimerCreate( "snapclient_timer", 5000 / portTICK_RATE_MS, pdTRUE, NULL, publish_state_timer_cb);
        xTimerStart(publish_state_tm_handle, 0);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
       // ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        mqtt_handle->event_handler(MQTT_EVENT_CMD, event->data, event->data_len);
        if(!strcmp(event->topic, mqtt_handle->topic_cmd)){

        }
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

void mqtt_app_start(mqtt_handle_t *ptr, mqtt__event_handle_cb event_handle){

	mqtt_handle=ptr;
	strcpy(mqtt_handle->name,         "ESP32 Snapcast Client");
	strcpy(mqtt_handle->server_uri,   "mqtt://192.168.0.221"  );
	strcpy(mqtt_handle->topic_cmd,    "esp32_snapcast_client/cmd");
	strcpy(mqtt_handle->topic_status, "esp32_snapcast_client/state");
	mqtt_handle->event_handler=event_handle;
	mqtt_handle->data.volume=100;
	mqtt_handle->data.muted =1;
	for(int x=0;x<10;x++){
		mqtt_handle->data.eq_gain[x]=0;
		mqtt_handle->data.eq_gain[x+10]=0;
	}
	mqtt_handle->state_data=mqtt_cmd_message_serialize(mqtt_handle);

	esp_mqtt_client_config_t mqtt_cfg = {
        .uri =mqtt_handle->server_uri,
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, _mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);

}

void mqtt_app_stop(void){

}
