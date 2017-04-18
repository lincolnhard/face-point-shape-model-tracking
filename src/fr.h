/*
 * fr.h
 *
 *  Created on: 2016/1/4
 *      Author: lincoln
 */

#ifndef FR_H_
#define FR_H_
#include "general_lincoln.h"
#define TOTAL_FACES 30
#define TRAIN_IMG_SIZE 30
#define EIGEN_VECTOR_NUM 6
#define PEOPLE_IN_DATABASE 3
#define EACH_PERSON_PIC_NUM 10
#define TRAIN_IMG_TOTAL_PIXEL 900

void init_face_recognition(const char* filename);
int face_recognize(const unsigned char* src, const rect_u16_lincoln face_region_rectangle);
void free_face_recognition();

#endif /* FR_H_ */
