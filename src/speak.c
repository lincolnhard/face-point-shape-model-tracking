/*
 * speak.c
 *
 *  Created on: 2016/1/5
 *      Author: lincoln
 */
#include <stdlib.h>
#include <string.h>
#include "speak_lib.h"

void init_speak(){
	espeak_Initialize(AUDIO_OUTPUT_PLAYBACK, 0, NULL, 0);
}
void speak_lincoln(const char* sentence){
	int synth_flags = espeakCHARS_AUTO | espeakPHONEMES | espeakENDPAUSE;
	if(sentence != NULL){
		int size = strlen(sentence);
		espeak_Synth(sentence, size + 1, 0, POS_CHARACTER, 0, synth_flags, NULL, NULL);
	}
	if(espeak_Synchronize() != EE_OK){
		fprintf(stderr, "espeak_Synchronize() failed, maybe error when opening output device\n");
		exit(4);
	}
}
