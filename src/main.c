/*
 ============================================================================
 Name        : main.c
 Author      : LincolnHard
 Version     :
 Copyright   : free and open
 Description : Hello World in C, Ansi-style
 ============================================================================
 */
#include "video_capture.h"
#include "draw_framebuffer.h"
#include "general_lincoln.h"
#include "speak.h"
#include "face_tracker.h"
#include <time.h> //for profiling

extern void* Pool_Starting_Address;
extern char* pool_current_address;
extern unsigned int pool_current_alloc_size;
#define SINGLEIMAGEDEBUGx

int main(){
	initialize_memory_pool_lincoln();
	unsigned char src_image[IMG_TOTAL_PIXEL];
#ifndef SINGLEIMAGEDEBUG
	init_framebuffer();
	init_video_capture();
	init_speak();
#endif
	init_face_tracker("face_classifier.txt");
	int i;
	char key = 0;

	for(i = 0; i < 10000; i++){
		pool_current_address = Pool_Starting_Address + MEMORYPOOL_RESERVE_SIZE;
		pool_current_alloc_size = MEMORYPOOL_RESERVE_SIZE;
#ifndef SINGLEIMAGEDEBUG
		key = video_capture(src_image, IMG_TOTAL_PIXEL);
#else
		read_pgm("num286.pgm", src_image);
#endif
		face_track(src_image);
		draw_ft_result(src_image);
#ifndef SINGLEIMAGEDEBUG
		if(key == 'e'){
			speak_lincoln("how do you do");
		}
		else if(key == 's'){
			imsize_u16_lincoln srcsize = {IMG_WIDTH, IMG_HEIGHT};
			char tempstring[32];
			sprintf(tempstring, "num%d.pgm", i);
			write_pgm(tempstring, src_image, srcsize, NULL);
			speak_lincoln("pic saved");
		}
		else if(key == 'q'){
			break;
		}
		else if(key == 'm'){
			key_debug_ft();
		}

		draw_framebuffer(src_image);
#endif
	}
#ifndef SINGLEIMAGEDEBUG
	free_video_capture();
	free_framebuffer();
#endif
	free_memory_pool_lincoln();

	puts("bye bye");
	return EXIT_SUCCESS;
}
