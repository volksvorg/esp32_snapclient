/*
 * tools.h
 *
 *  Created on: 29.01.2023
 *      Author: florian
 */

#ifndef MAIN_TOOLS_H_
#define MAIN_TOOLS_H_
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_log.h"
#include "audio_element.h"
#include "equalizer.h"
#include "board.h"
/*
typedef struct{
	unsigned sync:1;
	unsigned new_data:1;
	unsigned :6;
}ring_buffer_bits_t;

typedef struct{
	union{
		ring_buffer_bits_t bits;
		uint8_t bits_buffer;
	};
	char *buffer;
	char *data_write;
	char *data_read;
	uint16_t *data_size_buffer;
}ring_buffer_t;
*/
#define TOOLS_ESP_READ_BUFFER_SIZE    4096
#define TOOLS_SNAP_WIRE_CHUNK_BUFFER  3840
#define TOOLS_RINGBUFFER_ELEMENTS     50
#define TOOLS_SNAP_BUFFER_ELEMENTS    TOOLS_SNAP_WIRE_CHUNK_BUFFER * TOOLS_RINGBUFFER_ELEMENTS
#define TOOLS_SNAP_TASK_CORE          0
#define TOOLS_SNAP_TASK_PRIO          5
#define TOOLS_TIME_SYNC_TASK_CORE     1
#define TOOLS_TIME_SYNC_TASK_PRIO     5
#define TOOLS_COPY_AUDIO_TASK_CORE    0
#define TOOLS_COPY_AUDIO_TASK_PRIO    7
void tools_split_string(char* str, int size);
int tools_set_eq_gain(audio_element_handle_t self, int *gain);
int tools_set_audio_volume(audio_board_handle_t bh, int volume, int muted);





#endif /* MAIN_TOOLS_H_ */
