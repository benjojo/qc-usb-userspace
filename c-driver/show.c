/*****************************************************************************
 *
 *  show.c - Logitech QuickCam Express Viewer / Debug utility
 *
 *  Copyright (C) 2000 Georg Acher (acher@in.tum.de)
 *  Copyright (C) 2000 Mark Cave-Ayland (mca198@ecs.soton.ac.uk)
 *  Copyright (C) 2002 Tuukka Toivonen (tuukkat@ee.oulu.fi)
 *
 *  Requires quickcam-module (with V4L interface) and USB support!
 *
 *****************************************************************************/

/* Note: this program is meant only for debugging. It contains very ugly code */

/* Streamformat, as delivered by the camera driver:
   Raw image data for Bayer-RGB-matrix:
   G R for even rows
   B G for odd rows
*/

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <X11/extensions/XShm.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <inttypes.h>
#include <unistd.h>

#define DUMPDATA 0		/* If using this, set DUMPDATA also in quickcam.h */

#define NOKERNEL
#define COMPRESS 1
#define PARANOID 1
#define PDEBUG(fmt, args...)	do { printf("quickcam: " fmt, ## args); printf("\n"); } while(0)
#define POISON_VAL 0x00
#define POISON(x)
//#define PDEBUG(fmt, args...)
#define kmalloc(size, gfp)	malloc(size)
#define kfree(ptr)		free(ptr)
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int16_t s16;
typedef int32_t s32;
#define FALSE 0
#define TRUE (!FALSE)
#define MAX(a,b)	((a)>(b)?(a):(b))
#define MIN(a,b)	((a)<(b)?(a):(b))
#define CLIP(a,low,high) MAX((low),MIN((high),(a)))
#define BIT(x)		(1<<(x))
struct qc_mjpeg_data {
	int depth;		/* Bits per pixel in the final RGB image: 16 or 24 */
	u8 *encV;		/* Temporary buffers for holding YUV image data */
	u8 *encU;
	u8 *encY;
	/* yuv2rgb private data */
	void *table;
	void *table_rV[256];
	void *table_gU[256];
	int table_gV[256];
	void *table_bU[256];
	void (*yuv2rgb_func)(struct qc_mjpeg_data *, u8 *, u8 *, u8 *, u8 *, void *, void *, int);
};
int qcdebug = 0;
#define IDEBUG_INIT(x)
#define IDEBUG_EXIT(x)
#define IDEBUG_TEST(x)
#define DEBUGLOGIC 1
#define PRINTK(...)
#include "qc-mjpeg.c"

#include "videodev2.h"
#include "quickcam.h"

#define max(a,b) ((a)>(b)?(a):(b))
#define min(a,b) ((a)<(b)?(a):(b))

GC      gc;
GC      gc1,gcn;
Window  win;
Font font;
Display *dpy=0;
int     screen;
Pixmap  buffer;
int scrnW2,scrnH2;
unsigned short flags = 0;
char *text;
XColor colors[16];
XImage* img;
char *s;
XShmSegmentInfo shminfo;
int depth;
int bypp;

// Other stuff

unsigned char SENSOR_ADDR;
char buf[32768];
int po;
int shutter_val=0x80;
int gain=0xc0;
int white_balance=0;

static inline unsigned long long rdtsc(void)
{
	unsigned int lo, hi;

	asm volatile (
		"rdtsc\n"
		: "=a" (lo), "=d" (hi)
	);

	return ((unsigned long long)(hi) << 32) | (lo);
}

int tulos;

/* Supposedly fast division routine: according to my benchmarking, slow, don't use */
static inline unsigned int fastdiv(unsigned int dividend, unsigned int divisor) {
	unsigned int result;
	asm const(
		"shrl $16,%%edx\n"
		"\tdivw %3\n" 
		: "=a" (result)		/* %0 Outputs */
		: "a" (dividend), 	/* %1 Inputs, divident to both EAX and EDX */
		  "d" (dividend), 	/* %2 Inputs */
		  "q" (divisor) );	/* %3 */
	return result;
}

unsigned int testi(unsigned int dividend, unsigned int divisor) {
	tulos = fastdiv(dividend,divisor);
	return tulos;
}

#define LUT_RED   0
#define LUT_GREEN 256
#define LUT_BLUE  512
#define LUT_SIZE  (3*256)

static unsigned char lut[LUT_SIZE];

void bayer_to_rgb_noip_asm(unsigned char *bay, int bay_line,
		   unsigned char *rgb, int rgb_line,
		   int columns, int rows, int bpp);

/* Bayer image size must be even horizontally and vertically */
/* 10834 clocks */
static unsigned char bayer_midvalue(unsigned char *bay, int bay_line, 
	unsigned int columns, unsigned int rows)
{
	static const unsigned int stepsize = 8;
	unsigned char *cur_bay;
	unsigned int sum = 0;
	int column_cnt;
	int row_cnt;
	
	columns /= stepsize*2;
	rows    /= stepsize*2;
	
	row_cnt = rows;
	do {
		column_cnt = columns;
		cur_bay = bay;
		do {
			sum += cur_bay[0] + cur_bay[1] + cur_bay[bay_line];	/* R + G + B */
			cur_bay += 2*stepsize;
		} while (--column_cnt > 0);
		bay += 2*stepsize*bay_line;
	} while (--row_cnt > 0);
	sum /= 3 * columns * rows;
	return sum;
}

/* 89422 clocks */
static void bayer_equalize(unsigned char *bay, int bay_line, 
	unsigned int columns, unsigned int rows, unsigned char *lut)
{
	static const unsigned int stepsize = 4;
	unsigned short red_cnt[256], green_cnt[256], blue_cnt[256];
	unsigned int red_sum, green_sum, blue_sum;
	unsigned int red_tot, green_tot, blue_tot;
	unsigned char *cur_bay;
	int i, column_cnt, row_cnt;
	
	memset(red_cnt,   0, sizeof(red_cnt));
	memset(green_cnt, 0, sizeof(green_cnt));
	memset(blue_cnt,  0, sizeof(blue_cnt));
	
	columns /= 2*stepsize;
	rows    /= 2*stepsize;

	/* Compute histogram */
	row_cnt = rows;
	do {
		column_cnt = columns;
		cur_bay = bay;
		do {
			green_cnt[cur_bay[0]]++;
			red_cnt  [cur_bay[1]]++;
			blue_cnt [cur_bay[bay_line]]++;
			green_cnt[cur_bay[bay_line+1]]++;
			cur_bay += 2*stepsize;
		} while (--column_cnt > 0);
		bay += 2*stepsize*bay_line;
	} while (--row_cnt > 0);

	/* Compute lookup table based on the histogram */
	red_tot   = columns * rows;		/* Total number of pixels of each primary color */
	green_tot = red_tot * 2;
	blue_tot  = red_tot;
	red_sum   = 0;
	green_sum = 0;
	blue_sum  = 0;
	for (i=0; i<256; i++) {
		lut[LUT_RED   + i] = 255 * red_sum   / red_tot;
		lut[LUT_GREEN + i] = 255 * green_sum / green_tot;
		lut[LUT_BLUE  + i] = 255 * blue_sum  / blue_tot;
		red_sum   += red_cnt[i];
		green_sum += green_cnt[i];
		blue_sum  += blue_cnt[i];
	}
}

/* Apply the look-up table to the Bayer image data */
/* 391721-437413 PII, 295471 PIII */
static void bayer_lut(unsigned char *bay, int bay_line, 
	unsigned int columns, unsigned int rows, unsigned char *lut)
{
	unsigned char *cur_bay;
	unsigned int total_columns;
	
	total_columns = columns / 2;	/* Number of 2x2 bayer blocks */
	rows /= 2;
	do {
		columns = total_columns;
		cur_bay = bay;
		do {
			cur_bay[0] = lut[LUT_GREEN + cur_bay[0]];
			cur_bay[1] = lut[LUT_RED   + cur_bay[1]];
			cur_bay += 2;
		} while (--columns);
		bay += bay_line;
		columns = total_columns;
		cur_bay = bay;
		do {
			cur_bay[0] = lut[LUT_BLUE  + cur_bay[0]];
			cur_bay[1] = lut[LUT_GREEN + cur_bay[1]];
			cur_bay += 2;
		} while (--columns);
		bay += bay_line;
	} while (--rows);
}


/*
 * Convert Bayer image data to RGB, 24 bpp
 *
 * Camera is G1R1 G2R2 G3R3...
 *           B4G4 B5G5 B6G6...
 * Video  is B4G1R1 B4G1R1
 *           B4G4R1 B4G4R1

*/

static inline void write_rgb(void *addr, int bpp,
	unsigned char r, unsigned char g, unsigned char b)
{
	if (bpp==4) {	/* Negate to disabled */
		*(unsigned int *)addr = 
			(unsigned int)r << 16 | 
			(unsigned int)g << 8 |
			(unsigned int)b;
	} else {
		unsigned char *addr2 = (unsigned char *)addr;
		addr2[0] = b;
		addr2[1] = g;
		addr2[2] = r;
	}
}

/* With interpolation: 9367700-9491271 clocks */
/* Without interpolation: 8767177-8930714 */
/* static void quickcam_parse_data(struct usb_quickcam *quickcam, int curframe)*/
/* Original routine from quickcam.c */

/* No interpolation */
/* 2391747-2653574 clocks */
static inline void bayer_to_rgb_noip(unsigned char *bay, int bay_line,
		   unsigned char *rgb, int rgb_line,
		   int columns, int rows, int bpp)
{
	unsigned char *cur_bay, *cur_rgb;
	int bay_line2, rgb_line2;
	int total_columns;

	/* Process 2 lines and rows per each iteration */
	total_columns = columns >> 1;
	rows >>= 1;
	bay_line2 = 2*bay_line;
	rgb_line2 = 2*rgb_line;
	do {
		cur_bay = bay;
		cur_rgb = rgb;
		columns = total_columns;
		do {
			write_rgb(cur_rgb+0,            bpp, cur_bay[1], cur_bay[0],          cur_bay[bay_line]);
			write_rgb(cur_rgb+bpp,          bpp, cur_bay[1], cur_bay[0],          cur_bay[bay_line]);
			write_rgb(cur_rgb+rgb_line,     bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
			write_rgb(cur_rgb+rgb_line+bpp, bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
			cur_bay += 2;
			cur_rgb += 2*bpp;
		} while (--columns);
		bay += bay_line2;
		rgb += rgb_line2;
	} while (--rows);
}		  

/* Interpolate R,G,B horizontally. columns must be even */
static inline void bayer_to_rgb_horip(unsigned char *bay, int bay_line,
		  unsigned char *rgb, int rgb_line,
		  unsigned int columns, unsigned int rows, int bpp) 
{
	unsigned char *cur_bay, *cur_rgb;
	int bay_line2, rgb_line2;
	int total_columns;
	unsigned char red, green, blue;
	unsigned int column_cnt, row_cnt;

	/* Process 2 lines and rows per each iteration */
	total_columns = (columns-2) / 2;
	row_cnt = rows / 2;
	bay_line2 = 2*bay_line;
	rgb_line2 = 2*rgb_line;
	
	do {
		write_rgb(rgb+0,        bpp, bay[1], bay[0], bay[bay_line]);
		write_rgb(rgb+rgb_line, bpp, bay[1], bay[0], bay[bay_line]);
		cur_bay = bay + 1;
		cur_rgb = rgb + bpp;
		column_cnt = total_columns;
		do {
			green = ((unsigned int)cur_bay[-1]+cur_bay[1]) / 2;
			blue  = ((unsigned int)cur_bay[bay_line-1]+cur_bay[bay_line+1]) / 2;
			write_rgb(cur_rgb+0, bpp, cur_bay[0], green, blue);
			red   = ((unsigned int)cur_bay[0]+cur_bay[2]) / 2;
			write_rgb(cur_rgb+bpp, bpp, red, cur_bay[1], cur_bay[bay_line+1]);
			green = ((unsigned int)cur_bay[bay_line]+cur_bay[bay_line+2]) / 2;
			write_rgb(cur_rgb+rgb_line, bpp, cur_bay[0], cur_bay[bay_line], blue);
			write_rgb(cur_rgb+rgb_line+bpp, bpp, red, cur_bay[1], cur_bay[bay_line+1]);
			cur_bay += 2;
			cur_rgb += 2*bpp;
		} while (--column_cnt);
		write_rgb(cur_rgb+0,        bpp, cur_bay[0], cur_bay[-1],       cur_bay[bay_line-1]);
		write_rgb(cur_rgb+rgb_line, bpp, cur_bay[0], cur_bay[bay_line], cur_bay[bay_line-1]);
		bay += bay_line2;
		rgb += rgb_line2;
	} while (--row_cnt);
}		  

/* Interpolate R,G,B fully. columns and rows must be even */
static inline void bayer_to_rgb_ip(unsigned char *bay, int bay_line,
		  unsigned char *rgb, int rgb_line,
		  unsigned int columns, unsigned int rows, int bpp) 
{
	unsigned char *cur_bay, *cur_rgb;
	int bay_line2, rgb_line2;
	int total_columns;
	unsigned char red, green, blue;
	unsigned int column_cnt, row_cnt;

	/* Process 2 lines and rows per each iteration */
	total_columns = (columns-2) / 2;
	row_cnt = (rows-2) / 2;
	bay_line2 = 2*bay_line;
	rgb_line2 = 2*rgb_line;

	/* First scanline is handled here as a special case */	
	write_rgb(rgb, bpp, bay[1], bay[0], bay[bay_line]);
	cur_bay = bay + 1;
	cur_rgb = rgb + bpp;
	column_cnt = total_columns;
	do {
		green  = ((unsigned int)cur_bay[-1] + cur_bay[1] + cur_bay[bay_line]) / 3;
		blue   = ((unsigned int)cur_bay[bay_line-1] + cur_bay[bay_line+1]) / 2;
		write_rgb(cur_rgb, bpp, cur_bay[0], green, blue);
		red    = ((unsigned int)cur_bay[0] + cur_bay[2]) / 2;
		write_rgb(cur_rgb+bpp, bpp, red, cur_bay[1], cur_bay[bay_line+1]);
		cur_bay += 2;
		cur_rgb += 2*bpp;
	} while (--column_cnt);
	green = ((unsigned int)cur_bay[-1] + cur_bay[bay_line]) / 2;
	write_rgb(cur_rgb, bpp, cur_bay[0], green, cur_bay[bay_line-1]);

	/* Process here all other scanlines except first and last */
	bay += bay_line;
	rgb += rgb_line;
	do {
		red = ((unsigned int)bay[-bay_line+1] + bay[bay_line+1]) / 2;
		green = ((unsigned int)bay[-bay_line] + bay[1] + bay[bay_line]) / 3;
		write_rgb(rgb+0, bpp, red, green, bay[0]);
		blue = ((unsigned int)bay[0] + bay[bay_line2]) / 2;
		write_rgb(rgb+rgb_line, bpp, bay[bay_line+1], bay[bay_line], blue);
		cur_bay = bay + 1;
		cur_rgb = rgb + bpp;
		column_cnt = total_columns;
		do {
			red   = ((unsigned int)cur_bay[-bay_line]+cur_bay[bay_line]) / 2;
			blue  = ((unsigned int)cur_bay[-1]+cur_bay[1]) / 2;
			write_rgb(cur_rgb+0, bpp, red, cur_bay[0], blue);
			red   = ((unsigned int)cur_bay[-bay_line]+cur_bay[-bay_line+2]+cur_bay[bay_line]+cur_bay[bay_line+2]) / 4;
			green = ((unsigned int)cur_bay[0]+cur_bay[2]+cur_bay[-bay_line+1]+cur_bay[bay_line+1]) / 4;
			write_rgb(cur_rgb+bpp, bpp, red, green, cur_bay[1]);
			green = ((unsigned int)cur_bay[0]+cur_bay[bay_line2]+cur_bay[bay_line-1]+cur_bay[bay_line+1]) / 4;
			blue  = ((unsigned int)cur_bay[-1]+cur_bay[1]+cur_bay[bay_line2-1]+cur_bay[bay_line2+1]) / 4;
			write_rgb(cur_rgb+rgb_line, bpp, cur_bay[bay_line], green, blue);
			red   = ((unsigned int)cur_bay[bay_line]+cur_bay[bay_line+2]) / 2;
			blue  = ((unsigned int)cur_bay[1]+cur_bay[bay_line2+1]) / 2;
			write_rgb(cur_rgb+rgb_line+bpp, bpp, red, cur_bay[bay_line+1], blue);
			cur_bay += 2;
			cur_rgb += 2*bpp;
		} while (--column_cnt);
		red = ((unsigned int)cur_bay[-bay_line] + cur_bay[bay_line]) / 2;
		write_rgb(cur_rgb, bpp, red, cur_bay[0], cur_bay[-1]);
		green = ((unsigned int)cur_bay[0] + cur_bay[bay_line-1] + cur_bay[bay_line2]) / 3;
		blue = ((unsigned int)cur_bay[-1] + cur_bay[bay_line2-1]) / 2;
		write_rgb(cur_rgb+rgb_line, bpp, cur_bay[bay_line], green, blue);
		bay += bay_line2;
		rgb += rgb_line2;
	} while (--row_cnt);

	/* Last scanline is handled here as a special case */	
	green = ((unsigned int)bay[-bay_line] + bay[1]) / 2;
	write_rgb(rgb, bpp, bay[-bay_line+1], green, bay[0]);
	cur_bay = bay + 1;
	cur_rgb = rgb + bpp;
	column_cnt = total_columns;
	do {
		blue   = ((unsigned int)cur_bay[-1] + cur_bay[1]) / 2;
		write_rgb(cur_rgb, bpp, cur_bay[-bay_line], cur_bay[0], blue);
		red    = ((unsigned int)cur_bay[-bay_line] + cur_bay[-bay_line+2]) / 2;
		green  = ((unsigned int)cur_bay[0] + cur_bay[-bay_line+1] + cur_bay[2]) / 3;
		write_rgb(cur_rgb+bpp, bpp, red, green, cur_bay[1]);
		cur_bay += 2;
		cur_rgb += 2*bpp;
	} while (--column_cnt);
	write_rgb(cur_rgb, bpp, cur_bay[-bay_line], cur_bay[0], cur_bay[-1]);
}		  


static inline void bayer_to_rgb_cott(unsigned char *bay, int bay_line,
		   unsigned char *rgb, int rgb_line,
		   int columns, int rows, int bpp)
{
	unsigned char *cur_bay, *cur_rgb;
	int bay_line2, rgb_line2;
	int total_columns;

	/* Process 2 lines and rows per each iteration, but process the last row and column separately */
	total_columns = (columns>>1) - 1;
	rows = (rows>>1) - 1;
	bay_line2 = 2*bay_line;
	rgb_line2 = 2*rgb_line;
	do {
		cur_bay = bay;
		cur_rgb = rgb;
		columns = total_columns;
		do {
			write_rgb(cur_rgb+0,            bpp, cur_bay[1],           ((unsigned int)cur_bay[0] + cur_bay[bay_line+1])          /2, cur_bay[bay_line]);
			write_rgb(cur_rgb+bpp,          bpp, cur_bay[1],           ((unsigned int)cur_bay[2] + cur_bay[bay_line+1])          /2, cur_bay[bay_line+2]);
			write_rgb(cur_rgb+rgb_line,     bpp, cur_bay[bay_line2+1], ((unsigned int)cur_bay[bay_line2] + cur_bay[bay_line+1])  /2, cur_bay[bay_line]);
			write_rgb(cur_rgb+rgb_line+bpp, bpp, cur_bay[bay_line2+1], ((unsigned int)cur_bay[bay_line2+2] + cur_bay[bay_line+1])/2, cur_bay[bay_line+2]);
			cur_bay += 2;
			cur_rgb += 2*bpp;
		} while (--columns);
		write_rgb(cur_rgb+0,            bpp, cur_bay[1], ((unsigned int)cur_bay[0] + cur_bay[bay_line+1])/2, cur_bay[bay_line]);
		write_rgb(cur_rgb+bpp,          bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
		write_rgb(cur_rgb+rgb_line,     bpp, cur_bay[bay_line2+1], ((unsigned int)cur_bay[bay_line2] + cur_bay[bay_line+1])/2, cur_bay[bay_line]);
		write_rgb(cur_rgb+rgb_line+bpp, bpp, cur_bay[bay_line2+1], cur_bay[bay_line+1], cur_bay[bay_line]);
		bay += bay_line2;
		rgb += rgb_line2;
	} while (--rows);
	/* Last scanline handled here as special case */
	cur_bay = bay;
	cur_rgb = rgb;
	columns = total_columns;
	do {
		write_rgb(cur_rgb+0,            bpp, cur_bay[1], ((unsigned int)cur_bay[0] + cur_bay[bay_line+1])/2, cur_bay[bay_line]);
		write_rgb(cur_rgb+bpp,          bpp, cur_bay[1], ((unsigned int)cur_bay[2] + cur_bay[bay_line+1])/2, cur_bay[bay_line+2]);
		write_rgb(cur_rgb+rgb_line,     bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
		write_rgb(cur_rgb+rgb_line+bpp, bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line+2]);
		cur_bay += 2;
		cur_rgb += 2*bpp;
	} while (--columns);
	/* Last lower-right pixel is handled here as special case */
	write_rgb(cur_rgb+0,            bpp, cur_bay[1], ((unsigned int)cur_bay[0] + cur_bay[bay_line+1])/2, cur_bay[bay_line]);
	write_rgb(cur_rgb+bpp,          bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
	write_rgb(cur_rgb+rgb_line,     bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
	write_rgb(cur_rgb+rgb_line+bpp, bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
}		  

static inline void bayer_to_rgb_cottnoip(unsigned char *bay, int bay_line,
		   unsigned char *rgb, int rgb_line,
		   int columns, int rows, int bpp)
{
	unsigned char *cur_bay, *cur_rgb;
	int bay_line2, rgb_line2;
	int total_columns;

	/* Process 2 lines and rows per each iteration, but process the last row and column separately */
	total_columns = (columns>>1) - 1;
	rows = (rows>>1) - 1;
	bay_line2 = 2*bay_line;
	rgb_line2 = 2*rgb_line;
	do {
		cur_bay = bay;
		cur_rgb = rgb;
		columns = total_columns;
		do {
			write_rgb(cur_rgb+0,            bpp, cur_bay[1],           cur_bay[0], cur_bay[bay_line]);
			write_rgb(cur_rgb+bpp,          bpp, cur_bay[1],           cur_bay[2], cur_bay[bay_line+2]);
			write_rgb(cur_rgb+rgb_line,     bpp, cur_bay[bay_line2+1], cur_bay[bay_line+1], cur_bay[bay_line]);
			write_rgb(cur_rgb+rgb_line+bpp, bpp, cur_bay[bay_line2+1], cur_bay[bay_line+1], cur_bay[bay_line+2]);
			cur_bay += 2;
			cur_rgb += 2*bpp;
		} while (--columns);
		write_rgb(cur_rgb+0,            bpp, cur_bay[1], cur_bay[0], cur_bay[bay_line]);
		write_rgb(cur_rgb+bpp,          bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
		write_rgb(cur_rgb+rgb_line,     bpp, cur_bay[bay_line2+1], cur_bay[bay_line+1], cur_bay[bay_line]);
		write_rgb(cur_rgb+rgb_line+bpp, bpp, cur_bay[bay_line2+1], cur_bay[bay_line+1], cur_bay[bay_line]);
		bay += bay_line2;
		rgb += rgb_line2;
	} while (--rows);
	/* Last scanline handled here as special case */
	cur_bay = bay;
	cur_rgb = rgb;
	columns = total_columns;
	do {
		write_rgb(cur_rgb+0,            bpp, cur_bay[1], cur_bay[0], cur_bay[bay_line]);
		write_rgb(cur_rgb+bpp,          bpp, cur_bay[1], cur_bay[2], cur_bay[bay_line+2]);
		write_rgb(cur_rgb+rgb_line,     bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
		write_rgb(cur_rgb+rgb_line+bpp, bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line+2]);
		cur_bay += 2;
		cur_rgb += 2*bpp;
	} while (--columns);
	/* Last lower-right pixel is handled here as special case */
	write_rgb(cur_rgb+0,            bpp, cur_bay[1], cur_bay[0], cur_bay[bay_line]);
	write_rgb(cur_rgb+bpp,          bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
	write_rgb(cur_rgb+rgb_line,     bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
	write_rgb(cur_rgb+rgb_line+bpp, bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
}


#if 0
	/* Bilinear filtering */
	static int wrg0 = 0;	/* Weight for Red on Green */
	static int wbg0 = 0;
	static int wgr0 = 0;
	static int wbr0 = 0;
	static int wgb0 = 0;
	static int wrb0 = 0;
#elif 1
	/* Best PSNR */
	static int wrg0 = 144;
	static int wbg0 = 160;
	static int wgr0 = 120;
	static int wbr0 = 192;
	static int wgb0 = 120;
	static int wrb0 = 168;
#elif 0
	/* Very sharp */
	static int wrg0 = ;
	static int wbg0 = ;
	static int wgr0 = ;
	static int wbr0 = ;
	static int wgb0 = ;
	static int wrb0 = ;
#else
	/* Sharp with round multipliers */
	static int wrg0 = 256;
	static int wbg0 = 256;
	static int wgr0 = 128;
	static int wbr0 = 128;
	static int wgb0 = 256;
	static int wrb0 = 256;
#endif
	static unsigned int sharpness = 32767;

	int wrg;
	int wbg;
	int wgr;
	int wbr;
	int wgb;
	int wrb;


static void bayer_to_rgb_gptm_printval(void) {
	printf("0: wrg %i wbg %i wgr %i wbr %i wgb %i wrb %i\n", wrg0, wbg0, wgr0, wbr0, wgb0, wrb0);
	printf("f: wrg %i wbg %i wgr %i wbr %i wgb %i wrb %i\n", wrg, wbg, wgr, wbr, wgb, wrb);
	printf("sharpness = %i\n", sharpness);
}

static inline void bayer_to_rgb_gptm(unsigned char *bay, int bay_line,
		   unsigned char *rgb, int rgb_line,
		   int columns, int rows, int bpp)
{

	/* 0.4 fixed point weights, should be between 0-16. Larger value = sharper */
	unsigned int wu;
	int r,g,b,w;
	unsigned char *cur_bay, *cur_rgb;
	int bay_line2, bay_line3, rgb_line2;
	int total_columns;

	/* Compute weights */
	wu = (sharpness * sharpness) >> 16;
 	wu = (wu * wu) >> 16;
	wrg = (wrg0 * wu) >> 10;
	wbg = (wbg0 * wu) >> 10;
	wgr = (wgr0 * wu) >> 10;
	wbr = (wbr0 * wu) >> 10;
	wgb = (wgb0 * wu) >> 10;
	wrb = (wrb0 * wu) >> 10;
//	bayer_to_rgb_gptm_printval();

	/* Process 2 lines and rows per each iteration, but process the first and last two columns and rows separately */
	total_columns = (columns>>1) - 2;
	rows = (rows>>1) - 2;
	bay_line2 = 2*bay_line;
	bay_line3 = 3*bay_line;
	rgb_line2 = 2*rgb_line;

	/* Process first two pixel rows here */
	cur_bay = bay;
	cur_rgb = rgb;
	columns = total_columns + 2;
	do {
		write_rgb(cur_rgb+0,            bpp, cur_bay[1], cur_bay[0],          cur_bay[bay_line]);
		write_rgb(cur_rgb+bpp,          bpp, cur_bay[1], cur_bay[0],          cur_bay[bay_line]);
		write_rgb(cur_rgb+rgb_line,     bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
		write_rgb(cur_rgb+rgb_line+bpp, bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
		cur_bay += 2;
		cur_rgb += 2*bpp;
	} while (--columns);
	bay += bay_line2;
	rgb += rgb_line2;

	do {
		cur_bay = bay;
		cur_rgb = rgb;
		columns = total_columns;
		
		/* Process first 2x2 pixel block in a row here */
		write_rgb(cur_rgb+0,            bpp, cur_bay[1], cur_bay[0],          cur_bay[bay_line]);
		write_rgb(cur_rgb+bpp,          bpp, cur_bay[1], cur_bay[0],          cur_bay[bay_line]);
		write_rgb(cur_rgb+rgb_line,     bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
		write_rgb(cur_rgb+rgb_line+bpp, bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
		cur_bay += 2;
		cur_rgb += 2*bpp;

		do {
			w = 4*cur_bay[0] - (cur_bay[-bay_line-1] + cur_bay[-bay_line+1] + cur_bay[bay_line-1] + cur_bay[bay_line+1]);
			r = (512*(cur_bay[-1] + cur_bay[1]) + w*wrg) >> 10;
			b = (512*(cur_bay[-bay_line] + cur_bay[bay_line]) + w*wbg) >> 10;
			write_rgb(cur_rgb+0, bpp, CLIP(r,0,255), cur_bay[0], CLIP(b,0,255));

			w = 4*cur_bay[1] - (cur_bay[-bay_line2+1] + cur_bay[-1] + cur_bay[3] + cur_bay[bay_line2+1]);
			g = (256*(cur_bay[-bay_line+1] + cur_bay[0] + cur_bay[2] + cur_bay[bay_line+1]) + w*wgr) >> 10;
			b = (256*(cur_bay[-bay_line] + cur_bay[-bay_line+2] + cur_bay[bay_line] + cur_bay[bay_line+2]) + w*wbr) >> 10;
			write_rgb(cur_rgb+bpp, bpp, cur_bay[1], CLIP(g,0,255), CLIP(b,0,255));

			w = 4*cur_bay[bay_line] - (cur_bay[-bay_line] + cur_bay[bay_line-2] + cur_bay[bay_line+2] + cur_bay[bay_line3]);
			r = (256*(cur_bay[-1] + cur_bay[1] + cur_bay[bay_line2-1] + cur_bay[bay_line2+1]) + w*wrb) >> 10;
			g = (256*(cur_bay[0] + cur_bay[bay_line-1] + cur_bay[bay_line+1] + cur_bay[bay_line2]) + w*wgb) >> 10;
			write_rgb(cur_rgb+rgb_line, bpp, CLIP(r,0,255), CLIP(g,0,255), cur_bay[bay_line]);

			w = 4*cur_bay[bay_line+1] - (cur_bay[0] + cur_bay[2] + cur_bay[bay_line2] + cur_bay[bay_line2+2]);
			r = (512*(cur_bay[1] + cur_bay[bay_line2+1]) + w*wrg) >> 10;
			b = (512*(cur_bay[bay_line] + cur_bay[bay_line+2]) + w*wbg) >> 10;
			write_rgb(cur_rgb+rgb_line+bpp, bpp, CLIP(r,0,255), cur_bay[bay_line+1], CLIP(b,0,255));

			cur_bay += 2;
			cur_rgb += 2*bpp;
		} while (--columns);

		/* Process last 2x2 pixel block in a row here */
		write_rgb(cur_rgb+0,            bpp, cur_bay[1], cur_bay[0],          cur_bay[bay_line]);
		write_rgb(cur_rgb+bpp,          bpp, cur_bay[1], cur_bay[0],          cur_bay[bay_line]);
		write_rgb(cur_rgb+rgb_line,     bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
		write_rgb(cur_rgb+rgb_line+bpp, bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);

		bay += bay_line2;
		rgb += rgb_line2;
	} while (--rows);

	/* Process last two pixel rows here */
	cur_bay = bay;
	cur_rgb = rgb;
	columns = total_columns + 2;
	do {
		write_rgb(cur_rgb+0,            bpp, cur_bay[1], cur_bay[0],          cur_bay[bay_line]);
		write_rgb(cur_rgb+bpp,          bpp, cur_bay[1], cur_bay[0],          cur_bay[bay_line]);
		write_rgb(cur_rgb+rgb_line,     bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
		write_rgb(cur_rgb+rgb_line+bpp, bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
		cur_bay += 2;
		cur_rgb += 2*bpp;
	} while (--columns);
}

/* faster version of bayer_to_rgb_gptm with fixed multipliers */
static inline void bayer_to_rgb_gptm_fast(unsigned char *bay, int bay_line,
		   unsigned char *rgb, int rgb_line,
		   int columns, int rows, int bpp)
{
	int r,g,b,w;
	unsigned char *cur_bay, *cur_rgb;
	int bay_line2, bay_line3, rgb_line2;
	int total_columns;

	/* Process 2 lines and rows per each iteration, but process the first and last two columns and rows separately */
	total_columns = (columns>>1) - 2;
	rows = (rows>>1) - 2;
	bay_line2 = 2*bay_line;
	bay_line3 = 3*bay_line;
	rgb_line2 = 2*rgb_line;

	/* Process first two pixel rows here */
	cur_bay = bay;
	cur_rgb = rgb;
	columns = total_columns + 2;
	do {
		write_rgb(cur_rgb+0,            bpp, cur_bay[1], cur_bay[0],          cur_bay[bay_line]);
		write_rgb(cur_rgb+bpp,          bpp, cur_bay[1], cur_bay[0],          cur_bay[bay_line]);
		write_rgb(cur_rgb+rgb_line,     bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
		write_rgb(cur_rgb+rgb_line+bpp, bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
		cur_bay += 2;
		cur_rgb += 2*bpp;
	} while (--columns);
	bay += bay_line2;
	rgb += rgb_line2;

	do {
		cur_bay = bay;
		cur_rgb = rgb;
		columns = total_columns;
		
		/* Process first 2x2 pixel block in a row here */
		write_rgb(cur_rgb+0,            bpp, cur_bay[1], cur_bay[0],          cur_bay[bay_line]);
		write_rgb(cur_rgb+bpp,          bpp, cur_bay[1], cur_bay[0],          cur_bay[bay_line]);
		write_rgb(cur_rgb+rgb_line,     bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
		write_rgb(cur_rgb+rgb_line+bpp, bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
		cur_bay += 2;
		cur_rgb += 2*bpp;

		do {
			w = 4*cur_bay[0] - (cur_bay[-bay_line-1] + cur_bay[-bay_line+1] + cur_bay[bay_line-1] + cur_bay[bay_line+1]);
			r = (2*(cur_bay[-1] + cur_bay[1]) + w) >> 2;
			b = (2*(cur_bay[-bay_line] + cur_bay[bay_line]) + w) >> 2;
			write_rgb(cur_rgb+0, bpp, CLIP(r,0,255), cur_bay[0], CLIP(b,0,255));

			w = 4*cur_bay[1] - (cur_bay[-bay_line2+1] + cur_bay[-1] + cur_bay[3] + cur_bay[bay_line2+1]);
			g = (2*(cur_bay[-bay_line+1] + cur_bay[0] + cur_bay[2] + cur_bay[bay_line+1]) + w) >> 3;
			b = (2*(cur_bay[-bay_line] + cur_bay[-bay_line+2] + cur_bay[bay_line] + cur_bay[bay_line+2]) + w) >> 3;
			write_rgb(cur_rgb+bpp, bpp, cur_bay[1], CLIP(g,0,255), CLIP(b,0,255));

			w = 4*cur_bay[bay_line] - (cur_bay[-bay_line] + cur_bay[bay_line-2] + cur_bay[bay_line+2] + cur_bay[bay_line3]);
			r = ((cur_bay[-1] + cur_bay[1] + cur_bay[bay_line2-1] + cur_bay[bay_line2+1]) + w) >> 2;
			g = ((cur_bay[0] + cur_bay[bay_line-1] + cur_bay[bay_line+1] + cur_bay[bay_line2]) + w) >> 2;
			write_rgb(cur_rgb+rgb_line, bpp, CLIP(r,0,255), CLIP(g,0,255), cur_bay[bay_line]);

			w = 4*cur_bay[bay_line+1] - (cur_bay[0] + cur_bay[2] + cur_bay[bay_line2] + cur_bay[bay_line2+2]);
			r = (2*(cur_bay[1] + cur_bay[bay_line2+1]) + w) >> 2;
			b = (2*(cur_bay[bay_line] + cur_bay[bay_line+2]) + w) >> 2;
			write_rgb(cur_rgb+rgb_line+bpp, bpp, CLIP(r,0,255), cur_bay[bay_line+1], CLIP(b,0,255));

			cur_bay += 2;
			cur_rgb += 2*bpp;
		} while (--columns);

		/* Process last 2x2 pixel block in a row here */
		write_rgb(cur_rgb+0,            bpp, cur_bay[1], cur_bay[0],          cur_bay[bay_line]);
		write_rgb(cur_rgb+bpp,          bpp, cur_bay[1], cur_bay[0],          cur_bay[bay_line]);
		write_rgb(cur_rgb+rgb_line,     bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
		write_rgb(cur_rgb+rgb_line+bpp, bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);

		bay += bay_line2;
		rgb += rgb_line2;
	} while (--rows);

	/* Process last two pixel rows here */
	cur_bay = bay;
	cur_rgb = rgb;
	columns = total_columns + 2;
	do {
		write_rgb(cur_rgb+0,            bpp, cur_bay[1], cur_bay[0],          cur_bay[bay_line]);
		write_rgb(cur_rgb+bpp,          bpp, cur_bay[1], cur_bay[0],          cur_bay[bay_line]);
		write_rgb(cur_rgb+rgb_line,     bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
		write_rgb(cur_rgb+rgb_line+bpp, bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
		cur_bay += 2;
		cur_rgb += 2*bpp;
	} while (--columns);
}

/* Copy only those values into RGB image, which are in the Bayer image too, leave the rest zero */
static inline void bayer_to_rgb_copyonly(unsigned char *bay, int bay_line,
		   unsigned char *rgb, int rgb_line,
		   int columns, int rows, int bpp)
{
	unsigned char *cur_bay, *cur_rgb;
	int bay_line2, rgb_line2;
	int total_columns;

	/* Process 2 lines and rows per each iteration */
	total_columns = columns >> 1;
	rows >>= 1;
	bay_line2 = 2*bay_line;
	rgb_line2 = 2*rgb_line;
	do {
		cur_bay = bay;
		cur_rgb = rgb;
		columns = total_columns;
		do {
			write_rgb(cur_rgb+0,            bpp, 0,          cur_bay[0],          0);
			write_rgb(cur_rgb+bpp,          bpp, cur_bay[1], 0,                   0);
			write_rgb(cur_rgb+rgb_line,     bpp, 0,          0,                   cur_bay[bay_line]);
			write_rgb(cur_rgb+rgb_line+bpp, bpp, 0,          cur_bay[bay_line+1], 0);
			cur_bay += 2;
			cur_rgb += 2*bpp;
		} while (--columns);
		bay += bay_line2;
		rgb += rgb_line2;
	} while (--rows);
}		  


void bayer_to_rgb(unsigned char *bay, int bay_line, unsigned char *rgb, int rgb_line, int columns, int rows)
{
	/* Measure 100 frames at highest optimization level, cycles per pixel */
//	bayer_to_rgb_noip(bay, bay_line, rgb, rgb_line, columns, rows, 4);		/* 20.29 */
//	bayer_to_rgb_horip(bay, bay_line, rgb, rgb_line, columns, rows, 4);		/* 21.77 */
//	bayer_to_rgb_ip(bay, bay_line, rgb, rgb_line, columns, rows, 4);		/* 21.02 */
//	bayer_to_rgb_cott(bay, bay_line, rgb, rgb_line, columns, rows, 4);		/* 21.25 */
//	bayer_to_rgb_cottnoip(bay, bay_line, rgb, rgb_line, columns, rows, 4);		/* 21.35 */
//	bayer_to_rgb_gptm(bay, bay_line, rgb, rgb_line, columns, rows, 4);		/* 42.85 (36.62 with unsigned multipliers) */
//	bayer_to_rgb_gptm_fast(bay, bay_line, rgb, rgb_line, columns, rows, 4);		/* 37.44 */
	bayer_to_rgb_copyonly(bay, bay_line, rgb, rgb_line, columns, rows, 4);		/* 20.48 */
}

static inline void grey_to_buffer(unsigned char *grey, int grey_line, unsigned char *rgb, int rgb_line, int columns, int rows, int bpp)
{
	unsigned char *cur_grey, *cur_rgb;
	int total_columns = columns;
	do {
		cur_grey = grey;
		cur_rgb  = rgb;
		columns  = total_columns;
		do {
			write_rgb(cur_rgb, bpp, *cur_grey, *cur_grey, *cur_grey);
			cur_grey++;
			cur_rgb += bpp;
		} while (--columns);
		grey += grey_line;
		rgb  += rgb_line;
	} while (--rows);
}


/*
 Copyright (c) 2000 Jeroen B. Vreeken (pe1rxq@amsat.org)
 from Endpoints (formerly known as AOX) se401 USB Camera Driver
*/
/*
        This shouldn't really be done in a v4l driver....
        But it does make the image look a lot more usable.
        Basically it lifts the dark pixels more than the light pixels.
*/
/* 1195964 clock cycles on PIII, 4 times slower than LUT-approach */
static inline void enhance_picture(unsigned char *frame, int len)
{
        while (len--) {
                *frame=(((*frame^255)*(*frame^255))/255)^255;
                frame++;
        }
}

/*-----------------------------------------------------------------*/
void initializeX (int width, int height)
{
	XGCValues *xgc;

	if (dpy!=0) 
		return;

	xgc=( XGCValues *) malloc(sizeof(XGCValues) );

	if (!(dpy = XOpenDisplay (""))) 
	{
		(void) fprintf (stderr, "Error:  Can't open display\n");
		exit (1);
	}

	screen = DefaultScreen (dpy);   

	win = XCreateSimpleWindow (dpy, DefaultRootWindow (dpy),
				   0, 0, width, height, 1,
				   WhitePixel (dpy, screen),
				   BlackPixel (dpy, screen));

	XSelectInput (dpy, win, 
		      ExposureMask | StructureNotifyMask
		      | KeyPressMask | ButtonPressMask
		      |VisibilityChangeMask |PropertyChangeMask);

	depth=DefaultDepth(dpy,screen);
	if ((depth!=24)&&(depth!=16))
	{
		fprintf(stderr,"quickcam works only with 16/24Bit displays\n");
		exit(0);
	}
	
	if (depth==24)
		bypp=4;
	else
		bypp=2;

	XStoreName (dpy, win, "QuickCam");
	XMapWindow (dpy, win);

	gc = XCreateGC (dpy, win, 0,  xgc);

	for (;;) 
	{		
		XEvent event;
		XNextEvent(dpy, &event);
		if (event.type == Expose) break;
	}
}
/*-----------------------------------------------------------------*/
void NewPictImage(int width, int height)
{       
	img=NULL;
  
	img = XShmCreateImage(dpy, None, depth, ZPixmap, NULL,
			      &shminfo, width, height+16);

	if (img == NULL) 
	{
		fprintf(stderr, "Shared memory error.\n");
		fprintf(stderr, "Ximage error.\n");
		exit(0);
	}

	/* Success here, continue. */

	shminfo.shmid = shmget(IPC_PRIVATE, (width*bypp*(height+16)), 
			       IPC_CREAT|0777);
    
	if (shminfo.shmid < 0) 
	{
		XDestroyImage(img);
		img = NULL;
		fprintf(stderr, "Shared memory error.\n");
		fprintf(stderr, "Seg. id. error.\n");
		exit(0);
	}

	shminfo.shmaddr = (char *) shmat(shminfo.shmid, 0, 0);
	if (shminfo.shmaddr == ((char *) -1)) 
	{
		XDestroyImage(img);
		img = NULL;
		fprintf(stderr, "Shared memory error.\n");
		fprintf(stderr, "Address error.\n");
		exit(0);
	}

	img->data = shminfo.shmaddr;
	shminfo.readOnly = False;

	XShmAttach(dpy, &(shminfo));
	shmctl(shminfo.shmid,IPC_RMID,NULL);
	XSync(dpy, False);

	fprintf(stderr, "Sharing memory.\n");
}

void set_pixel24(int mode, int x, int y, int yy, int xxx, int width,
		 long long *mid_valuer, long long *mid_valueg, long long *mid_valueb)
{
	unsigned char *o;

	o=((char*)shminfo.shmaddr)+4*x+(width*4*(yy));
 
	if (!(y&1)) // even row
	{
		if ((x&1)) // odd column, red
		{
			*mid_valuer+=xxx;
			*(o+2)=xxx;
			*(o+2-4)=xxx;
	      
			if (!mode)
			{
				*(o+2+4*width)=xxx;
				*(o+2-4+4*width)=xxx;
			}
		}
		else  // green
		{
			*(o+1)=xxx;		     
			*(o+1+4)=xxx;
			*mid_valueg+=xxx;
		}
	}
	else
	{
		if ((x&1)) // odd column green
		{	
			*mid_valueg+=xxx;
			*(o+1)=xxx;
			*(o+1-4)=xxx;			
		}
		else //blue
		{
			*mid_valueb+=xxx;
			*(o)=xxx;
			*(o+4)=xxx;
			if (!mode)
			{
				*(o-4*width)=xxx;
				*(o+4-4*width)=xxx;	
			}
		}
	}
}

void set_rgbpixel24(int x, int y, int r, int g, int b, int width)
{
	unsigned char *o;

	o=((char*)shminfo.shmaddr)+4*x+(width*4*y);
	*(o+2) = r;
	*(o+1) = g;
	*(o  ) = b;
}
void set_rgbpixel16(int x, int y, int r, int g, int b, int width) 
{
	unsigned short *o;

	o = ((ushort*)shminfo.shmaddr)+x+(width*y);
	*(o) = (r>>3) << 11 | (g >> 2) << 5 | (b >> 3);
}
/*-----------------------------------------------------------------*/
// Byte 0:  GGGBBBBB
// Byte 1:  RRRRRGGG
// short :  RRRR RGGG GGGB BBBB

#define SET_PIXR(addr, val) *(addr)=(*(addr)&~0xf800)|(((val)>>3)<<11)
#define SET_PIXG(addr, val) *(addr)=(*(addr)&~0x07e0)|(((val)>>2)<<5)
#define SET_PIXB(addr, val) *(addr)=(*(addr)&~0x001f)|(((val)>>3))

#define WEIGHTING(x,y) 1

void set_pixel16(int mode, int x, int y, int yy, int xxx, int width,
		 long long *mid_valuer, long long *mid_valueg, long long *mid_valueb)
{
	short *o;

	o=(short*)((char*)shminfo.shmaddr+ 2*x+ width*2*yy);
 

	if (!(y&1)) // even row
	{
		if ((x&1)) // odd column, red
		{
			*mid_valuer+=xxx;
#if 1
			SET_PIXR(o,xxx);
			SET_PIXR(o+1,xxx);
			if (!mode)
			{
				SET_PIXR(o+width,xxx);
				SET_PIXR(o+1+width,xxx);
			}
#endif
		}
		else  // green
		{
			 
			SET_PIXG(o+1,xxx);
			SET_PIXG(o,xxx);
			*mid_valueg+=xxx;
		}
	}
	else
	{
		if ((x&1)) // odd column green
		{	
			*mid_valueg+=xxx;
			SET_PIXG(o+1,xxx);
			SET_PIXG(o+0,xxx);
		}
		else //blue
		{
			*mid_valueb+=xxx;
#if 1
			SET_PIXB(o,xxx);
			SET_PIXB(o+1,xxx);
			if (!mode)
			{
				SET_PIXB(o+width,xxx);
				SET_PIXB(o+1+width,xxx);
			}
#endif
		}
	}
}


void print_cap(struct video_capability *vidcap, struct video_window *vidwin, 
				struct video_channel *vidchan,struct video_picture *vidpic) {
	if (vidcap) {
		printf("Name: %s\n", vidcap->name);
		printf("Type: ");
		if (vidcap->type & VID_TYPE_CAPTURE)    printf("capture ");
		if (vidcap->type & VID_TYPE_TUNER)      printf("tuner ");
		if (vidcap->type & VID_TYPE_TELETEXT)   printf("teletext ");
		if (vidcap->type & VID_TYPE_OVERLAY)    printf("overlay ");
		if (vidcap->type & VID_TYPE_CHROMAKEY)  printf("chromakey ");
		if (vidcap->type & VID_TYPE_CLIPPING)   printf("clipping ");
		if (vidcap->type & VID_TYPE_FRAMERAM)   printf("frameram ");
		if (vidcap->type & VID_TYPE_SCALES)     printf("scales ");
		if (vidcap->type & VID_TYPE_MONOCHROME) printf("monochrome ");
		if (vidcap->type & VID_TYPE_SUBCAPTURE) printf("subcapture ");
		printf("\n");
		printf("Channels: %i\n", vidcap->channels);
		printf("Audio devices: %i\n", vidcap->audios);
		printf("Maxsize: %i,%i\n", vidcap->maxwidth,vidcap->maxheight);
		printf("Minsize: %i,%i\n", vidcap->minwidth,vidcap->minheight);
		printf("\n");
	}
	if (vidwin) {
		printf("Overlay coords: %i,%i\n", (int)vidwin->x, (int)vidwin->y);
		printf("Capture size: %i,%i\n", vidwin->width, vidwin->height);
		printf("Chromakey: %i\n", vidwin->chromakey);
		printf("Flags: ");
		if (vidwin->flags & VIDEO_CAPTURE_ODD)    printf("odd ");
		if (vidwin->flags & VIDEO_CAPTURE_EVEN)   printf("even ");
		printf("\n");
		printf("\n");
	}
	if (vidchan) {
		printf("Channel: %i\n", vidchan->channel);
		printf("Name: %s\n", vidchan->name);
		printf("Tuners: %i\n", vidchan->tuners);
		printf("Flags: ");
		if (vidchan->flags & VIDEO_VC_TUNER) printf("tuner ");
		if (vidchan->flags & VIDEO_VC_AUDIO) printf("audio ");
//		if (vidchan->flags & VIDEO_VC_NORM) printf("norm ");
		printf("\n");
		printf("Type: ");
		switch (vidchan->type) {
			case VIDEO_TYPE_TV: printf("tv\n"); break;
			case VIDEO_TYPE_CAMERA: printf("camera\n"); break;
			default: printf("(unknown)\n"); break;
		}
		printf("Norm: %i\n", vidchan->norm);
		printf("\n");
	}
	if (vidpic) {
		printf("Brightness: %i\n", vidpic->brightness);
		printf("Hue:        %i\n", vidpic->hue);
		printf("Color:      %i\n", vidpic->colour);
		printf("Contrast:   %i\n", vidpic->contrast);
		printf("Whiteness:  %i\n", vidpic->whiteness);
		printf("Depth:      %i\n", vidpic->depth);
		printf("Palette:    ");
		switch (vidpic->palette) {
			case VIDEO_PALETTE_GREY:    printf("Linear intensity grey scale (255 is brightest).\n");break;
			case VIDEO_PALETTE_HI240:   printf("The BT848 8bit colour cube.\n");break;
			case VIDEO_PALETTE_RGB565:  printf("RGB565 packed into 16 bit words.\n");break;
			case VIDEO_PALETTE_RGB555:  printf("RGV555 packed into 16 bit words, top bit undefined.\n");break;
			case VIDEO_PALETTE_RGB24:   printf("RGB888 packed into 24bit words.\n");break;
			case VIDEO_PALETTE_RGB32:   printf("RGB888 packed into the low 3 bytes of 32bit words. The top 8bits are undefined.\n");break;
			case VIDEO_PALETTE_YUV422:  printf("Video style YUV422 - 8bits packed 4bits Y 2bits U 2bits V\n");break;
			case VIDEO_PALETTE_YUYV:    printf("Describe me\n");break;
			case VIDEO_PALETTE_UYVY:    printf("Describe me\n");break;
			case VIDEO_PALETTE_YUV420:  printf("YUV420 capture\n");break;
			case VIDEO_PALETTE_YUV411:  printf("YUV411 capture\n");break;
			case VIDEO_PALETTE_RAW:     printf("RAW capture (BT848)\n");break;
			case VIDEO_PALETTE_YUV422P: printf("YUV 4:2:2 Planar\n");break;
			case VIDEO_PALETTE_YUV411P: printf("YUV 4:1:1 Planar\n");break;
#ifdef VIDEO_PALETTE_BAYER
			case VIDEO_PALETTE_BAYER:   printf("Raw Bayer capture\n");break;
#endif
#ifdef VIDEO_PALETTE_MJPEG
			case VIDEO_PALETTE_MJPEG:   printf("MJPEG capture\n");break;
#endif
			default: printf("(unknown)\n"); break;
		}
	}
}

/*-----------------------------------------------------------------*/
int main(int argc, char** argv)
{
	struct qc_mjpeg_data mjpeg_data;
//	int usepalette = VIDEO_PALETTE_GREY;
	int usepalette = VIDEO_PALETTE_RGB24;
//	int usepalette = VIDEO_PALETTE_BAYER;
//	int usepalette = VIDEO_PALETTE_MJPEG;
	int fd,r,y,x;
	int W;
	int H;
	int flag = 0;
	int mode=0; // 0:352*288, 1: 176*144
	char *device = "/dev/video0";
	int argp = 1;
	unsigned char *buffer;
	int buffersize;
	struct video_capability vidcap;
	struct video_window vidwin;
	struct video_channel vidchan;
	struct video_picture vidpic;
	XEvent event;
	unsigned long long starttime, stoptime, totaltime=0, minimum=~0;
	int benchmarks=0;
	int framenum = 0;
	time_t begintime=0,nowtime;
	
#if DUMPDATA
	usepalette = VIDEO_PALETTE_GREY;
#endif
	if (argp < argc && memcmp("/dev", argv[argp], 4) == 0)
		device = argv[argp++];

	if (argp < argc)
		mode=atoi(argv[argp++]);

	// Open the driver...
	fd=open(device,O_RDWR);
	if (fd==-1)
	{
		perror("Can not open video device");
		exit(0);
	}
	if (mode>1)
	{
		fprintf(stderr, "Usage: show <mode>\n"
			"mode: 0 (352*288), 1 (176*144)\n");
		exit(0);
	}
	r=ioctl(fd, VIDIOCGCAP, &vidcap);
	if (r!=0) { perror("ioctl"); exit(1); }
	memset(&vidwin, 0, sizeof(vidwin));

	vidwin.width = vidcap.maxwidth;///2 + 1;
	vidwin.height = vidcap.maxheight;///2 + 1;
//vidwin.width=48;
//vidwin.height=32;
//vidwin.width=360;
//vidwin.height=270;

	r=ioctl(fd, VIDIOCSWIN, &vidwin);
	if (r!=0) { perror("ioctl VIDIOCSWIN"); exit(1); }
	r=ioctl(fd, VIDIOCGWIN, &vidwin);
	if (r!=0) { perror("ioctl VIDIOCGWIN"); exit(1); }
	vidchan.channel = 0;
	r=ioctl(fd, VIDIOCGCHAN, &vidchan);
	if (r!=0) { perror("ioctl VIDIOCGCHAN"); exit(1); }
	r=ioctl(fd, VIDIOCSCHAN, &vidchan);
	if (r!=0) { perror("ioctl VIDIOCSCHAN"); exit(1); }
	r=ioctl(fd, VIDIOCGPICT, &vidpic);
	if (r!=0) { perror("ioctl VIDIOCGPICT"); exit(1); }
	vidpic.depth = (usepalette!=VIDEO_PALETTE_BAYER && usepalette!=VIDEO_PALETTE_GREY) ? 24 : 8;
	vidpic.palette = usepalette;
	r=ioctl(fd, VIDIOCSPICT, &vidpic);
	if (r!=0) { perror("ioctl VIDIOCSPICT"); exit(1); }
	r=ioctl(fd, VIDIOCGPICT, &vidpic);
	if (r!=0) { perror("ioctl VIDIOCGPICT"); exit(1); }
	print_cap(&vidcap,&vidwin,&vidchan,&vidpic);

	W = vidwin.width;
	H = vidwin.height;
	buffersize = (usepalette==VIDEO_PALETTE_BAYER || usepalette==VIDEO_PALETTE_GREY) ? H*W : 3*H*W;
	printf("using frame size: %i x %i, buffersize=%i\n",W,H,buffersize);
	buffer = (char *)malloc(buffersize);
	initializeX(W,H);
	if (usepalette==VIDEO_PALETTE_MJPEG) {
		r = qc_mjpeg_init(&mjpeg_data, 32, FALSE);
		if (r) { printf("qc_mjpeg_init() failed, code=%i\n",r);exit(1);}
	}
	NewPictImage(W,H);
	printf("START\n");
	while (1) {
		int midvalue, readlen;
//		usleep(3000000);
//		usleep(50000);
//		printf("read()=");
		fflush(stdout);
		readlen = read(fd,buffer,buffersize);
//		printf("%i\n",readlen);
		if (readlen!=buffersize && usepalette!=VIDEO_PALETTE_MJPEG) { 
			printf("Short read by %i, skipping frame\n", buffersize-readlen);
exit(1);
			continue; 
		}
		if (usepalette==VIDEO_PALETTE_BAYER) {
/*			for (y=0; y<H; y++) for (x=0; x<W; x++) {
				set_rgbpixel24(x,y,buffer[y*W+x],buffer[y*W+x],buffer[y*W+x],W);
			}*/
//			midvalue = bayer_midvalue(buffer,W, W,H);
//			printf("midval=%i\n", midvalue);
//			bayer_equalize(buffer,W, W,H, lut);
starttime = rdtsc();
//			bayer_lut(buffer,W, W,H, lut);
stoptime = rdtsc();
if (flag) enhance_picture(buffer, W*H);
			bayer_to_rgb(buffer,W,shminfo.shmaddr,4*W, W,H);
stoptime -= starttime;
totaltime += stoptime;
benchmarks++;
if (stoptime<minimum) minimum=stoptime;
printf("Elapsed: %i, Average: %i, Minimum: %i\n", (int)stoptime, (int)(totaltime/benchmarks), (int)minimum);
		} else if (usepalette==VIDEO_PALETTE_GREY) {
#if DUMPDATA
			int w=163; 
			int h=288;
			grey_to_buffer(buffer,w, shminfo.shmaddr,4*W, w,h, 4);
#else
			grey_to_buffer(buffer,W,shminfo.shmaddr,4*W, W,H, 4);
#endif
		} else if (usepalette==VIDEO_PALETTE_MJPEG) {
			printf("data: %02X %02X %02X %02X %02X   size: %i bytes\n",
				buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], readlen);
			r = qc_mjpeg_decode(&mjpeg_data, buffer, readlen, shminfo.shmaddr);
			if (r) { printf("qc_mjpeg_decode() failed, code=%i\n",r);}
		} else {
			for (y=0; y<H; y++) for (x=0; x<W; x++) {
				set_rgbpixel24(x,y,buffer[3*(y*W+x)+2],buffer[3*(y*W+x)+1],buffer[3*(y*W+x)+0],W);
			}
		}
		XPutImage(dpy,win,gc,img,0,0,0,0,W,H);
		XSync(dpy,0);
		if (begintime==0) {
			begintime = time(NULL);
			framenum = 0;
		} else {
			framenum++;
			nowtime = time(NULL);
			nowtime -= begintime;
			if (nowtime>1 && framenum%10==0) {
				printf("%i frames, %i seconds = %i.%i fps\n", framenum, (int)nowtime, (int)(framenum/nowtime), (int)((framenum*10/nowtime) % 10));
			}
		}
		while (XPending(dpy)) {
			XNextEvent(dpy, &event);
			if (event.type == KeyPress) {
				char *c;
				int step = 1000;
				int delta = 32;
				c = XKeysymToString(XKeycodeToKeysym(dpy,event.xkey.keycode,0));
				r = 0;
//	printf("wrg %i wbg %i wgr %i wbr %i wgb %i wrb %i\n", wrg, wbg, wgr, wbr, wgb, wrb);
				if (c[0]!=0 && c[1]==0) switch (c[0]) {
					case 'q': goto done;
					case 'a':
						if (vidpic.brightness >= step) vidpic.brightness -= step;
						r = 1;
						break;
					case 's':
						if (vidpic.brightness <= 0xFFFF-step) vidpic.brightness += step;
						r = 1;
						break;
					case 'z':
						if (vidpic.contrast >= step) vidpic.contrast -= step;
						r = 1;
						break;
					case 'x':
						if (vidpic.contrast <= 0xFFFF-step) vidpic.contrast += step;
						r = 1;
						break;
					case 'r':	wrg0 -= delta;	bayer_to_rgb_gptm_printval();	break;
					case 't':	wrg0 += delta;	bayer_to_rgb_gptm_printval();	break;
					case 'f':	wbg0 -= delta;	bayer_to_rgb_gptm_printval();	break;
					case 'g':	wbg0 += delta;	bayer_to_rgb_gptm_printval();	break;
					case 'v':	wgr0 -= delta;	bayer_to_rgb_gptm_printval();	break;
					case 'b':	wgr0 += delta;	bayer_to_rgb_gptm_printval();	break;
					case 'y':	wbr0 -= delta;	bayer_to_rgb_gptm_printval();	break;
					case 'u':	wbr0 += delta;	bayer_to_rgb_gptm_printval();	break;
					case 'h':	wgb0 -= delta;	bayer_to_rgb_gptm_printval();	break;
					case 'j':	wgb0 += delta;	bayer_to_rgb_gptm_printval();	break;
					case 'n':	wrb0 -= delta;	bayer_to_rgb_gptm_printval();	break;
					case 'm':	wrb0 += delta;	bayer_to_rgb_gptm_printval();	break;
					case 'i':
						wrg0 -= delta;
						wbg0 -= delta;
						wgr0 -= delta;
						wbr0 -= delta;
						wgb0 -= delta;
						wrb0 -= delta;
						bayer_to_rgb_gptm_printval();	
						break;
					case 'o':
						wrg0 += delta;
						wbg0 += delta;
						wgr0 += delta;
						wbr0 += delta;
						wgb0 += delta;
						wrb0 += delta;
						bayer_to_rgb_gptm_printval();	
						break;
					case 'k':
						sharpness -= 8192/4;
						printf("sharpness: %i\n", sharpness);
						break;
					case 'l':
						sharpness += 8192/4;
						printf("sharpness: %i\n", sharpness);
						break;
					case 'p':
						bayer_to_rgb_gptm_printval();
						printf("sharpness: %i\n", sharpness);
						break;
					case 'c':
						flag = !flag;
						break;
					default:
						printf("Key %s not used (press q to quit)\n", c);
						break;						
				}
				if (r) {
					r=ioctl(fd, VIDIOCSPICT, &vidpic);
					if (r!=0) { perror("ioctl VIDIOCSPICT"); exit(1); }
					print_cap(NULL,NULL,NULL,&vidpic);
				}
			}
		}
	}
	done:
	printf("QUIT\n");
	close(fd);
	return 0;
}
