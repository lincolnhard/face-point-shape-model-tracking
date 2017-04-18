/*
 * video_capture.c
 *
 *  Created on: Dec 24, 2015
 *      Author: Lincoln
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <fcntl.h> /* low-level i/o control (open)*/
#include <errno.h>
#include <string.h> /* strerror show errno meaning */
#include <sys/stat.h> /* getting information about files attributes */
#include <linux/videodev2.h> /* v4l2 structure */
#include <sys/mman.h> /* memory mapping */
#include <unistd.h> /* read write close */
#include <assert.h>
#include <sys/time.h> /* for select time */

#define CLEAR(x) memset (&(x), 0, sizeof (x))
static char* dev_name = "/dev/video0";
static int fd = -1; /* vidoe0 file descriptor*/
struct buffer {
	void * start;
	size_t length;
};
/* Queried image buffers! */
struct buffer* buffers = NULL;
static unsigned int n_buffers = 0;
struct v4l2_buffer buf_in_while_loop;
static fd_set fds;
struct timeval tv;

/* wrapped errno display function by v4l2 API */
static void errno_exit(const char * s) {
	fprintf(stderr, "%s error %d, %s\n", s, errno, strerror(errno));
	exit(EXIT_FAILURE);
}

/* wrapped ioctrl function by v4l2 API */
static int xioctl(int fd, int request, void * arg) {
	int r;
	do{
		r = ioctl(fd, request, arg);
	}
	while(-1 == r && EINTR == errno);
	return r;
}

static void open_device() {
	struct stat st;
	if (-1 == stat(dev_name, &st)) {
		fprintf(stderr, "Cannot identify '%s': %d, %s\n", dev_name, errno, strerror(errno));
		exit(EXIT_FAILURE);
	}
	if (!S_ISCHR(st.st_mode)) {
		fprintf(stderr, "%s is no device\n", dev_name);
		exit(EXIT_FAILURE);
	}
	fd = open(dev_name, O_RDWR | O_NONBLOCK, 0);
	if (-1 == fd) {
		fprintf(stderr, "Cannot open '%s': %d, %s\n", dev_name, errno, strerror(errno));
		exit(EXIT_FAILURE);
	}
}

/*
 * 1. Memory mapped buffers are located in device memory and must be allocated with this ioctl
 * 	  before they can be mapped into the application's address space
 * 2. set four images in buffer
 */
static void init_mmap() {
	struct v4l2_requestbuffers req;
	CLEAR(req);
	req.count = 4;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;
	/* Initiate Memory Mapping */
	if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req)) {
		if (EINVAL == errno) {
			fprintf(stderr, "%s does not support memory mapping\n", dev_name);
			exit(EXIT_FAILURE);
		} else {
			errno_exit("VIDIOC_REQBUFS");
		}
	}
	/* video output requires at least two buffers, one displayed and one filled by the application */
	if (req.count < 2) {
		fprintf(stderr, "Insufficient buffer memory on %s\n", dev_name);
		exit(EXIT_FAILURE);
	}
	buffers = calloc(req.count, sizeof(*buffers));

	if (!buffers) {
		fprintf(stderr, "Out of memory\n");
		exit(EXIT_FAILURE);
	}
	for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
		struct v4l2_buffer buf;
		CLEAR(buf);
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = n_buffers;
		/* Query the status of a buffer */
		if (-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf)){
			errno_exit("VIDIOC_QUERYBUF");
		}
		buffers[n_buffers].length = buf.length; //640 * 480 * 2 = 614400
		buffers[n_buffers].start = mmap(NULL /* start anywhere */, buf.length,
		PROT_READ | PROT_WRITE /* required */,
		MAP_SHARED /* recommended */, fd, buf.m.offset);

		if (MAP_FAILED == buffers[n_buffers].start){
			errno_exit("mmap");
		}
	}
}

/*
 * set video streaming format here
 * 640*480
 * YUYV
 * no cropping no scaling
 */
static void init_device() {
	struct v4l2_capability cap;
	struct v4l2_format fmt;
	unsigned int min;
	/* Query device capabilities */
	if (-1 == xioctl(fd, VIDIOC_QUERYCAP, &cap)) {
		if (EINVAL == errno) {
			fprintf(stderr, "%s is no V4L2 device\n", dev_name);
			exit(EXIT_FAILURE);
		} else {
			errno_exit("VIDIOC_QUERYCAP");
		}
	}
	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		fprintf(stderr, "%s is no video capture device\n", dev_name);
		exit(EXIT_FAILURE);
	}
	if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
		fprintf(stderr, "%s does not support streaming i/o\n", dev_name);
		exit(EXIT_FAILURE);
	}

#if 0
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;
	/* Select video input, video standard and tune here. */
	CLEAR(cropcap);
	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	/* Information about the video cropping and scaling abilities */
	if (0 == xioctl(fd, VIDIOC_CROPCAP, &cropcap)) {
		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		crop.c = cropcap.defrect; /* reset to default rect */
	} else {
		/* Errors ignored. */
	}
#endif

	CLEAR(fmt);
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width = 640;
	fmt.fmt.pix.height = 480;
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
	fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
	if (-1 == xioctl(fd, VIDIOC_S_FMT, &fmt)){
		errno_exit("VIDIOC_S_FMT");
	}
	/* Note VIDIOC_S_FMT may change width and height. */
	/* Buggy driver paranoia. */
	/* YUYV sampling 4 2 2, so bytes per pixel is 2*/
	min = fmt.fmt.pix.width * 2;
	if (fmt.fmt.pix.bytesperline < min){
		fmt.fmt.pix.bytesperline = min;
	}
	min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
	if (fmt.fmt.pix.sizeimage < min){
		fmt.fmt.pix.sizeimage = min;
	}
	init_mmap();
}

static void start_capturing() {
	unsigned int i;
	enum v4l2_buf_type type;
	for (i = 0; i < n_buffers; ++i) {
		struct v4l2_buffer buf;
		CLEAR(buf);
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;
		/* enqueue an empty (capturing) or filled (output) buffer in the driver's incoming queue */
		if (-1 == xioctl(fd, VIDIOC_QBUF, &buf)){
			errno_exit("VIDIOC_QBUF");
		}
	}
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	/* Start streaming I/O */
	if (-1 == xioctl(fd, VIDIOC_STREAMON, &type)){
		errno_exit("VIDIOC_STREAMON");
	}
}

static void close_device() {
	if (-1 == close(fd))
		errno_exit("close");
	fd = -1;
}

static void stop_capturing() {
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (-1 == xioctl(fd, VIDIOC_STREAMOFF, &type)){
		errno_exit("VIDIOC_STREAMOFF");
	}
}

static void uninit_device() {
	unsigned int i;
	for (i = 0; i < n_buffers; ++i){
		if (-1 == munmap(buffers[i].start, buffers[i].length)){
			errno_exit("munmap");
		}
	}
	free(buffers);
}

void init_video_capture(){
	open_device();
	init_device();
	start_capturing();
}
#if 0
void video_capture(unsigned char* dst, const int dstlen){
	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	/* Timeout. */
	tv.tv_sec = 0;
	tv.tv_usec = 500000;
	if(select(fd + 1, &fds, NULL, NULL, &tv) < 1){
		puts("no buf!!!");
		return;
	}

	struct v4l2_buffer buf;
	CLEAR(buf);
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;
	/* dequeue from buffer */
	if(-1 == xioctl(fd, VIDIOC_DQBUF, &buf)){
		switch(errno){
		case EAGAIN:
			return;
		default:
			errno_exit("VIDIOC_DQBUF");
		}
	}
	assert(buf.index < n_buffers);
	/* give Y channel to destination pointer */
	unsigned char* im_from_cam = (unsigned char*)buffers[buf.index].start;
	int i;
	for(i = 0; i < dstlen; i++){
		dst[i] = im_from_cam[2 * i];
	}
	/* queue in buffer */
	if(-1 == xioctl(fd, VIDIOC_QBUF, &buf)){
		errno_exit("VIDIOC_QBUF");
	}
}
#else
char video_capture(unsigned char* dst, const int dstlen){
	char key[2] = {0};
	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	FD_SET(fileno(stdin), &fds);
	/* Timeout. */
	tv.tv_sec = 0;
	tv.tv_usec = 1000;
	select(fd + 1, &fds, NULL, NULL, &tv);

	if(FD_ISSET(fileno(stdin), &fds)){
		read(fileno(stdin), &key, 2);
		printf("key: %c\n", key[0]);
		return key[0];
	}

	if(FD_ISSET(fd, &fds)){
		CLEAR(buf_in_while_loop);
		buf_in_while_loop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf_in_while_loop.memory = V4L2_MEMORY_MMAP;
		/* dequeue from buffer */
		if(-1 == xioctl(fd, VIDIOC_DQBUF, &buf_in_while_loop)){
			switch(errno){
			case EAGAIN:
				return 0;
			default:
				errno_exit("VIDIOC_DQBUF");
			}
		}
		assert(buf_in_while_loop.index < n_buffers);
		/* give Y channel to destination pointer */
		unsigned char* im_from_cam = (unsigned char*)buffers[buf_in_while_loop.index].start;
		int i;
		for(i = 0; i < dstlen; i++){
			dst[i] = im_from_cam[2 * i];
		}
		/* queue-in buffer */
		if(-1 == xioctl(fd, VIDIOC_QBUF, &buf_in_while_loop)){
			errno_exit("VIDIOC_QBUF");
		}
	}
	return 0;
}
#endif
void free_video_capture(){
	stop_capturing();
	uninit_device();
	close_device();
}
