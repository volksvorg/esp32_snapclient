#ifndef _WIFI_INTERFACE_H_
#define _WIFI_INTERFACE_H_
#include "raw_stream.h"
#include "audio_common.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "esp_event_base.h"
void wifi_interface_deinit(void);
void wifi_init_station(char *ssid, char *passwd);
void wifi_interface_init(esp_event_handler_t handle);
void wifi_interface_softap_start(void);
void wifi_interface_station_start(char *ssid, char *passwd);
#endif /* _WIFI_INTERFACE_H_ */
