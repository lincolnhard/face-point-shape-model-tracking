/*
 ============================================================================
 Name        : fd.h
 Author      : LincolnHard
 Version     :
 Copyright   : free and open
 Description :
 ============================================================================
 */

#ifndef FD_H_
#define FD_H_
#include "general_lincoln.h"
/* the minimum face size is 24 * FD_START_SCALE pixels */
#define FD_START_SCALE 5.0f
#define FD_SCALE_FACTOR 1.2f
#define DETECTION_ROI_MOVING_STEP 1
#define CLASSIFY_START_STAGE 0
#define FD_MIN_NEIGHBORS 1
#define GROUP_EPS 0.4f
#define GROUP_THRESHOLD 1
/* note: below are actually all constant (do not change them because they are related to the face_classifier.txt)*/
#define VJ_EMPIRICAL_DETECTION_ROI 24
#define VJ_EMPIRICAL_DETECTION_ROI_AREA 576
#define FD_TOTAL_STAGES 25
#define FD_TOTAL_FILTERS 2913

/* Remember to initialize memory pool before initialize face detection */
/* note that there are no free fd function, buffers will be free with memory pool */
void init_face_detection(const char* filename);
vector_lincoln* face_detect(const unsigned char* src);
void draw_fd_result(unsigned char* src);

#endif /* FD_H_ */
