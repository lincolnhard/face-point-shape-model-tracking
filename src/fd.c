/*
 ============================================================================
 Name        : fd.c
 Author      : LincolnHard
 Version     :
 Copyright   : free and open
 Description : Hello World in C, Ansi-style
 ============================================================================
 */
#include "fd.h"

/* there are 25 stages in cascade classifier */
/* here records how many Haar filters in each stages */
static unsigned char filters_in_each_stages[FD_TOTAL_STAGES];
/* there are total 2913 filters in the whole classifier */
/* each filter needs 15 parameters to describe 1 Haar features (12 for shape, 3 for weights) */
static unsigned char shape_in_each_filters[34956]; //2913*12
static short weights_in_each_filters[8739]; //2913*3
/* each filter has 1 related threshold */
static short threshold_for_each_filters[2913];
/* each filter also has 2 related values, alpha1 and alpha2 (don't know the exact meaning) */
/* the deal is like when Haar value pass threshold it would return alpha1 else it return alpha2 */
static short alpha1_for_each_filters[2913];
static short alpha2_for_each_filters[2913];
/* finally each stage has 1 related threshold */
static short threshold_for_each_stages[FD_TOTAL_STAGES];
/* detection buffers */
static unsigned char* scaled_img;
static unsigned int* integral_img;
static unsigned int* sqintegral_img;
static unsigned int** scaled_shape_in_each_filters;
/* detection result vector */
static vector_lincoln* fd_result_vec;

void draw_fd_result(unsigned char* src){
	int i;
	for(i = 0; i < fd_result_vec->num_elems; i++){
		rect_u16_lincoln resultrect = ((rect_u16_lincoln*)fd_result_vec->beginning)[i];
		//vector_pop_lincoln(vec, &resultrect);
		draw_rectangle(src, &resultrect);
	}
}

void init_face_detection(const char* filename){
	FILE* fp;
	char linecontent[10];
	fp = fopen(filename, "r");
	if(fp == NULL){
		puts("cannot read essential classifier file");
		return;
	}
	int i, j, k, l;
	int shape_index = 0;
	int weight_index = 0;
	int filter_index = 0;
	for(i = 0; i < FD_TOTAL_STAGES; i++){
		fgets(linecontent, 10, fp);
		filters_in_each_stages[i] = (unsigned char)atoi(linecontent);
	}
	for(i = 0; i < FD_TOTAL_STAGES; i++){
		for(j = 0; j < filters_in_each_stages[i]; j++){
			for(k = 0; k < 3 ; k++){
				for(l = 0; l < 4; l++){
					fgets(linecontent, 10, fp);
					shape_in_each_filters[shape_index] = (unsigned char)atoi(linecontent);
					shape_index++;
				}
				fgets(linecontent, 10, fp);
				weights_in_each_filters[weight_index] = (short)atoi(linecontent);
				weight_index++;
			}
			fgets(linecontent, 10, fp);
			threshold_for_each_filters[filter_index] = (short)atoi(linecontent);
			fgets(linecontent, 10, fp);
			alpha1_for_each_filters[filter_index] = (short)atoi(linecontent);
			fgets(linecontent, 10, fp);
			alpha2_for_each_filters[filter_index] = (short)atoi(linecontent);
			filter_index++;
		}
		fgets(linecontent, 10, fp);
		threshold_for_each_stages[i] = (short)atoi(linecontent);
	}
	fclose(fp);

	scaled_img = (unsigned char*)malloc_lincoln(IMG_WIDTH * IMG_HEIGHT * sizeof(unsigned char));
	integral_img = (unsigned int*)malloc_lincoln(IMG_WIDTH * IMG_HEIGHT * sizeof(unsigned int));
	sqintegral_img = (unsigned int*)malloc_lincoln(IMG_WIDTH * IMG_HEIGHT * sizeof(unsigned int));
	scaled_shape_in_each_filters = (unsigned int**)malloc_lincoln(12 * FD_TOTAL_FILTERS * sizeof(unsigned int*));
	fd_result_vec = (vector_lincoln*)malloc_lincoln(sizeof(vector_lincoln));
	vector_init_lincoln(fd_result_vec, sizeof(rect_u16_lincoln), 0);
}

/* note: the square integral image array type should adjust with image size in case of overflow */
void create_integral_and_sqintegral_image(const unsigned char* src, const imsize_u16_lincoln imsize, unsigned int* integral_dst, unsigned int* sqintegral_dst){
	unsigned short i, j;
	unsigned int row_accum_temp, row_sqaccum_temp;
	unsigned int upleft_accum_temp, upleft_sqaccum_temp;
	unsigned int iterate_index;
	for(j = 0; j < imsize.height; j++){
		row_accum_temp = 0;
		row_sqaccum_temp = 0;
		for(i = 0; i < imsize.width; i++){
			iterate_index = imsize.width * j + i;
			row_accum_temp += src[iterate_index];
			row_sqaccum_temp += src[iterate_index] * src[iterate_index];
			upleft_accum_temp = row_accum_temp;
			upleft_sqaccum_temp = row_sqaccum_temp;
			if(j != 0){
				upleft_accum_temp += integral_dst[iterate_index - imsize.width];
				upleft_sqaccum_temp += sqintegral_dst[iterate_index - imsize.width];
			}
			integral_dst[iterate_index] = upleft_accum_temp;
			sqintegral_dst[iterate_index] = upleft_sqaccum_temp;
		}
	}
}

void set_scaled_buffers_and_roi(unsigned int* (*intimg_roicorners)[4], unsigned int* (*sqintimg_roicorners)[4], unsigned short scaledwidth){
	(*intimg_roicorners)[0] = integral_img;
	(*intimg_roicorners)[1] = integral_img + VJ_EMPIRICAL_DETECTION_ROI -1;
	(*intimg_roicorners)[2] = integral_img + scaledwidth * (VJ_EMPIRICAL_DETECTION_ROI -1);
	(*intimg_roicorners)[3] = integral_img + scaledwidth * (VJ_EMPIRICAL_DETECTION_ROI -1) + VJ_EMPIRICAL_DETECTION_ROI -1;
	(*sqintimg_roicorners)[0] = sqintegral_img;
	(*sqintimg_roicorners)[1] = sqintegral_img + VJ_EMPIRICAL_DETECTION_ROI -1;
	(*sqintimg_roicorners)[2] = sqintegral_img + scaledwidth * (VJ_EMPIRICAL_DETECTION_ROI -1);
	(*sqintimg_roicorners)[3] = sqintegral_img + scaledwidth * (VJ_EMPIRICAL_DETECTION_ROI -1) + VJ_EMPIRICAL_DETECTION_ROI -1;
	int i, j, k;
	int s_idx = 0;
	for(i = 0; i < FD_TOTAL_STAGES; i++){
		for(j = 0; j < filters_in_each_stages[i]; j++){
			for(k = 0; k < 3; k++){
				unsigned char shapex = shape_in_each_filters[s_idx + k * 4];
				unsigned char shapey = shape_in_each_filters[s_idx + k * 4 + 1];
				unsigned char shapew = shape_in_each_filters[s_idx + k * 4 + 2];
				unsigned char shapeh = shape_in_each_filters[s_idx + k * 4 + 3];
				if(k < 2){
					scaled_shape_in_each_filters[s_idx + k * 4] = integral_img + shapex + shapey * scaledwidth;
					scaled_shape_in_each_filters[s_idx + k * 4 + 1] = integral_img + shapex + shapew + shapey * scaledwidth;
					scaled_shape_in_each_filters[s_idx + k * 4 + 2] = integral_img + shapex + (shapey + shapeh) * scaledwidth;
					scaled_shape_in_each_filters[s_idx + k * 4 + 3] = integral_img + shapex + shapew + (shapey + shapeh) * scaledwidth;
				}
				else{
					if((shapex == 0) && (shapey == 0) && (shapew == 0) && (shapeh == 0)){
						scaled_shape_in_each_filters[s_idx + k * 4] = NULL;
						scaled_shape_in_each_filters[s_idx + k * 4 + 1] = NULL;
						scaled_shape_in_each_filters[s_idx + k * 4 + 2] = NULL;
						scaled_shape_in_each_filters[s_idx + k * 4 + 3] = NULL;
					}
					else{
						scaled_shape_in_each_filters[s_idx + k * 4] = integral_img + shapex + shapey * scaledwidth;
						scaled_shape_in_each_filters[s_idx + k * 4 + 1] = integral_img + shapex + shapew + shapey * scaledwidth;
						scaled_shape_in_each_filters[s_idx + k * 4 + 2] = integral_img + shapex + (shapey + shapeh) * scaledwidth;
						scaled_shape_in_each_filters[s_idx + k * 4 + 3] = integral_img + shapex + shapew + (shapey + shapeh) * scaledwidth;
					}
				}
			}
			s_idx += 12;
		}
	}
}

int haar_filters_commitee(const unsigned int roi_sd, const unsigned int roi_pos, int f_idx, int s_idx, int w_idx){
	int filterthreshold = roi_sd * threshold_for_each_filters[f_idx];
	int filtervalue = (*(scaled_shape_in_each_filters[s_idx] + roi_pos) -
							 *(scaled_shape_in_each_filters[s_idx + 1] + roi_pos) -
							 *(scaled_shape_in_each_filters[s_idx + 2] + roi_pos) +
							 *(scaled_shape_in_each_filters[s_idx + 3] + roi_pos)) * weights_in_each_filters[w_idx];
	filtervalue += (*(scaled_shape_in_each_filters[s_idx + 4] + roi_pos) -
			 	 	 	 *(scaled_shape_in_each_filters[s_idx + 5] + roi_pos) -
						 *(scaled_shape_in_each_filters[s_idx + 6] + roi_pos) +
						 *(scaled_shape_in_each_filters[s_idx + 7] + roi_pos)) * weights_in_each_filters[w_idx + 1];
	if(scaled_shape_in_each_filters[s_idx + 8] != NULL){
		filtervalue += (*(scaled_shape_in_each_filters[s_idx + 8] + roi_pos) -
			 	 	 	 	 *(scaled_shape_in_each_filters[s_idx + 9] + roi_pos) -
							 *(scaled_shape_in_each_filters[s_idx + 10] + roi_pos) +
							 *(scaled_shape_in_each_filters[s_idx + 11] + roi_pos)) * weights_in_each_filters[w_idx + 2];
	}
	if(filtervalue >= filterthreshold){
		return alpha2_for_each_filters[f_idx];
	}
	else{
		return alpha1_for_each_filters[f_idx];
	}
}

bool cascade_classify(const unsigned int roi_standard_deviation, const unsigned int roi_position){
	int filter_index = 0;
	int shape_index = 0;
	int weight_index = 0;
	int stage_sum;
	int k, l;
	/* note again: there are 25 stages in cascade classifier */
	for(k = CLASSIFY_START_STAGE; k < FD_TOTAL_STAGES; k++){
		stage_sum = 0;
		/* do weak classifier combination (in each stages) */
		for(l = 0; l < filters_in_each_stages[k]; l++){
			stage_sum += haar_filters_commitee(roi_standard_deviation, roi_position, filter_index, shape_index, weight_index);
			filter_index++;
			shape_index += 12;
			weight_index += 3;
		}
		//printf("stage_sum: %d\n", stage_sum);
		if(stage_sum < 0.4 * threshold_for_each_stages[k]){
			return false;
		}
	}
	return true;
}

bool rects_predicate(const rect_u16_lincoln* r1, const rect_u16_lincoln* r2){
	unsigned short minw = (r1->w < r2->w) ? r1->w : r2->w;
	unsigned short minh = (r1->h < r2->h) ? r1->h : r2->h;
	float delta = GROUP_EPS * (minw + minh) * 0.5;
	return abs(r1->x - r2->x) <= delta && abs(r1->y - r2->y) <= delta &&
		   abs(r1->x + r1->w - r2->x - r2->w) <= delta && abs(r1->x + r1->h - r2->x - r2->h) <= delta;
}

short rects_partition(vector_lincoln* labels_ptr){
	short rects_num = fd_result_vec->num_elems;
	const rect_u16_lincoln* rects_ptr = (rect_u16_lincoln*)(fd_result_vec->beginning);
	const unsigned char PARENT = 0;
	const unsigned char RANK = 1;
	vector_lincoln _nodes;
	vector_init_lincoln(&_nodes, sizeof(short), 2 * rects_num);
	short (*nodes)[2] = (short (*)[2])_nodes.beginning;
	short i, j;

	/* The first O(N) pass: create N single-vertex trees */
	for(i = 0; i < rects_num; i++){
		nodes[i][PARENT] = -1;
		nodes[i][RANK] = 0;
	}
	// The main O(N^2) pass: merge connected components
	for(i = 0; i < rects_num; i++){
		short root = i;

		/* find root */
		while(nodes[root][PARENT] >= 0){
			root = nodes[root][PARENT];
		}
		for(j = 0; j < rects_num; j++){
			if(i == j || rects_predicate(&rects_ptr[i], &rects_ptr[j]) == false){
				continue;
			}
			short root2 = j;

			while(nodes[root][PARENT] >= 0){
				root = nodes[root][PARENT];
			}
			if(root2 != root) {
				// unite both trees
				short rank = nodes[root][RANK];
				short rank2 = nodes[root2][RANK];
				if (rank > rank2) {
					nodes[root2][PARENT] = root;
				} else {
					nodes[root][PARENT] = root2;
					nodes[root2][RANK] += (rank == rank2);
					root = root2;
				}
				if (nodes[root][PARENT] >= 0) {
					return 0;
				}
				short k = j, parent;
				// compress the path from node2 to root
				while ((parent = nodes[k][PARENT]) >= 0) {
					nodes[k][PARENT] = root;
					k = parent;
				}
				// compress the path from node to root
				k = i;
				while ((parent = nodes[k][PARENT]) >= 0) {
					nodes[k][PARENT] = root;
					k = parent;
				}
			}
		}
	}
	// Final O(N) pass: enumerate classes
	short nclasses = 0;
	short tmp = 0;
	for(i = 0; i < rects_num; i++){
		short root = i;
		while (nodes[root][PARENT] >= 0) {
			root = nodes[root][PARENT];
		}
		// re-use the rank as the class label
		if (nodes[root][RANK] >= 0) {
			nodes[root][RANK] = ~nclasses++;
		}
		tmp = ~nodes[root][RANK];
		vector_push_lincoln(labels_ptr, &tmp);
	}
	return nclasses;
}

void group_rects(){
	vector_lincoln labels;
	short nlabels = fd_result_vec->num_elems;
	vector_init_lincoln(&labels, sizeof(short), nlabels);
	short nclasses = rects_partition(&labels);
	vector_lincoln rrects;
	vector_init_lincoln(&rrects, sizeof(rect_u16_lincoln), nclasses);
	vector_lincoln rweights;
	vector_init_lincoln(&rweights, sizeof(short), nclasses);
	short i, j;

	for(i = 0; i < nlabels; i++){
		short cls = ((short*)labels.beginning)[i];
		((rect_u16_lincoln*)rrects.beginning)[cls].x += ((rect_u16_lincoln*)fd_result_vec->beginning)[i].x;
		((rect_u16_lincoln*)rrects.beginning)[cls].y += ((rect_u16_lincoln*)fd_result_vec->beginning)[i].y;
		((rect_u16_lincoln*)rrects.beginning)[cls].w += ((rect_u16_lincoln*)fd_result_vec->beginning)[i].w;
		((rect_u16_lincoln*)rrects.beginning)[cls].h += ((rect_u16_lincoln*)fd_result_vec->beginning)[i].h;
		((short*)rweights.beginning)[cls]++;
	}
	for(i = 0; i < nclasses; i++){
		rect_u16_lincoln r = ((rect_u16_lincoln*)rrects.beginning)[i];
		float s = 1.0f / ((short*)rweights.beginning)[i];
		((rect_u16_lincoln*)rrects.beginning)[i].x = u16_round_lincoln(r.x * s);
		((rect_u16_lincoln*)rrects.beginning)[i].y = u16_round_lincoln(r.y * s);
		((rect_u16_lincoln*)rrects.beginning)[i].w = u16_round_lincoln(r.w * s);
		((rect_u16_lincoln*)rrects.beginning)[i].h = u16_round_lincoln(r.h * s);
	}
	vector_clear_lincoln(fd_result_vec);

	for(i = 0; i < nclasses; i++){
		rect_u16_lincoln r1 = ((rect_u16_lincoln*)rrects.beginning)[i];
		short n1 = ((short*)rweights.beginning)[i];
		if(n1 <= GROUP_THRESHOLD){
			continue;
		}
		/* filter out small face rectangles inside large rectangles */
		for(j = 0; j < nclasses; j++){
			short n2 = ((short*)rweights.beginning)[j];
			if(j == i || n2 <= GROUP_THRESHOLD){
				continue;
			}
			rect_u16_lincoln r2 = ((rect_u16_lincoln*)rrects.beginning)[j];
			short dx = u16_round_lincoln(r2.w * GROUP_EPS);
			short dy = u16_round_lincoln(r2.h * GROUP_EPS);
			if (i != j && r1.x >= r2.x - dx && r1.y >= r2.y - dy &&
			    r1.x + r1.w <= r2.x + r2.w + dx &&
			    r1.y + r1.h <= r2.y + r2.h + dy &&
			    (n2 > (3 > n1 ? 3 : n1) || n1 < 3)){
				break;
			}
		}
		if(j == nclasses){
			vector_push_lincoln(fd_result_vec, &r1);
		}
	}
}

vector_lincoln* face_detect(const unsigned char* src){
	/* clear buffer first */
	vector_clear_lincoln(fd_result_vec);

	float scale_size_factor;
	imsize_u16_lincoln scaled_img_size;

	/* iterate scaling (making image pyramid) */
	for(scale_size_factor = FD_START_SCALE; ; scale_size_factor *= FD_SCALE_FACTOR){
		scaled_img_size.width = (unsigned short)(IMG_WIDTH / scale_size_factor);
		scaled_img_size.height = (unsigned short)(IMG_HEIGHT / scale_size_factor);
		/* iterate stop when source size is scaled under 24 */
		if(scaled_img_size.width < VJ_EMPIRICAL_DETECTION_ROI || scaled_img_size.height < VJ_EMPIRICAL_DETECTION_ROI){
			break;
		}
		/* resizing source using nearest neighbor interpolation (todo: may change to bilinear here) */
		resize_interpolation(src, NULL, scaled_img_size, scaled_img);
		create_integral_and_sqintegral_image(scaled_img, scaled_img_size, integral_img, sqintegral_img);

		unsigned int* roi_corners_in_intimg[4];
		unsigned int* roi_corners_in_sqintimg[4];

		set_scaled_buffers_and_roi(&roi_corners_in_intimg, &roi_corners_in_sqintimg, scaled_img_size.width);

		unsigned short roi_moving_range_w = scaled_img_size.width - VJ_EMPIRICAL_DETECTION_ROI + 1;
		unsigned short roi_moving_range_h = scaled_img_size.height - VJ_EMPIRICAL_DETECTION_ROI + 1;
		unsigned short i, j;
		unsigned int roi_position;

		/* V-J ROI iterate through scaled image*/
		for(j = 0; j < roi_moving_range_h; j += DETECTION_ROI_MOVING_STEP){
			for(i = 0; i < roi_moving_range_w; i += DETECTION_ROI_MOVING_STEP){
				roi_position = j * scaled_img_size.width + i;
				unsigned int roi_sqsum = roi_corners_in_sqintimg[3][roi_position] - roi_corners_in_sqintimg[2][roi_position] -
												 roi_corners_in_sqintimg[1][roi_position] + roi_corners_in_sqintimg[0][roi_position];
				unsigned int roi_sum = roi_corners_in_intimg[3][roi_position] - roi_corners_in_intimg[2][roi_position] -
											  roi_corners_in_intimg[1][roi_position] + roi_corners_in_intimg[0][roi_position];
				int roi_variance = roi_sqsum * VJ_EMPIRICAL_DETECTION_ROI_AREA - roi_sum * roi_sum;
				unsigned int roi_standard_deviation = (roi_variance > 0) ? int_sqrt(roi_variance) : 1;

				bool is_get_face_candidate = cascade_classify(roi_standard_deviation, roi_position);
				if(is_get_face_candidate){
					unsigned short x_candidate = u16_round_lincoln(i * scale_size_factor);
					unsigned short y_candidate = u16_round_lincoln(j * scale_size_factor);
					unsigned short w_h_candidate = u16_round_lincoln(VJ_EMPIRICAL_DETECTION_ROI * scale_size_factor);
					rect_u16_lincoln face_candidate = {x_candidate, y_candidate, w_h_candidate, w_h_candidate};
					vector_push_lincoln(fd_result_vec, &face_candidate);
				}

			}
		}
	}
	if(fd_result_vec->num_elems > 0){
		group_rects();
	}
	return fd_result_vec;
}


