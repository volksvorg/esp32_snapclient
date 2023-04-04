#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "webserver.h"
#include "sdkconfig.h"
#include "mqtt.h"
#include "sntp_client.h"
#include "snapcast.h"
#include "wifi_interface.h"

#define EXAMPLE_ESP_WIFI_SSID      "ESP32"
#define EXAMPLE_ESP_WIFI_PASS      "mega1720"
#define EXAMPLE_ESP_WIFI_CHANNEL   11
#define EXAMPLE_MAX_STA_CONN       4

//#define EXAMPLE_ESP_WIFI_SSID      "myssid"
//#define EXAMPLE_ESP_WIFI_PASS      "mypasswd"
#define EXAMPLE_ESP_MAXIMUM_RETRY  5

static const char *TAG = "!!!!!WIFI_INTERFACE!!!!!";
static char start_bit=0;

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;
esp_event_handler_instance_t instance_any_id;
esp_event_handler_instance_t instance_got_ip;

esp_event_handler_t wifi_handle;


/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1





void wifi_init_station(char *ssid, char *passwd)
{
    s_wifi_event_group = xEventGroupCreate();




    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_handle, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_handle, NULL, &instance_got_ip));

    wifi_config_t wifi_config_sta = {
        .sta = {
            .ssid = "ssid",
            .password = "passwd",
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
	     .threshold.authmode = WIFI_AUTH_OPEN,
	     .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
        },
    };
    strcpy((char*)wifi_config_sta.sta.ssid, ssid);
    strcpy((char*)wifi_config_sta.sta.password, passwd);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config_sta) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
  /*  EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

   * xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s", wifi_config_sta.sta.ssid, wifi_config_sta.sta.password);
        start_webserver();
        //snapclient_start(pipeline, raw_stream_reader);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s", wifi_config_sta.sta.ssid, wifi_config_sta.sta.password);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }*/

    /* The event will not be processed after unregister */

    start_bit=1;
}



void wifi_init_softap(void){
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_handle, NULL, NULL));
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .channel = EXAMPLE_ESP_WIFI_CHANNEL,
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d", EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS, EXAMPLE_ESP_WIFI_CHANNEL);
    start_bit=1;
}

void wifi_interface_deinit(void){
	if(start_bit==1){
		ESP_LOGI(TAG, "DEINIT");
		//ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
		//ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
		//vEventGroupDelete(s_wifi_event_group);
		esp_wifi_disconnect();
		esp_wifi_stop();
		//esp_wifi_deinit();
		//vTaskDelay(1000 / portTICK_PERIOD_MS);
		//esp_event_loop_delete_default();
	}
}

void wifi_interface_init(esp_event_handler_t handle){
	wifi_handle = handle;
	ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
	esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));
}

void wifi_interface_softap_start(void)
{
	wifi_interface_deinit();
    wifi_init_softap();
}

void wifi_interface_station_start(char *ssid, char *passwd){

	wifi_interface_deinit();

	wifi_init_station(ssid, passwd);


}
