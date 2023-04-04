idf_component_register(SRCS "webserver.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_http_server tools wifi_interface spiffs audio_pipeline esp-adf-libs audio_board)
