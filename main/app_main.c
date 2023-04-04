/* Play music to from local server

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "audio_mem.h"
#include "snapcast_stream.h"
#include "mp3_decoder.h"
#include "flac_decoder.h"
#include "FLAC/stream_decoder.h"
#include "i2s_stream.h"

#include "esp_peripherals.h"
#include "periph_wifi.h"
#include "periph_sdcard.h"
#include "periph_button.h"
#include "board.h"
#include "equalizer.h"
#include "mqtt.h"
#include "sntp_client.h"
#include "wifi_interface.h"
#include "mdns.h"
#include "webserver.h"
#include "tools.h"
#include "audio_idf_version.h"

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0))
#include "esp_netif.h"
#else
#include "tcpip_adapter.h"
#endif

#define CONFIG_TCP_URL "192.168.0.221"
#define CONFIG_TCP_PORT 1704

#define EXAMPLE_ESP_MAXIMUM_RETRY  5
static int s_retry_num = 0;

static const char *TAG = "SNAPCAST_CLIENT_MAIN";






audio_pipeline_handle_t pipeline;
audio_element_handle_t snapcast_stream_reader, flac_decoder, i2s_stream_writer, equalizer;
audio_board_handle_t board_handle;

static mqtt_handle_t            mqtt_handle;
static int audio_volume = 100;
void mqtt_event_handler(int event, char* data, uint32_t data_size){
	char *data_buffer;
	switch(event){
	case MQTT_EVENT_CMD:
		data_buffer=audio_malloc(data_size+1);
		strncpy(data_buffer, data, data_size);
		//ESP_LOGI(TAG,"MQTT new data=%s",data_buffer);
		mqtt_cmd_message_deserialize(&mqtt_handle, data_buffer);
		//ESP_LOGI(TAG,"MQTT new volume=%d",mqtt_handle.data.volume);
		//ESP_LOGI(TAG,"MQTT  is  muted=%d",mqtt_handle.data.muted);
	/*	for(int x=0;x<10;x++){
			ESP_LOGI(TAG,"MQTT equalier gain%d=%d", x, mqtt_handle.data.eq_gain[x]);
		}*/
		audio_volume=tools_set_audio_volume(board_handle, mqtt_handle.data.volume, mqtt_handle.data.muted);
		tools_set_eq_gain(equalizer, mqtt_handle.data.eq_gain);
		mqtt_handle.state_data=mqtt_cmd_message_serialize(&mqtt_handle);
		audio_free(data_buffer);
		break;
	case MQTT_EVENT_NAME:
		break;

	}
}

void webserver_event_handler(int event, char* data, uint32_t data_size){
	switch(event){
		case WEBSERVER_EVENT_NEW_SSID:
			ESP_LOGI(TAG,"SSID=%s, Size=%d", data, data_size);
			break;
		case WEBSERVER_EVENT_NEW_PASSWORD:
			ESP_LOGI(TAG,"PASSWD=%s, Size=%d", data, data_size);
			break;
		case WEBSERVER_EVENT_NEW_MQTT_SERVER_IP:
			ESP_LOGI(TAG,"SERVER IP=%s, Size=%d", data, data_size);
			break;
		case WEBSERVER_EVENT_NEW_MQTT_TOPIC_1:
			ESP_LOGI(TAG,"TOPIC1=%s, Size=%d", data, data_size);
			break;
		case WEBSERVER_EVENT_NEW_MQTT_TOPIC_2:
			ESP_LOGI(TAG,"TOPIC2=%s, Size=%d", data, data_size);
			break;
	}
}

esp_err_t snapcast_stream_event_handler(snapcast_stream_event_msg_t *msg, snapcast_stream_status_t state, void *event_ctx){
	int *tmp;
	int volume=0;
	switch(state){
	case SNAPCAST_STREAM_STATE_NONE:
			ESP_LOGI(TAG,"Snapcast NONE");
			break;
	case SNAPCAST_STREAM_STATE_CONNECTED:
			ESP_LOGI(TAG,"Snapcast Connected");
		break;
	case SNAPCAST_STREAM_STATE_CHANGING_SONG_MESSAGE:
			ESP_LOGI(TAG,"Snapcast Changing Song");
	break;
	case SNAPCAST_STREAM_STATE_TCP_SOCKET_TIMEOUT_MESSAGE:
			ESP_LOGI(TAG,"Snapcast Timeout");
			audio_element_stop(flac_decoder);
			audio_element_stop (i2s_stream_writer);
	break;
	case SNAPCAST_STREAM_STATE_SNTP_MESSAGE:
			ESP_LOGI(TAG,"Get Time");
			sntp_get_time();
		break;
	case SNAPCAST_STREAM_STATE_SERVER_SETTINGS_MESAGE:
			ESP_LOGI(TAG,"Snapcast Server Settings");
			tmp=(int*)msg->data;
			tools_set_audio_volume(board_handle, tmp[0], tmp[1]);
			ESP_LOGI(TAG,"Snapcast Volume=%d", volume);
		break;
	case SNAPCAST_STREAM_START_AUDIO_ELEMENT:
		    ESP_LOGI(TAG,"Snapcast Stop");
			//audio_element_run(flac_decoder);
			//audio_element_run(i2s_stream_writer);
			break;
	case SNAPCAST_STREAM_STOP_AUDIO_ELEMENT:
			//audio_element_stop(flac_decoder);
			//audio_element_stop (i2s_stream_writer);
		break;
	}
	return ESP_OK;
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data){
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d", MAC2STR(event->mac), event->aid);

    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d", MAC2STR(event->mac), event->aid);

    }
    else if(event_id == WIFI_EVENT_STA_STOP){
    	ESP_LOGI(TAG, "WIFI_EVENT_STA_STOP");

    }
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
		esp_wifi_connect();
	} else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
		ESP_LOGI(TAG, "!!!!!!! WIFI FAILD !!!!!");
		if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
			esp_wifi_connect();
			s_retry_num++;
			ESP_LOGI(TAG, "retry to connect to the AP");
		} else {
			//xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
		}
		ESP_LOGI(TAG,"connect to the AP fail");
	} else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
		ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
		ESP_LOGW(TAG, "!!!!got ip:!!!!!!" IPSTR, IP2STR(&event->ip_info.ip));
		sntp_client_init();
		sntp_get_time();
		start_webserver(webserver_event_handler);
		mqtt_app_start(&mqtt_handle, mqtt_event_handler);
		ESP_LOGI(TAG, "[ 5 ] Start audio_pipeline");
		audio_hal_set_volume(board_handle->audio_hal, 90);
		audio_pipeline_run(pipeline);
		//audio_element_run(snapcast_stream_reader);
		s_retry_num = 0;
	}
}




void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0))
    ESP_ERROR_CHECK(esp_netif_init());
#else
    tcpip_adapter_init();
#endif

    //initialise_mdns();
    esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set(TAG, ESP_LOG_INFO);
    esp_log_level_set("SNAPCAST_STREAM", ESP_LOG_INFO);
  //  esp_log_level_set("SNAPCAST_STREAM_TIME", ESP_LOG_INFO);
  //  esp_log_level_set("MY_MQTT", ESP_LOG_WARN);

    ESP_LOGI(TAG, "[ 1 ] Start codec chip");
    board_handle = audio_board_init();
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_DECODE, AUDIO_HAL_CTRL_START);

    ESP_LOGI(TAG, "[2.0] Create audio pipeline for playback");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);
    AUDIO_NULL_CHECK(TAG, pipeline, return);

    ESP_LOGI(TAG, "[2.1] Create i2s stream to write data to codec chip");
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    i2s_cfg.i2s_config.sample_rate = 48000;
    i2s_cfg.task_core=1;
    i2s_cfg.i2s_config.dma_buf_len=300;
    i2s_cfg.i2s_config.dma_buf_count=6;

    i2s_stream_writer = i2s_stream_init(&i2s_cfg);
    AUDIO_NULL_CHECK(TAG, i2s_stream_writer, return);

    ESP_LOGI(TAG, "[2.2] Create flac decoder to decode mp3 file");
    flac_decoder_cfg_t flac_cfg = DEFAULT_FLAC_DECODER_CONFIG();
    flac_cfg.out_rb_size =4 * 4096;
    flac_cfg.task_core=1;
    flac_decoder = flac_decoder_init(&flac_cfg);
    //audio_element_set_read_cb(flac_decoder, flac_read_cb, NULL);

    equalizer_cfg_t eq_cfg = DEFAULT_EQUALIZER_CONFIG();
    int set_gain[] = { -13, -13, -13, -13, -13, -13, -13, -13, -13, -13, -13, -13, -13, -13, -13, -13, -13, -13, -13, -13};
    eq_cfg.set_gain = set_gain; // The size of gain array should be the multiplication of NUMBER_BAND and number channels of audio stream data. The minimum of gain is -13 dB.
    eq_cfg.task_core =1;
    equalizer = equalizer_init(&eq_cfg);

    ESP_LOGI(TAG, "[2.2] Create tcp client stream to read data");
    snapcast_stream_cfg_t snapcast_cfg = SNAPCAST_STREAM_CFG_DEFAULT();
    snapcast_cfg.type = AUDIO_STREAM_READER;
    snapcast_cfg.port = CONFIG_TCP_PORT;
    snapcast_cfg.host = CONFIG_TCP_URL;
    snapcast_cfg.state= SNAPCAST_STREAM_STATE_NONE;
    snapcast_cfg.event_handler=snapcast_stream_event_handler;
    snapcast_stream_reader = snapcast_stream_init(&snapcast_cfg);
    AUDIO_NULL_CHECK(TAG, snapcast_stream_reader, return);


    ESP_LOGI(TAG, "[2.3] Register all elements to audio pipeline");
    audio_pipeline_register(pipeline, snapcast_stream_reader, "snapcast");
    audio_pipeline_register(pipeline, flac_decoder, "flac_decoder");
    audio_pipeline_register(pipeline, equalizer, "equalizer");
    audio_pipeline_register(pipeline, i2s_stream_writer, "i2s");

    ESP_LOGI(TAG, "[2.4] Link it together snapcast-->flac_decoder-->equalizer-->i2s");
    audio_pipeline_link(pipeline, (const char *[]) {"snapcast", "flac_decoder","equalizer", "i2s"}, 4);

    ESP_LOGI(TAG, "[ 3 ] Start and wait for Wi-Fi network");
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);
    audio_board_key_init(set);


    wifi_interface_init(wifi_event_handler);
    wifi_interface_station_start(CONFIG_WIFI_SSID, CONFIG_WIFI_PASSWORD);
  /*  periph_wifi_cfg_t wifi_cfg = {
        .ssid = CONFIG_WIFI_SSID,
        .password = CONFIG_WIFI_PASSWORD,
    };
    esp_periph_handle_t wifi_handle = periph_wifi_init(&wifi_cfg);
    esp_periph_start(set, wifi_handle);
    periph_wifi_wait_for_connected(wifi_handle, portMAX_DELAY);*/
	ESP_LOGI(TAG, "[ 4 ] Set up  event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);

    ESP_LOGI(TAG, "[4.1] Listening event from all elements of pipeline");
    audio_pipeline_set_listener(pipeline, evt);

    ESP_LOGI(TAG, "[4.2] Listening event from peripherals");
    audio_event_iface_set_listener(esp_periph_set_get_event_iface(set), evt);
    char source[20];
  //  audio_element_run(snapcast_stream_reader);
    while (1) {
        audio_event_iface_msg_t msg;
        esp_err_t ret = audio_event_iface_listen(evt, &msg, portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[ * ] Event interface error : %d", ret);
            continue;
        }
        if (msg.source == (void *) snapcast_stream_reader){
        	sprintf(source, "%s", "snapcast");
        }else if(msg.source == (void *) i2s_stream_writer){
        	sprintf(source, "%s", "i2s");
        }else if(msg.source == (void *)flac_decoder){
            sprintf(source, "%s", "flac");
        }else {
        	sprintf(source, "%s", "unknown");
        }


        if ( msg.source_type == PERIPH_ID_BUTTON  && msg.cmd == PERIPH_BUTTON_PRESSED  ){
            if ((int) msg.data == get_input_play_id()) {
                ESP_LOGI(TAG, "[ * ] [Play] touch tap event");
            } else if ((int) msg.data == get_input_set_id()) {
                ESP_LOGI(TAG, "[ * ] [Set] touch tap event");
            } else if ((int) msg.data == get_input_mode_id()) {
                ESP_LOGI(TAG, "[ * ] [mode] tap event");
            } else if ((int) msg.data == get_input_volup_id()) {
				ESP_LOGI(TAG, "[ * ] [Vol+] touch tap event");
				int volume = 0;
				audio_hal_get_volume(board_handle->audio_hal, &volume);
				volume+=1;
				tools_set_audio_volume(board_handle, volume, 1);
            } else if ((int) msg.data == get_input_voldown_id()) {
				ESP_LOGI(TAG, "[ * ] [Vol-] touch tap event");
				int volume = 0;
				audio_hal_get_volume(board_handle->audio_hal, &volume);
				volume-=1;
				tools_set_audio_volume(board_handle, volume, 1);
            }
        }


        ESP_LOGI(TAG, "[ X ] Event message %d:%d from %s", msg.source_type, msg.cmd, source);
        if (msg.source == (void *) flac_decoder && msg.cmd == AEL_STATUS_STATE_PAUSED) {
        	audio_element_resume(flac_decoder, 0.5, portMAX_DELAY);
        }
        if (msg.cmd == AEL_MSG_CMD_REPORT_STATUS)
			switch ( (int) msg.data ) {
				case AEL_STATUS_NONE:
					ESP_LOGI(TAG, "[ X ]   status AEL_STATUS_NONE");
					break;
				case AEL_STATUS_ERROR_OPEN:
					ESP_LOGI(TAG, "[ X ]   status AEL_STATUS_ERROR_OPEN");
					break;
				case AEL_STATUS_ERROR_INPUT:
					ESP_LOGI(TAG, "[ X ]   status AEL_STATUS_ERROR_INPUT");
					break;
				case AEL_STATUS_ERROR_PROCESS:
					ESP_LOGI(TAG, "[ X ]   status AEL_STATUS_ERROR_PROCESS");
					break;
				case AEL_STATUS_ERROR_OUTPUT:
					ESP_LOGI(TAG, "[ X ]   status AEL_STATUS_ERROR_OUTPUT");
					break;
				case AEL_STATUS_ERROR_CLOSE:
					ESP_LOGI(TAG, "[ X ]   status AEL_STATUS_ERROR_CLOSE");
					break;
				case AEL_STATUS_ERROR_TIMEOUT:
					ESP_LOGI(TAG, "[ X ]   status AEL_STATUS_ERROR_TIMEOUT");
					break;
				case AEL_STATUS_ERROR_UNKNOWN:
					ESP_LOGI(TAG, "[ X ]   status AEL_STATUS_ERROR_UNKNOWN");
					break;
				case AEL_STATUS_INPUT_DONE:
					ESP_LOGI(TAG, "[ X ]   status AEL_STATUS_INPUT_DONE");
					break;
				case AEL_STATUS_INPUT_BUFFERING:
					ESP_LOGI(TAG, "[ X ]   status AEL_STATUS_INPUT_BUFFERING");
					break;
				case AEL_STATUS_OUTPUT_DONE:
					ESP_LOGI(TAG, "[ X ]   status AEL_STATUS_OUTPUT_DONE");
					break;
				case AEL_STATUS_OUTPUT_BUFFERING:
					ESP_LOGI(TAG, "[ X ]   status AEL_STATUS_OUTPUT_BUFFERING");
					break;
				case AEL_STATUS_STATE_RUNNING:
					ESP_LOGI(TAG, "[ X ]   status AEL_STATUS_STATE_RUNNING");
					break;
				case AEL_STATUS_STATE_PAUSED:
					ESP_LOGI(TAG, "[ X ]   status AEL_STATUS_STATE_PAUSED");
					break;
				case AEL_STATUS_STATE_STOPPED:
					ESP_LOGI(TAG, "[ X ]   status AEL_STATUS_STATE_STOPPED");
					break;
				case AEL_STATUS_STATE_FINISHED:
					ESP_LOGI(TAG, "[ X ]   status AEL_STATUS_STATE_FINISHED");
					break;
				case AEL_STATUS_MOUNTED:
					ESP_LOGI(TAG, "[ X ]   status AEL_STATUS_MOUNTED");
					break;
				case AEL_STATUS_UNMOUNTED:
					ESP_LOGI(TAG, "[ X ]   status AEL_STATUS_UNMOUNTED");
					break;
			}

        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *) flac_decoder
            && msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {
            audio_element_info_t music_info = {0};
            audio_element_getinfo(flac_decoder, &music_info);

            ESP_LOGI(TAG, "[ * ] Receive music info from flac decoder, sample_rates=%d, bits=%d, ch=%d",
                     music_info.sample_rates, music_info.bits, music_info.channels);

            audio_element_setinfo(i2s_stream_writer, &music_info);
            i2s_stream_set_clk(i2s_stream_writer, music_info.sample_rates, music_info.bits, music_info.channels);
            continue;
        }

        /* Stop when the last pipeline element (i2s_stream_writer in this case) receives stop event */
        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *) i2s_stream_writer && msg.cmd == AEL_MSG_CMD_REPORT_STATUS && (((int)msg.data == AEL_STATUS_STATE_STOPPED) || ((int)msg.data == AEL_STATUS_STATE_FINISHED))) {
            ESP_LOGW(TAG, "[ * ] Stop event received");

        }
    }
}
