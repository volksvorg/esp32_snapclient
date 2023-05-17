/*
 * storage_data.h
 *
 *  Created on: 19.04.2023
 *      Author: florian
 */

#ifndef MAIN_DATA_STORAGE_H_
#define MAIN_DATA_STORAGE_H_

#include"esp_err.h"
#include"tools.h"
#include "mqtt.h"


void storage_default_values(audio_data_t *audio_data, wifi_data_t *wifi_data, mqtt_data_t *mqtt_data);
void storage_startup_init(audio_data_t *audio_data, wifi_data_t *wifi_data, mqtt_data_t *mqtt_data);
esp_err_t save_data_to_nvs(audio_data_t *audio_data, wifi_data_t *wifi_data, mqtt_data_t *mqtt_data);
esp_err_t read_data_from_nvs(audio_data_t *audio_data, wifi_data_t *wifi_data, mqtt_data_t *mqtt_data);
esp_err_t storage_save_audio_data(audio_data_t *audio_data);
esp_err_t storage_save_wifi_data (wifi_data_t  *wifi_data);
esp_err_t storage_save_mqtt_data (mqtt_data_t  *mqtt_data);
#endif /* MAIN_DATA_STORAGE_H_ */
