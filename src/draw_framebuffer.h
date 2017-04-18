/*
 * draw_framebuffer.h
 *
 *  Created on: Dec 24, 2015
 *      Author: erasmus
 */
/*
 * may encounter permission problem
 * /dev/fb0 is in the video group so one must add itself to the video group
 * command: # gpasswd -a {user} video
 */
#ifndef DRAW_FRAMEBUFFER_H_
#define DRAW_FRAMEBUFFER_H_
void init_framebuffer();
void draw_framebuffer(unsigned char* src);
void free_framebuffer();
#endif /* DRAW_FRAMEBUFFER_H_ */
