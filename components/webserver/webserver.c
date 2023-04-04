// ESP32 Server with static HTML page

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include <esp_http_server.h>
#include "wifi_interface.h"
#include "webserver.h"
#include "tools.h"




static const char *TAG = "!!!WEBSERVER!!!";
httpd_handle_t server=NULL;
httpd_config_t config;
webserver__event_handle_cb _event_handler;



const char root[] =     "<!DOCTYPE html>\
						<html>\
						<style>\
						html {\
						font-family: Arial;\
						display: inline-block;\
						margin: 0px auto;\
						text-align: center;\
						}\
						h1 {\
						color: #070812;\
						padding: 2vh;\
						}\
						.button {\
						display: inline-block;\
						background-color: #364cf4; //red color\
						border: none;\
						border-radius: 4px;\
						color: white;\
						padding: 16px 40px;\
						text-decoration: none;\
						font-size: 30px;\
						margin: 2px;\
						cursor: pointer;\
						}\
						.button2 {\
						background-color: #364cf4; //blue color\
						}\
						.content {\
						padding: 50px;\
						}\
						.card-grid {\
						max-width: 800px;\
						margin: 0 auto;\
						display: grid;\
						grid-gap: 2rem;\
						grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));\
						}\
						.card {\
						background-color: white;\
						box-shadow: 2px 2px 12px 1px rgba(140, 140, 140, .5);\
						}\
						.card-title {\
						font-size: 1.2rem;\
						font-weight: bold;\
						color: #034078\
						}\
						</style>\
						<head>\
						<title>ESP32 Snapclient</title>\
						<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\
						</head>\
						<body>\
						<h2>Menu</h2>\
						<div class=\"content\">\
						<div class=\"card-grid\">\
						<div class=\"card\">\
						<p><a href=\"/wifi\"><button class=\"button\">WLAN</button></a></p>\
						<p><a href=\"/mqtt\"><button class=\"button\">MQTT</button></a></p>\
						</div>\
						</div>\
						</div>\
						</body>\
						</html>";

const char wifi[] =     "<!DOCTYPE html>\
						<html>\
						<style>\
						html {\
						font-family: Arial;\
						display: inline-block;\
						margin: 0px auto;\
						text-align: center;\
						}\
						h1 {\
						color: #070812;\
						padding: 2vh;\
						}\
						.button {\
						display: inline-block;\
						background-color: #364cf4; //red color\
						border: none;\
						border-radius: 4px;\
						color: white;\
						padding: 16px 40px;\
						text-decoration: none;\
						font-size: 30px;\
						margin: 2px;\
						cursor: pointer;\
						}\
						.button2 {\
						background-color: #364cf4; //blue color\
						}\
						.content {\
						padding: 50px;\
						}\
						.card-grid {\
						max-width: 800px;\
						margin: 0 auto;\
						display: grid;\
						grid-gap: 2rem;\
						grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));\
						}\
						.card {\
						background-color: white;\
						box-shadow: 2px 2px 12px 1px rgba(140, 140, 140, .5);\
						}\
						.card-title {\
						font-size: 1.2rem;\
						font-weight: bold;\
						color: #034078\
						}\
						</style>\
						<head>\
						<title>ESP32 Snapclient</title>\
						<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\
						</head>\
						<body>\
						<h2>WLAN</h2>\
						<div class=\"content\">\
						<div class=\"card-grid\">\
						<div class=\"card\">\
						<br>\
						<form action=\"/post\" method=\"post\">\
						___SSID : <input type=\"text\" name=\"ssid\"><br><br>\
						PASSWD: <input type=\"text\" name=\"passwd\"><br><br>\
						<input type=\"submit\"><br><br>\
						</form>\
						</div>\
						</div>\
						</div>\
						</body>\
						</html>";

const char mqtt[] =     "<!DOCTYPE html>\
						<html>\
						<style>\
						html {\
						font-family: Arial;\
						display: inline-block;\
						margin: 0px auto;\
						text-align: center;\
						}\
						h1 {\
						color: #070812;\
						padding: 2vh;\
						}\
						.button {\
						display: inline-block;\
						background-color: #364cf4; //red color\
						border: none;\
						border-radius: 4px;\
						color: white;\
						padding: 16px 40px;\
						text-decoration: none;\
						font-size: 30px;\
						margin: 2px;\
						cursor: pointer;\
						}\
						.button2 {\
						background-color: #364cf4; //blue color\
						}\
						.content {\
						padding: 50px;\
						}\
						.card-grid {\
						max-width: 800px;\
						margin: 0 auto;\
						display: grid;\
						grid-gap: 2rem;\
						grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));\
						}\
						.card {\
						background-color: white;\
						box-shadow: 2px 2px 12px 1px rgba(140, 140, 140, .5);\
						}\
						.card-title {\
						font-size: 1.2rem;\
						font-weight: bold;\
						color: #034078\
						}\
						</style>\
						<head>\
						<title>ESP32 Snapclient</title>\
						<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\
						</head>\
						<body>\
						<h2>MQTT</h2>\
						<div class=\"content\">\
						<div class=\"card-grid\">\
						<div class=\"card\">\
						<br>\
						<form action=\"/post\" method=\"post\">\
						Server : <input type=\"text\" name=\"server\"><br><br>\
						Topic 1: <input type=\"text\" name=\"topic1\"><br><br>\
						Topic 2: <input type=\"text\" name=\"topic2\"><br><br>\
						<input type=\"submit\"><br><br>\
						</form>\
						</div>\
						</div>\
						</div>\
						</body>\
						</html>";

/*
const char amp[]=       "<!DOCTYPE html>\
		<html>\
		<head>\
		<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\
		<style>\
		html {\
		        font-family: Arial;\
		        display: inline-block;\
		        margin: 0px auto;\
		        text-align: center;\
		    }\
		  h1 {\
		    color: #070812;\
		    padding: 2vh;\
		  }\
		  .button {\
		    display: inline-block;\
		    background-color: #364cf4; //red color\
		    border: none;\
		    border-radius: 4px;\
		    color: white;\
		    padding: 16px 40px;\
		    text-decoration: none;\
		    font-size: 30px;\
		    margin: 2px;\
		    cursor: pointer;\
		  }\
		  .button2 {\
		    background-color: #364cf4; //blue color\
		  }\
		  .content {\
		    padding: 50px;\
		  }\
		  .card-grid {\
		    max-width: 800px;\
		    margin: 0 auto;\
		    display: grid;\
		    grid-gap: 2rem;\
		    grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));\
		  }\
		  .card {\
		    background-color: white;\
		    box-shadow: 2px 2px 12px 1px rgba(140, 140, 140, .5);\
		  }\
		  .card-title {\
		    font-size: 1.2rem;\
		    font-weight: bold;\
		    color: #034078\
		  }\
		.slidecontainer {\
		  width: 75%;\
		  margin-left: auto !important;\
		 margin-right: auto !important;\
		}\
		input[type=\"range\"] {\
		}\
		.slider {\
		  -webkit-appearance: none;\
		  align: center;\
		  width: 100%;\
		  height: 15px;\
		  border-radius: 5px;\
		  background: #d3d3d3;\
		  outline: none;\
		  opacity: 0.7;\
		  -webkit-transition: .2s;\
		  transition: opacity .2s;\
		}\
		.slider:hover {\
		  opacity: 1;\
		}\
		.slider::-webkit-slider-thumb {\
		  -webkit-appearance: none;\
		  appearance: none;\
		  width: 25px;\
		  height: 25px;\
		  border-radius: 50%;\
		  background: #048acf;\
		  cursor: pointer;\
		}\
		.slider::-moz-range-thumb {\
		  width: 25px;\
		  height: 25px;\
		  border-radius: 50%;\
		  background: #048acf;;\
		  cursor: pointer;\
		}\
		</style>\
		</head>\
		<body>\
		<h1>10 Band Equalizer</h1>\
		 	<div class=\"content\">\
		        <div class=\"card-grid\">\
		            <div class=\"card\">\
		            	<form action=\"/post\" method=\"post\">\
		                  <br><br>\
		                  <input type=\"range\" min=\"-13\" max=\"13\" value=\"0\"  orient=\"vertical\" name=\"freq1\"/>\
		                  <input type=\"range\" min=\"-13\" max=\"13\" value=\"0\" orient=\"vertical\" name=\"freq2\"/>\
		                  <input type=\"range\" min=\"-13\" max=\"13\" value=\"0\" orient=\"vertical\" name=\"freq3\"/>\
		                  <input type=\"range\" min=\"-13\" max=\"13\" value=\"0\" orient=\"vertical\" name=\"freq4\"/>\
		                  <input type=\"range\" min=\"-13\" max=\"13\" value=\"0\" orient=\"vertical\" name=\"freq5\"/>\
		                  <input type=\"range\" min=\"-13\" max=\"13\" value=\"0\" orient=\"vertical\" name=\"freq6\"/>\
		                  <input type=\"range\" min=\"-13\" max=\"13\" value=\"0\" orient=\"vertical\" name=\"freq7\"/>\
		                  <input type=\"range\" min=\"-13\" max=\"13\" value=\"0\" orient=\"vertical\" name=\"freq8\"/>\
		                  <input type=\"range\" min=\"-13\" max=\"13\" value=\"0\" orient=\"vertical\" name=\"freq9\"/>\
		                  <input type=\"range\" min=\"-13\" max=\"13\" value=\"0\" orient=\"vertical\" name=\"freq10\"/>\
		                 <br>Volume\
		                  <br>\
		                  <input type=\"range\" min=\"60\" max=\"100\" value=\"60\"  name=\"volume\"/>\
		                  <br>\
		                  <input type=\"submit\"><br>\
		                  <br><br>\
		            	</form>\
					</div>\
				</div>\
			</div>\
		</body>\
		</html>\
		";
*/


esp_err_t root_handler(httpd_req_t *req){

    httpd_resp_send(req, root, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t wifi_handler(httpd_req_t *req){

    httpd_resp_send(req, wifi, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t mqtt_handler(httpd_req_t *req){

    httpd_resp_send(req, mqtt, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/*
esp_err_t amp_handler(httpd_req_t *req){

    httpd_resp_send(req, amp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}*/

esp_err_t get_handler_str(httpd_req_t *req){
    // Read the URI line and get the host
    char *buf;
    int buf_len;
    buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;
    if (buf_len > 1)
    {
        buf = malloc(buf_len);
        if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK)
        {
            ESP_LOGI(TAG, "Host: %s", buf);
        }
        free(buf);
    }

    // Read the URI line and get the parameters
     buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found URL query: %s", buf);
            char param1[32];
            char param2[32];
            char param3[32];
            if (httpd_query_key_value(buf, "SSID", param1, sizeof(param1)) == ESP_OK) {
                ESP_LOGI(TAG, "The ssid value = %s", param1);
                _event_handler(WEBSERVER_EVENT_NEW_SSID, param1, strlen(param1));
            }
            if (httpd_query_key_value(buf, "PASSWD", param2, sizeof(param2)) == ESP_OK) {
               ESP_LOGI(TAG, "The passwd value = %s", param2);
               _event_handler(WEBSERVER_EVENT_NEW_PASSWORD, param2, strlen(param2));
            }
            if (httpd_query_key_value(buf, "server", param1, sizeof(param1)) == ESP_OK) {
                ESP_LOGI(TAG, "The Server value = %s", param1);
                _event_handler(WEBSERVER_EVENT_NEW_MQTT_SERVER_IP, param1, strlen(param1));
            }
            if (httpd_query_key_value(buf, "topic1", param2, sizeof(param2)) == ESP_OK) {
            	tools_split_string(param2, sizeof(param2));
            	ESP_LOGI(TAG, "The Topic1 value = %s", param2);
            	_event_handler(WEBSERVER_EVENT_NEW_MQTT_TOPIC_1, param2, strlen(param2));
            }
            if (httpd_query_key_value(buf, "topic2", param3, sizeof(param3)) == ESP_OK) {
            	tools_split_string(param3, sizeof(param3));
            	ESP_LOGI(TAG, "The Topic2 value = %s", param3);
            	_event_handler(WEBSERVER_EVENT_NEW_MQTT_TOPIC_2, param3, strlen(param3));
            	//httpd_resp_send(req, mqtt, HTTPD_RESP_USE_STRLEN);
            }
        }
        free(buf);
    }

    // The response
   // const char resp[] = "The data was sent ...";

    return ESP_OK;
}

esp_err_t post_handler(httpd_req_t *req){
	char content[100];
	char param1[32];
	char param2[32];
	char param3[32];
	memset(content, 0x00, sizeof(content));
	memset(param1, 0x00, sizeof(param1));
	memset(param2, 0x00, sizeof(param2));
	memset(param3, 0x00, sizeof(param3));
	int recv_size = MIN(req->content_len, sizeof(content));
	httpd_req_recv(req, content, recv_size);
	//httpd_req_recv(req, content, sizeof(content));
	printf("Content received: %s\n", content);
	httpd_resp_send(req, root, HTTPD_RESP_USE_STRLEN);
	vTaskDelay(1000 / portTICK_PERIOD_MS);
	if (httpd_query_key_value(content, "SSID", param1, sizeof(param1)) == ESP_OK) {
		_event_handler(WEBSERVER_EVENT_NEW_SSID, param1, strlen(param1));
	}
	if (httpd_query_key_value(content, "PASSWD", param2, sizeof(param2)) == ESP_OK) {
		_event_handler(WEBSERVER_EVENT_NEW_PASSWORD, param2, strlen(param2));
	}
	if (httpd_query_key_value(content, "server", param1, sizeof(param1)) == ESP_OK) {
		_event_handler(WEBSERVER_EVENT_NEW_MQTT_SERVER_IP, param1, strlen(param1));
	}
	if (httpd_query_key_value(content, "topic1", param2, sizeof(param2)) == ESP_OK) {
		tools_split_string(param2, sizeof(param2));
		_event_handler(WEBSERVER_EVENT_NEW_MQTT_TOPIC_1, param2, strlen(param2));
	}
	if (httpd_query_key_value(content, "topic2", param3, sizeof(param3)) == ESP_OK) {
		tools_split_string(param3, sizeof(param3));
		_event_handler(WEBSERVER_EVENT_NEW_MQTT_TOPIC_2, param3, strlen(param3));
	}
	return ESP_OK;
}

esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    if (strcmp("/hello", req->uri) == 0) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "/hello URI is not available");
        /* Return ESP_OK to keep underlying socket open */
        return ESP_OK;
    } else if (strcmp("/echo", req->uri) == 0) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "/echo URI is not available");
        /* Return ESP_FAIL to close underlying socket */
        return ESP_FAIL;
    }
    /* For any other URI send 404 and close socket */
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Some 404 error message");
    return ESP_FAIL;
}

/* URI handler structure for GET /uri */
httpd_uri_t uri_root= {
    .uri = "/",
    .method = HTTP_GET,
    .handler = root_handler,
    .user_ctx = NULL};

httpd_uri_t uri_wifi = {
    .uri = "/wifi",
    .method = HTTP_GET,
    .handler = wifi_handler,
    .user_ctx = NULL};

httpd_uri_t uri_mqtt = {
    .uri = "/mqtt",
    .method = HTTP_GET,
    .handler = mqtt_handler,
    .user_ctx = NULL};

/*
httpd_uri_t uri_amp = {
    .uri = "/amp",
    .method = HTTP_GET,
    .handler = amp_handler,
    .user_ctx = NULL};
*/
httpd_uri_t uri_get_input = {
    .uri = "/get",
    .method = HTTP_GET,
    .handler = get_handler_str,
    .user_ctx = NULL};

httpd_uri_t uri_post = {
    .uri = "/post",
    .method = HTTP_POST,
    .handler = post_handler,
    .user_ctx = NULL};


void start_webserver(webserver__event_handle_cb event_handler)
{
			config.task_priority      			= tskIDLE_PRIORITY+5;
			config.stack_size        			= 4096;
			config.core_id            			= tskNO_AFFINITY;
			config.server_port        			= 80;
			config.ctrl_port          			= 32768;
			config.max_open_sockets				= 7;
			config.max_uri_handlers  			= 8;
			config.max_resp_headers   			= 8;
			config.backlog_conn       			= 5;
			config.lru_purge_enable   			= false;
			config.recv_wait_timeout  			= 5;
			config.send_wait_timeout  			= 5;
			config.global_user_ctx 				= NULL;
			config.global_user_ctx_free_fn 		= NULL;
			config.global_transport_ctx 		= NULL;
			config.global_transport_ctx_free_fn = NULL;
			config.open_fn 			 			= NULL;
			config.close_fn 		 			= NULL;
			config.uri_match_fn 	 			= NULL;

	_event_handler=event_handler;

	stop_webserver();
	ESP_LOGI(TAG,"!!!!!!!Starting WEBSERVER ONE !!!!!!");
    if (httpd_start(&server, &config) == ESP_OK)
    {
        ESP_LOGI(TAG,"!!!!!!!STARTING WEBSERVER TWO !!!!!!");
        httpd_register_uri_handler(server, &uri_root);
    	httpd_register_uri_handler(server, &uri_wifi);
        httpd_register_uri_handler(server, &uri_mqtt);
        httpd_register_uri_handler(server, &uri_get_input);
        httpd_register_uri_handler(server, &uri_post);
        httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, http_404_error_handler);
    }
    //return server;
}

void stop_webserver()
{
    if (server)
    {
    	ESP_LOGI(TAG,"!!!!!!!Stopping WEBSERVER!!!!!!");
       // httpd_stop(server);
        ESP_LOGI(TAG,"!!!!!!!Stopping WEBSERVER 2 !!!!!!");
        server=NULL;
    }
}

void webserver_init(audio_element_handle_t eq, audio_board_handle_t bh){

}



