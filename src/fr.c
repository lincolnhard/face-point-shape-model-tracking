/*
 * fr.c
 *
 *  Created on: 2016/1/4
 *      Author: lincoln
 */
#include "fr.h"
#include "float.h"
#include "svm.h"

/* the pre-trained PCA data*/
extern float average_train_image[TRAIN_IMG_TOTAL_PIXEL];
extern float eigen_vector_array[EIGEN_VECTOR_NUM][TRAIN_IMG_TOTAL_PIXEL];
extern float projected_sample_coeffs[TOTAL_FACES][EIGEN_VECTOR_NUM];

unsigned char* face_img;
float* current_average_im_subtract;
float* current_projected_coeffs;
struct svm_model* svmtrainedmodel;
struct svm_node* current_svm_node;

static int get_min_facespace_distance(float* current_face_coeffs){
	int i, j;
	double min_dist = DBL_MAX;
	int match_face_no = 0;
	double sqdist[TOTAL_FACES] = {0};
	for(i = 0; i < TOTAL_FACES; i++){
		for(j = 0; j < EIGEN_VECTOR_NUM; j++){
			float dist = current_face_coeffs[j] - projected_sample_coeffs[i][j];
			sqdist[i] += dist * dist;
		}
	}
#if 0
	/* recognition combined with group faces */
	double distsum[PEOPLE_IN_DATABASE] = {0};
	for(i = 0; i < PEOPLE_IN_DATABASE; i++){
		for(j = 0; j < EACH_PERSON_PIC_NUM; j++){
			distsum[i] += sqdist[i * EACH_PERSON_PIC_NUM + j];
		}
		if(distsum[i] < min_dist){
			min_dist = distsum[i];
			match_face_no = i;
		}
	}
#else
	/* recognition with simple compare to all image */
	for(i = 0; i < TOTAL_FACES; i++){
		if(sqdist[i] < min_dist){
			min_dist = sqdist[i];
			match_face_no = i;
		}
	}
#endif

	printf("min_dist: %.3f\n", min_dist);
	/* compare with threshold */
	if(min_dist > 330000.0){
		/* recognition failed (no such face in database) */
		return -1;
	}
	else{
		/* recognition complete and return the index */
		return match_face_no;
	}
}

int face_recognize(const unsigned char* src, const rect_u16_lincoln facerect){
	const imsize_u16_lincoln frsize = {TRAIN_IMG_SIZE, TRAIN_IMG_SIZE};
	resize_interpolation(src, &facerect, frsize, face_img);

	/* for debugging */
	//write_pgm("faceroi.pgm", face_img, frsize, NULL);

	/* eigen decomposite */
	int i, j;
	for(i = 0; i < TRAIN_IMG_TOTAL_PIXEL; i++){
		current_average_im_subtract[i] = face_img[i] - average_train_image[i];
	}
	for(i = 0; i < EIGEN_VECTOR_NUM; i++){
		float accutemp = 0.0f;
		for(j = 0; j < TRAIN_IMG_TOTAL_PIXEL; j++){
			accutemp += current_average_im_subtract[j] * eigen_vector_array[i][j];
		}
		current_projected_coeffs[i] = accutemp;
	}

	for(i = 0; i < EIGEN_VECTOR_NUM; i++){
		current_svm_node[i].value = (double)current_projected_coeffs[i];
	}

	double predict_label = svm_predict(svmtrainedmodel, current_svm_node);
	printf("predict_label = %f\n", predict_label);

	int recogresult = get_min_facespace_distance(current_projected_coeffs);

	return recogresult;
}

void init_face_recognition(const char* filename){
	face_img = (unsigned char*)malloc_lincoln(TRAIN_IMG_TOTAL_PIXEL * sizeof(unsigned char));
	current_average_im_subtract = (float*)malloc_lincoln(TRAIN_IMG_TOTAL_PIXEL * sizeof(float));
	current_projected_coeffs = (float*)malloc_lincoln(EIGEN_VECTOR_NUM * sizeof(float));
	svmtrainedmodel = svm_load_model(filename);
	if(svmtrainedmodel == NULL){
		puts("can't open svm model file");
	}
	current_svm_node = (struct svm_node*)malloc_lincoln((EIGEN_VECTOR_NUM + 1) * sizeof(struct svm_node));
	/* initialize svm node */
	int i;
	for(i = 0; i < EIGEN_VECTOR_NUM; i++){
		current_svm_node[i].index = i + 1;
	}
	current_svm_node[EIGEN_VECTOR_NUM].index = -1;
}

void free_face_recognition(){
	svm_free_and_destroy_model(&svmtrainedmodel);
}
