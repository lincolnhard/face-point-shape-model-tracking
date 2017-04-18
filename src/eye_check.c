/*
 * eye_check.c
 *
 *  Created on: 2016/1/31
 *      Author: lincoln
 */
#include "general_lincoln.h"

void drowsy_eye_detection(const unsigned char* src, const rect_u16_lincoln* eyerect){
	int i, j, index;
	unsigned int accumtemp[2] = {0, 0};
	for(j = 0; j < eyerect[0].h; j++){
		for(i = 0; i < eyerect[0].w; i++){
			index = eyerect[0].x + i + (eyerect[0].y + j) * IMG_WIDTH;
			accumtemp[0] += src[index];
		}
	}
	printf("eye pix accum: %d\n", accumtemp[0]);
}
