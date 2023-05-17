/*
 * storage_data.c
 *
 *  Created on: 19.04.2023
 *      Author: florian
 */
#include "data_storage.h"
#include <string.h>
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "audio_mem.h"
#define NVS_NAMESPACE "my_namespace"

static const char *TAG = "DATA STORAGE";
// Definiere Schlüssel für die NVS Einträge
#define NVS_KEY_AUDIO_DATA   "audio_data"
#define NVS_KEY_WIFI_DATA    "wifi_data"
#define NVS_KEY_MQTT_DATA    "mqtt_data"



// Funktion zum Schreiben der Daten im NVS
esp_err_t write_nvs_data(const char *key, void *data, unsigned int size)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_blob(nvs_handle, key, data, size);
    if (err != ESP_OK) {
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        nvs_close(nvs_handle);
        return err;
    }

    nvs_close(nvs_handle);
    return ESP_OK;
}

// Funktion zum Lesen der Daten aus dem NVS
esp_err_t read_nvs_data(const char *key, void *data, unsigned int size)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_get_blob(nvs_handle, key, data, &size);
    if (err != ESP_OK) {
        nvs_close(nvs_handle);
        return err;
    }

    nvs_close(nvs_handle);
    return ESP_OK;
}





esp_err_t save_data_to_nvs(audio_data_t *audio_data, wifi_data_t *wifi_data, mqtt_data_t *mqtt_data) {
    // Öffne den NVS-Handle
	nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Fehler beim Öffnen des NVS-Handles: %s\n", esp_err_to_name(err));
        return err;
    }

    // Schreibe die Daten in den NVS-Speicher
    err = nvs_set_blob(nvs_handle, "audio_data", audio_data, sizeof(audio_data_t));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Fehler beim Schreiben der EQ-Daten in den NVS: %s\n", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_blob(nvs_handle, "wifi_data", wifi_data, sizeof(wifi_data_t));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Fehler beim Schreiben der Wifi-Daten in den NVS: %s\n", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_blob(nvs_handle, "mqtt_data", mqtt_data, sizeof(mqtt_data_t));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Fehler beim Schreiben der MQTT-Daten in den NVS: %s\n", esp_err_to_name(err));
        return err;
    }

    // Schließe den NVS-Handle
    nvs_close(nvs_handle);
    return ESP_OK;
}

esp_err_t read_data_from_nvs(audio_data_t *audio_data, wifi_data_t *wifi_data, mqtt_data_t *mqtt_data){
    esp_err_t err;
    nvs_handle_t my_handle;
    unsigned int size = 0;

    // Öffne den Namespace "storage" im NVS
    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        return err;
    }

    // Lese Daten aus dem NVS und speichere sie in den entsprechenden Variablen
    size = sizeof(audio_data_t);
    err = nvs_get_blob(my_handle, "audio_data", audio_data, &size);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        return err;
    }

    size = sizeof(wifi_data_t);
    err = nvs_get_blob(my_handle, "wifi_data", wifi_data, &size);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        return err;
    }

    size = sizeof(mqtt_data_t);
    err = nvs_get_blob(my_handle, "mqtt_data", mqtt_data, &size);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        return err;
    }

    // Schließe den Namespace
    nvs_close(my_handle);

    return ESP_OK;
}

esp_err_t storage_save_wifi_data(wifi_data_t *wifi_data){
	esp_err_t err;
	nvs_handle_t nvs_handle;

	// Öffne den Namespace "storage" im NVS
	err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
	if (err != ESP_OK) {
		return err;
	}
	err = nvs_set_blob(nvs_handle, "wifi_data", wifi_data, sizeof(wifi_data_t));
	if (err != ESP_OK) {
		ESP_LOGW(TAG, "Fehler beim Schreiben der Wifi-Daten in den NVS: %s\n", esp_err_to_name(err));
		return err;
	}
	// Schließe den Namespace
	nvs_close(nvs_handle);
	return ESP_OK;
}

esp_err_t storage_save_mqtt_data(mqtt_data_t *mqtt_data){
	esp_err_t err;
	nvs_handle_t nvs_handle;

	// Öffne den Namespace "storage" im NVS
	err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
	if (err != ESP_OK) {
		return err;
	}
	err = nvs_set_blob(nvs_handle, "mqtt_data", mqtt_data, sizeof(mqtt_data_t));
	if (err != ESP_OK) {
		ESP_LOGW(TAG, "Fehler beim Schreiben der Wifi-Daten in den NVS: %s\n", esp_err_to_name(err));
		return err;
	}
	// Schließe den Namespace
	nvs_close(nvs_handle);
	return ESP_OK;
}


esp_err_t storage_save_audio_data(audio_data_t *audio_data){
	esp_err_t err=ESP_OK;
	nvs_handle_t my_handle;
	unsigned int size = 0, save=0;
	size = sizeof(audio_data_t);
	audio_data_t *audio_tmp = (audio_data_t*)audio_malloc(size);
	// Öffne den Namespace "storage" im NVS
	ESP_LOGI(TAG,"Saving Audio Data ...");
	err = nvs_open("storage", NVS_READWRITE, &my_handle);
	if (err != ESP_OK) {
		ESP_LOGW(TAG, "Error open Storage: %s\n", esp_err_to_name(err));
	}
	// Lese Daten aus dem NVS und speichere sie in den entsprechenden Variablen
	err = nvs_get_blob(my_handle, "audio_data", audio_tmp, &size);
	if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
		ESP_LOGW(TAG, "Error Reading Audio Storage: %s\n", esp_err_to_name(err));
	}
	ESP_LOGI(TAG,"Audio_tmp first=%x",audio_tmp->first);
	for(int x=0;x<10;x++){
		ESP_LOGI(TAG,"Gain %d Audio Tmp=%d, Audio Data=%d",x, audio_tmp->gain[x], audio_data->gain[x]);
		if(audio_tmp->gain[x]!=audio_data->gain[x]){
			save=1;
		}
	}
	if(save==1){
		err = nvs_set_blob(my_handle, "audio_data", audio_data, sizeof(audio_data_t));
		if (err != ESP_OK) {
			ESP_LOGW(TAG, "Error writing audio data in NVS: %s\n", esp_err_to_name(err));
			return err;
		}
	}
	// Schließe den Namespace
	nvs_close(my_handle);
	audio_free(audio_tmp);
	return err;
}

void storage_startup_init(audio_data_t *audio_data, wifi_data_t *wifi_data, mqtt_data_t *mqtt_data){
	 esp_err_t err;
	 ESP_LOGI(TAG, "Data Storage");
	 err=read_data_from_nvs(audio_data, wifi_data, mqtt_data);
	 ESP_LOGI(TAG, "Audio Data First=%x", audio_data->first);
	 ESP_LOGI(TAG, "Wifi Data First=%x",  wifi_data->first);
	 ESP_LOGI(TAG, "MQTT Data First=%x",  mqtt_data->first);
	  if(audio_data->first!=0xDEADBEEF || wifi_data->first!=0xDEADBEEF || mqtt_data->first!=0xDEADBEEF){
		 audio_data->first=0xDEADBEEF;
		 wifi_data->first =0xDEADBEEF;
		 mqtt_data->first =0xDEADBEEF;
		 storage_default_values(audio_data, wifi_data, mqtt_data);
		 save_data_to_nvs(audio_data, wifi_data, mqtt_data);
	 }
}


void storage_default_values(audio_data_t *audio_data, wifi_data_t *wifi_data, mqtt_data_t *mqtt_data){

	 strcpy(wifi_data->ssid, 	"dlink");
	 strcpy(wifi_data->password, "Mega.17.20");
	 strcpy(wifi_data->dns_name, "esp32_snapclient");

	strcpy(mqtt_data->client_name, "ESP32 Snapcast Client");
	strcpy(mqtt_data->server_uri,  "mqtt://192.168.0.221"  );
	strcpy(mqtt_data->cmd_topic,   "esp32_snapcast_client/cmd");
	strcpy(mqtt_data->state_topic, "esp32_snapcast_client/state");

	audio_data->volume=80;
	audio_data->muted=0;
	for(int x=0;x<10;x++){
		audio_data->gain[x]=0;
		audio_data->gain[x+10]=0;
	}
}

