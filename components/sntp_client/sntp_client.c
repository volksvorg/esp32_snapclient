/* LwIP SNTP example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "sntp_client.h"

#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_sleep.h"


#include "esp_sntp.h"
static const char *TAG = "SNTP";
#define MY_TZ "CET-1CEST,M3.5.0/02,M10.5.0/03"





void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Notification of a time synchronization event");
}

void sntp_get_time(void)
{
	// xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, false, true, portMAX_DELAY);

    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    ESP_LOGI(TAG, "Getting time over NTP.");
    sntp_obtain_time();
    time(&now);
    char strftime_buf[64];
    // Set timezone to Eastern Standard Time and print local time
    setenv("TZ", MY_TZ, 1);
    tzset();
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGW(TAG, "The current date/time in Berlin is: %s", strftime_buf);

}

void sntp_obtain_time(void){

    // wait for time to be set
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 10;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
    time(&now);
    localtime_r(&now, &timeinfo);
}

void sntp_client_init(){
	ESP_LOGI(TAG, "Initializing SNTP");
	sntp_setoperatingmode(SNTP_OPMODE_POLL);
	sntp_setservername(0, "europe.pool.ntp.org");
	sntp_set_time_sync_notification_cb(time_sync_notification_cb);
	sntp_init();
}
