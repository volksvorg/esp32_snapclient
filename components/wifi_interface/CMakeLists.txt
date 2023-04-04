idf_component_register(SRCS "wifi_interface.c"
                       INCLUDE_DIRS "include"
                       REQUIRES my_mqtt esp_peripherals webserver sntp_client snapcast audio_pipeline audio_stream)
