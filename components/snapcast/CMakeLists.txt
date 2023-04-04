idf_component_register(SRCS "snapcast.c"
                       INCLUDE_DIRS "include"
                       REQUIRES buffer tools json sntp_client audio_stream audio_pipeline )

