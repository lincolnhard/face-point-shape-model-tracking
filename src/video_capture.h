/*
 * video_capture.h
 *
 *  Created on: Dec 24, 2015
 *      Author: Lincoln
 */

#ifndef VIDEO_CAPTURE_H_
#define VIDEO_CAPTURE_H_
void init_video_capture();
char video_capture(unsigned char* dst, const int dstlen);
void free_video_capture();
#endif /* VIDEO_CAPTURE_H_ */
