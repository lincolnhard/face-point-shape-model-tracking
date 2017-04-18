/*
 * face_tracker.c
 *
 *  Created on: 2016/1/17
 *      Author: lincoln
 */
#include "face_tracker.h"
#include "fd.h"
#include "eye_check.h"
#include <math.h>

extern const float detector_offset[3];
extern const float detector_reference[FEATURE_POINTS_NUMBER_X_2];
extern const double shapemodel_V[FEATURE_POINTS_NUMBER_X_2 * SUB_SPACE_DIM];
extern const double shapemodel_e[SUB_SPACE_DIM];
extern const float patchmodel_reference[FEATURE_POINTS_NUMBER_X_2];
extern const float patchmodel_patchmean[FEATURE_POINTS_NUMBER];
extern const float patchmodel_patchstddev[FEATURE_POINTS_NUMBER];
extern const float patchmodel_patches[FEATURE_POINTS_NUMBER][PATCH_SIZE_X_PATCH_SIZE];
/* log table based on exponential for input range 1 ~ 256 */
extern const float logTable[256];

static face_tracker_params tracker_params;

static void check_ft_status(){
	/* 0.473669 is normalized sample size parameter */
	unsigned short face_width = (unsigned short)(tracker_params.p[0] * 0.473669);
	float yaw_angle = 45.0f * (float)tracker_params.p[1] / ((float)tracker_params.p[0]+ 0.0001f);
	float response_sum = 0;
	int i;
	for(i = 0; i < FEATURE_POINTS_NUMBER; i++){
		response_sum += tracker_params.match_responses[i];
	}
	response_sum *= FEATURE_POINTS_NUMBER_RECIPROCAL;
	/* reset(re-detect) condition: 1. face width 2. yaw angle 3. response result */
	if(face_width > 250.0f || face_width < 110.0f || yaw_angle > 35.0f || yaw_angle < -35.0f || response_sum < 0.5f){
		tracker_params.track_reset_count += (tracker_params.track_reset_count < FACE_TRACKING_RESET_COUNT_BORDER);
	} else {
		tracker_params.track_reset_count -= (tracker_params.track_reset_count > 0);
	}
	if(tracker_params.track_reset_count > FACE_TRACKING_RESET_COUNT_TRESHOLD){
		tracker_params.track_reset_count = 0;
		tracker_params.isTracking = false;
	}
}

static float find_max(const float* __restrict input_tm_result, const unsigned char search_size, point_2f32_lincoln* max_location){
	int i;
	int location = 0;
	float maximum = input_tm_result[0];
	const unsigned char resultsize = search_size + 1;
	for(i = 0; i < resultsize * resultsize; i++){
		if(input_tm_result[i] > maximum){
			maximum = input_tm_result[i];
			location = i;
		}
	}
	max_location->x = (int)(location % resultsize);
	max_location->y = (int)(location / resultsize);
	return maximum;
}

static void log_and_template_match(const int index, const float* __restrict patch, const unsigned char wsize,
											  const unsigned char* __restrict affined_patch, float* log_affined_patch,
											  float* __restrict ncc_result){
	int x, y, i, j;
	int count = 0;
	float temp_pixel = 0.0f;
	float src_roi_square_sum = 0.0f;
	float src_roi_mean = 0.0f;

	for(i = 0; i < wsize * wsize; i++){
		log_affined_patch[i] = logTable[affined_patch[i]];
	}
#if 0 /* pre-training each patch mean and standard deviation */
	for(j = 0; j < 24; j++){
		float patchmean_lincoln = 0.0f;
		for(i = 0; i < 121; i++){
			patchmean_lincoln += patchmodel_patches[j][i];
		}
		patchmean_lincoln = patchmean_lincoln / 121.0f;

		float patchstddev_lincoln = 0.0f;
		for(i = 0; i < 121; i++){
			patchstddev_lincoln += (patchmodel_patches[j][i] - patchmean_lincoln) * (patchmodel_patches[j][i] - patchmean_lincoln);
		}
		patchstddev_lincoln = sqrt(patchstddev_lincoln);
		printf("patchstddev_lincoln = %f\n", patchstddev_lincoln);
	}
#endif
	for(y = 5; y < wsize - 5; y++){
		for(x = 5; x < wsize - 5; x++){
			temp_pixel = 0.0f;
			src_roi_square_sum = 0.0f;
			src_roi_mean = 0.0f;
			for(j = -5; j < 6; j++){
				for(i = -5; i < 6; i++){
					src_roi_mean += log_affined_patch[x + i + (y + j) * wsize];
				}
			}
			/* note: 0.0082644628 = 1 / 121 (hard code here, the value should manual adjust with PATCH_SIZE) */
			src_roi_mean *= 0.0082644628;

			for(j = -5; j < 6; j++){
				for(i = -5; i < 6; i++){
					float tempix_minus_mean = log_affined_patch[x + i + (y + j) * wsize] - src_roi_mean;
					temp_pixel += tempix_minus_mean * (patch[i + 5 + (j + 5) * 11] - patchmodel_patchmean[index]);
					src_roi_square_sum += tempix_minus_mean * tempix_minus_mean;
				}
			}
			/* normalized cross-correlation score values range from 1 (perfect match) to -1 (completely anti-correlated) */
			ncc_result[count] = temp_pixel / (patchmodel_patchstddev[index] * sqrt(src_roi_square_sum));
			count++;
		}
	}
}

static void affine_transform(const unsigned char* __restrict src, const float* __restrict M,
									  const unsigned char wsize, unsigned char* __restrict affined_dst){
	int x, y;
	int count = 0;
	for(y = 0; y < wsize; y++){
		for(x = 0; x < wsize; x++){
			float fx = M[0] * x + M[1] * y + M[2];
			float fy = M[3] * x + M[4] * y + M[5];
			int sy = (int)fy;
			fy -= sy;
			short cbufy[2];
			cbufy[0] = (1.0f - fy) * 2048;
			cbufy[1] = 2048 - cbufy[0];

			int sx = (int)fx;
			fx -= sx;
			short cbufx[2];
			cbufx[0] = (1.0f - fx) * 2048;
			cbufx[1] = 2048 - cbufx[0];

			affined_dst[count] = (src[sx + sy * IMG_WIDTH] * cbufx[0] * cbufy[0] +
										 src[sx + (sy + 1) * IMG_WIDTH] * cbufx[0] * cbufy[1] +
										 src[sx + 1 + sy * IMG_WIDTH] * cbufx[1] * cbufy[0] +
										 src[sx + 1 + (sy + 1) * IMG_WIDTH] * cbufx[1] * cbufy[1]) >> 22;

			count++;
		}
	}
}

static void apply_simil(const float* __restrict sim, const point_2f32_lincoln* in_pts, point_2f32_lincoln* out_pts) {
	int i;
	for (i = 0; i < FEATURE_POINTS_NUMBER; i++) {
		out_pts[i].x = sim[0] * in_pts[i].x + sim[1] * in_pts[i].y + sim[2];
		out_pts[i].y = sim[3] * in_pts[i].x + sim[4] * in_pts[i].y + sim[5];
	}
}

static void calc_affine_simil(float* __restrict sim_ref_to_cur, float* __restrict sim_cur_to_ref){
	float mx = 0, my = 0;
	int i;
	for(i = 0; i < FEATURE_POINTS_NUMBER; i++){
		mx += tracker_params.feature_points[i].x;
		my += tracker_params.feature_points[i].y;
	}
	mx *= FEATURE_POINTS_NUMBER_RECIPROCAL;
	my *= FEATURE_POINTS_NUMBER_RECIPROCAL;

	float b = 0, c = 0;
	for(i = 0; i < FEATURE_POINTS_NUMBER; i++){
		b += (patchmodel_reference[2 * i] * (tracker_params.feature_points[i].x - mx) + patchmodel_reference[2 * i + 1] * (tracker_params.feature_points[i].y - my));
		c += (patchmodel_reference[2 * i] * (tracker_params.feature_points[i].y - my) - patchmodel_reference[2 * i + 1] * (tracker_params.feature_points[i].x - mx));
	}
	/* 2.243621e-05 = patchmodel_reference square sum reciprocal */
	float scale = sqrt(b * b + c * c) * 2.243621e-05;
	float theta = atan2(c, b);
	float sc = scale * cos(theta);
	float ss = scale * sin(theta);
	sim_ref_to_cur[0] = sc;
	sim_ref_to_cur[1] = -ss;
	sim_ref_to_cur[2] = mx;
	sim_ref_to_cur[3] = ss;
	sim_ref_to_cur[4] = sc;
	sim_ref_to_cur[5] = my;

	float recip_d = 1/(scale * scale);
	sim_cur_to_ref[0] = sim_ref_to_cur[4] * recip_d;
	sim_cur_to_ref[1] = -sim_ref_to_cur[1] * recip_d;
	sim_cur_to_ref[2] = (sim_ref_to_cur[1] * sim_ref_to_cur[5] - sim_ref_to_cur[2] * sim_ref_to_cur[4]) * recip_d;
	sim_cur_to_ref[3] = -sim_ref_to_cur[3] * recip_d;
	sim_cur_to_ref[4] = sim_ref_to_cur[0] * recip_d;
	sim_cur_to_ref[5] = (sim_ref_to_cur[2] * sim_ref_to_cur[3] - sim_ref_to_cur[0] * sim_ref_to_cur[5]) * recip_d;
}

static void calc_peaks(const unsigned char* src, const unsigned char fitting_search_size){
	/* make two affine transformation matrix */
	/* 1. frontal shape (reference) to current shape (tracker_params.points) */
	float affinematrix_front_to_current[6] = {0};
	/* 2. current shape (tracker_params.points) to frontal shape (reference) */
	float affinematrix_current_to_front[6] = {0};
	calc_affine_simil(affinematrix_front_to_current, affinematrix_current_to_front);
	/* note: should check whether __STDC_IEC_559__ is defined */
	point_2f32_lincoln* transformed_points = (point_2f32_lincoln*)malloc_lincoln(FEATURE_POINTS_NUMBER * sizeof(point_2f32_lincoln));
	apply_simil(affinematrix_current_to_front, tracker_params.feature_points, transformed_points);

	/* the last col of affine transform is an adjustment, let feature points be the center of its related patches */
	float affine_matrix_temp[6] = {0};
	affine_matrix_temp[0] = affinematrix_front_to_current[0];
	affine_matrix_temp[1] = affinematrix_front_to_current[1];
	affine_matrix_temp[3] = affinematrix_front_to_current[3];
	affine_matrix_temp[4] = affinematrix_front_to_current[4];

	unsigned char wsize = fitting_search_size + PATCH_SIZE;
	unsigned char* affined_patch = (unsigned char*)malloc_lincoln(wsize * wsize * sizeof(unsigned char));
	float* log_affined_patch = (float*)malloc_lincoln(wsize * wsize * sizeof(float));
	/* note: should check whether __STDC_IEC_559__ is defined */
	float* templ_match_result = (float*)malloc_lincoln((fitting_search_size + 1) * (fitting_search_size + 1) * sizeof(float));
	int i;
	for(i = 0; i < FEATURE_POINTS_NUMBER; i++){
		/* adjust patch center */
		float halfwidth = (float)(wsize - 1) * 0.5f;
		affine_matrix_temp[2] = tracker_params.feature_points[i].x - (affine_matrix_temp[0] * halfwidth + affine_matrix_temp[1] * halfwidth);
		affine_matrix_temp[5] = tracker_params.feature_points[i].y - (affine_matrix_temp[3] * halfwidth + affine_matrix_temp[4] * halfwidth);
		affine_transform(src, affine_matrix_temp, wsize, affined_patch);
		log_and_template_match(i, patchmodel_patches[i], wsize, affined_patch, log_affined_patch, templ_match_result);

		point_2f32_lincoln max_loc = {0.0f, 0.0f};
		tracker_params.match_responses[i] = find_max(templ_match_result, fitting_search_size, &max_loc);

		transformed_points[i].x += (max_loc.x - 0.5f * fitting_search_size);
		transformed_points[i].y += (max_loc.y - 0.5f * fitting_search_size);
	}
	apply_simil(affinematrix_front_to_current, transformed_points, tracker_params.feature_points);
}

static void calc_shape(double* __restrict p, point_2f32_lincoln* pts) {
	int i = 0, j = 0;
	double tempx = 0.0, tempy = 0.0;
	for (j = 0; j < FEATURE_POINTS_NUMBER; j++) {
		tempx = 0.0f;
		tempy = 0.0f;
		for (i = 0; i < SUB_SPACE_DIM; i++) {
			tempx += shapemodel_V[i + SUB_SPACE_DIM_X_2 * j] * p[i];
			tempy += shapemodel_V[i + SUB_SPACE_DIM_X_2 * j + SUB_SPACE_DIM] * p[i];
		}
		pts[j].x = tempx;
		pts[j].y = tempy;
	}
}

static void clamp(double* __restrict p){
	/* todo :lots of post modified here, tuned based on testing */
	double scale = p[0];
	/* the p[0] to p[3] are the rigid parameter, so the iteration start from p[4] */
	/* ignore p[5], p[7] to reduced eigen-space dimension */
	/* up-down */
	double v4 = CLAMP_FACTOR * shapemodel_e[4] * scale;
	/* left-right */
	double v6 = 1.1 * CLAMP_FACTOR * shapemodel_e[6] * scale;

	if(fabs(p[4]) > v4){
		if(p[4] > 0){
			p[4] = v4;
		}
		else{
			p[4] = -v4;
		}
	}
	if(fabs(p[6]) > v6){
		if(p[6] > 0){
			p[6] = v6;
		}
		else {
			p[6] = -v6;
		}
	}
	/* allow more room for yaw-angle turning */
	p[6] *= 1.1;
	p[5] = 0.0;
	p[7] = 0.0;
}

static void calc_params(point_2f32_lincoln* pts, double* __restrict p){
	int i;
	memset(p, 0, SUB_SPACE_DIM * sizeof(double));
	for(i = 0; i < FEATURE_POINTS_NUMBER; i++){
		p[0] += pts[i].x * shapemodel_V[SUB_SPACE_DIM_X_2 * i + 0] + pts[i].y * shapemodel_V[SUB_SPACE_DIM_X_2 * i + 8];
		p[1] += pts[i].x * shapemodel_V[SUB_SPACE_DIM_X_2 * i + 1] + pts[i].y * shapemodel_V[SUB_SPACE_DIM_X_2 * i + 9];
		p[2] += pts[i].x * shapemodel_V[SUB_SPACE_DIM_X_2 * i + 2] + pts[i].y * shapemodel_V[SUB_SPACE_DIM_X_2 * i + 10];
		p[3] += pts[i].x * shapemodel_V[SUB_SPACE_DIM_X_2 * i + 3] + pts[i].y * shapemodel_V[SUB_SPACE_DIM_X_2 * i + 11];
		p[4] += pts[i].x * shapemodel_V[SUB_SPACE_DIM_X_2 * i + 4] + pts[i].y * shapemodel_V[SUB_SPACE_DIM_X_2 * i + 12];
		//p[5] += pts[i].x * shapemodel_V[SUB_SPACE_DIM_X_2 * i + 5] + pts[i].y * shapemodel_V[SUB_SPACE_DIM_X_2 * i + 13];
		p[6] += pts[i].x * shapemodel_V[SUB_SPACE_DIM_X_2 * i + 6] + pts[i].y * shapemodel_V[SUB_SPACE_DIM_X_2 * i + 14];
		//p[7] += pts[i].x * shapemodel_V[SUB_SPACE_DIM_X_2 * i + 7] + pts[i].y * shapemodel_V[SUB_SPACE_DIM_X_2 * i + 15];
	}
	clamp(p);
}

static void fit(const unsigned char* src, const unsigned char fitting_search_size){
	calc_peaks(src, fitting_search_size);
	calc_params(tracker_params.feature_points, tracker_params.p);
	calc_shape(tracker_params.p, tracker_params.feature_points);
}

static bool detect_face_and_init_feature_positions(const unsigned char* src){
	/* face detection */
	vector_lincoln* fdresult = face_detect(src);
	if(fdresult->num_elems > 0){
		rect_u16_lincoln faceroi = ((rect_u16_lincoln*)fdresult->beginning)[0];
		float face_center_x = (float)(faceroi.x + (faceroi.w >> 1));
		float face_center_y = (float)(faceroi.y + (faceroi.h >> 1));
		float patch_avg_center_offset_x = detector_offset[0] * faceroi.w;
		float patch_avg_center_offset_y = detector_offset[1] * faceroi.w;
		float patch_avg_size_offset = detector_offset[2] * faceroi.w;
		int i;
		for(i = 0; i < FEATURE_POINTS_NUMBER; i++){
			tracker_params.feature_points[i].x = face_center_x + patch_avg_center_offset_x + patch_avg_size_offset * detector_reference[2 * i];
			tracker_params.feature_points[i].y = face_center_y + patch_avg_center_offset_y + patch_avg_size_offset * detector_reference[2 * i + 1];
		}
		return true;
	}
	else{
		return false;
	}
}

/*
 * the feature points layout and index (there are four regions: left eye, right eye, nose, mouth)
 * 			1			   6
 *	  	   0  4  2                 7  9  5
 *		  	3 			   8
 *		  	        10   16
 *		  	      11	   15
 *		  	       12 13 14
 *
 *		  	       18 19 20
 *		  	      17         21
 *		  	          23  22
 */
static void set_eye_roi(){
	/* left eye */
	tracker_params.eyeroi[0].x = (unsigned short)tracker_params.feature_points[0].x;
	tracker_params.eyeroi[0].y = (unsigned short)tracker_params.feature_points[1].y;
	tracker_params.eyeroi[0].w = (unsigned short)(tracker_params.feature_points[2].x - tracker_params.feature_points[0].x);
	tracker_params.eyeroi[0].h = (unsigned short)(tracker_params.feature_points[3].y - tracker_params.feature_points[1].y);
	/* right eye */
	tracker_params.eyeroi[1].x = (unsigned short)tracker_params.feature_points[7].x;
	tracker_params.eyeroi[1].y = (unsigned short)tracker_params.feature_points[6].y;
	tracker_params.eyeroi[1].w = (unsigned short)(tracker_params.feature_points[5].x - tracker_params.feature_points[7].x);
	tracker_params.eyeroi[1].h = (unsigned short)(tracker_params.feature_points[8].y - tracker_params.feature_points[6].y);
	/* assert eye range */
	tracker_params.eyeroi[0].x *= (tracker_params.eyeroi[0].x > 0);
	tracker_params.eyeroi[0].y *= (tracker_params.eyeroi[0].y > 0);
	tracker_params.eyeroi[1].x *= (tracker_params.eyeroi[1].x > 0);
	tracker_params.eyeroi[1].y *= (tracker_params.eyeroi[1].y > 0);
	if(tracker_params.eyeroi[0].x + tracker_params.eyeroi[0].w >= IMG_WIDTH) {
		tracker_params.eyeroi[0].w = IMG_WIDTH - 1 - tracker_params.eyeroi[0].x;
	}
	if (tracker_params.eyeroi[0].y + tracker_params.eyeroi[0].h >= IMG_HEIGHT) {
		tracker_params.eyeroi[0].h = IMG_HEIGHT - 1 - tracker_params.eyeroi[0].y;
	}
	if (tracker_params.eyeroi[1].x + tracker_params.eyeroi[1].w >= IMG_WIDTH) {
		tracker_params.eyeroi[1].w = IMG_WIDTH - 1 - tracker_params.eyeroi[1].x;
	}
	if (tracker_params.eyeroi[1].y + tracker_params.eyeroi[1].h >= IMG_HEIGHT) {
		tracker_params.eyeroi[1].h = IMG_HEIGHT - 1 - tracker_params.eyeroi[1].y;
	}
}

void face_track(const unsigned char* src){
	if(tracker_params.isTracking == false){
		bool isFaceExist = detect_face_and_init_feature_positions(src);
		if(isFaceExist == false){
			tracker_params.isTracking = false;
			return;
		}
		else{
			tracker_params.isTracking = true;
		}
	}
	int level;
	for(level = 0; level < 2; level++){
		fit(src, tracker_params.search_size[level]);
	}

	//set_eye_roi();
	//drowsy_eye_detection(src, tracker_params.eyeroi);
	check_ft_status();
}

void draw_ft_result(unsigned char* src){
	if(tracker_params.isTracking){
		int i, j;
#if 1 /* draw all feature points */
		for(i = 0; i < FEATURE_POINTS_NUMBER; i++){
			for(j = -1; j < 2; j++){
				memset(src + (int)tracker_params.feature_points[i].x - 1 + ((int)tracker_params.feature_points[i].y + j) * IMG_WIDTH, 255, 3);
			}
		}
#else /* draw only eye roi */
		for(i = 0; i < 2; i++){
			for(j = -1; j < 2; j++){
				memset(src + (int)tracker_params.eyeroi[i].x + ((int)tracker_params.eyeroi[i].y + j) * IMG_WIDTH, 255, (int)tracker_params.eyeroi[i].w);
				memset(src + (int)tracker_params.eyeroi[i].x + ((int)tracker_params.eyeroi[i].y + (int)tracker_params.eyeroi[i].h + j) * IMG_WIDTH, 255, (int)tracker_params.eyeroi[i].w);
			}
		}
#endif
	}
}

void key_debug_ft(){
	printf("isTracking: %d\n", tracker_params.isTracking);
	printf("track_reset_count: %d\n", tracker_params.track_reset_count);
	printf("featurex: %d\n", (int)tracker_params.feature_points[0].x);
	printf("featurey: %d\n", (int)tracker_params.feature_points[0].y);
}

void init_face_tracker(const char* filename){
	tracker_params.search_size[0] = 19;
	tracker_params.search_size[1] = 9;
	tracker_params.isTracking = false;
	tracker_params.track_reset_count = 0;
	init_face_detection(filename);
}
