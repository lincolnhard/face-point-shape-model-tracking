/*
 * speak.h
 *
 *  Created on: 2016/1/5
 *      Author: lincoln
 */

#ifndef SPEAK_H_
#define SPEAK_H_

/*
 * based on eSpeak library
 *
 * steps to make binaries:
 *  		  1. sudo apt-get install libportaudio0
 *			  2. (optional)sudo cp -r espeak-data /usr/share/
 *			  3. sudo ln -s libportaudio.so.0 libportaudio.so
 *			  4. copy either portaudio18.h (old Ubuntu, Debian)
 *			  	  		     or portaudio19.h (new Ubuntu, Fedora) to portaudio.h
 *			  5. (optional)open Makefile change AUDIO = portaudio to AUDIO = portaudio0
 *			  6. make
 *			  7. make install
 *	or just use the pre-compiled shared library in linux_32bit/shared_library/libespeak.so.1.1.48
 *
 *	shared library usage:
 *	1. copy it to /usr/lib and /usr/local/lib
 *	2. make symbol link ln -s libespeak.so.1.1.48 libespeak.so
 *	3. add "espeak" into the eclipse CDT Libraries (GCC C Linker->Libraries->Libraries)
 *	4. include speak_lib.h
 */
void init_speak();
void speak_lincoln(const char* sentence_to_said);

#endif /* SPEAK_H_ */
