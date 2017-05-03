/*
 ============================================================================
 Name        : main.c
 Author      : LincolnHard
 Version     :
 Copyright   : free and open
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
using namespace cv;

#include "ft.h"

int main
    (
    void
    )
{
    Mat im;
    Mat gray;
    VideoCapture cap;
    cap.open(0);
    if (!cap.isOpened())
        {
        printf("Failed to open cam\n");
        exit(EXIT_FAILURE);
        }
    cap.set(CV_CAP_PROP_FRAME_WIDTH, IMG_WIDTH);
    cap.set(CV_CAP_PROP_FRAME_HEIGHT, IMG_HEIGHT);

	init_face_tracker();
	while(1)
        {
        restore_memory_pool_lincoln();

        cap >> im;
        cvtColor(im, gray, CV_BGR2GRAY);

		face_track(gray.data);
		draw_ft_result(gray.data);

        imshow("demo", gray);
        if (waitKey(10) == 27) //esc
            {
			break;
		    }
	    }

    free_face_tracker();
	return EXIT_SUCCESS;
}
