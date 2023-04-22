/* Start of file */

/* {{{ [fold] Comments */
/*
 * qcset: Logitech QuickCam USB Video Camera driver configuration tool.
 * Copyright (C) 2002,2003 Tuukka Toivonen <tuukkat@ee.oulu.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
/* }}} */
/* {{{ [fold] Standard includes */
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <math.h>
/* }}} */
/* {{{ [fold] Structures and types */
#include "videodev2.h"
#include "quickcam.h"

#define FALSE		0
#define TRUE		(!FALSE)
typedef unsigned char Bool;
#define BIT(x)		(1<<(x))
#define MAX(a,b)	((a)>(b)?(a):(b))
#define MIN(a,b)	((a)<(b)?(a):(b))
#define SIZE(x)		(sizeof(x)/sizeof((x)[0]))
#define CLIP(a,low,high) MAX((low),MIN((high),(a)))

char *progname = "qcset";

struct bitflags {
	char *name;
	int val;
};
const struct bitflags flags_debug[] = {
	{ "user",		QC_DEBUGUSER },
	{ "camera",		QC_DEBUGCAMERA },
	{ "init",		QC_DEBUGINIT },
	{ "logic",		QC_DEBUGLOGIC },
	{ "errors",		QC_DEBUGERRORS },
	{ "adaptation",		QC_DEBUGADAPTATION },
	{ "controlurbs",	QC_DEBUGCONTROLURBS },
	{ "bitstream",		QC_DEBUGBITSTREAM },
	{ "interrupts",		QC_DEBUGINTERRUPTS },
	{ "mutex",		QC_DEBUGMUTEX },
	{ "common",		QC_DEBUGCOMMON },
	{ "frame",		QC_DEBUGFRAME },
	{ "all",		QC_DEBUGALL },
	{ NULL,			0 }
};
const struct bitflags flags_compat[] = {
	{ "16x",		QC_COMPAT_16X },
	{ "dblbuf",		QC_COMPAT_DBLBUF },
	{ "torgb",		QC_COMPAT_TORGB },
	{ NULL,			0 }
};
const struct bitflags flags_quality[] = {
	{ "fastest",		0 },
	{ "good",		1 },
	{ "horizontal",		2 },
	{ "bilinear",		3 },
	{ "better",		4 },
	{ "best",		5 },
	{ NULL,			0 }
};
const struct bitflags flags_bool[] = {
	{ "y",			1 },
	{ "n",			0 },
	{ NULL,			0 }
};

const struct ioctlstruct {
	char *name;
	int get;
	int set;
	Bool bitfield;
	const struct bitflags *flags;
} ioctlinfo[] = {
	{ "debug",		VIDIOCQCGDEBUG,		VIDIOCQCSDEBUG,		TRUE,	flags_debug },	/* For backwards compatibility */
	{ "qcdebug",		VIDIOCQCGDEBUG,		VIDIOCQCSDEBUG,		TRUE,	flags_debug },
	{ "keepsettings",	VIDIOCQCGKEEPSETTINGS,	VIDIOCQCSKEEPSETTINGS,	FALSE,	flags_bool },
	{ "settle", 		VIDIOCQCGSETTLE,	VIDIOCQCSSETTLE,	FALSE,	NULL },
	{ "subsample",		VIDIOCQCGSUBSAMPLE,	VIDIOCQCSSUBSAMPLE,	FALSE,	flags_bool },
	{ "compress",		VIDIOCQCGCOMPRESS,	VIDIOCQCSCOMPRESS,	FALSE,	flags_bool },
	{ "frameskip",		VIDIOCQCGFRAMESKIP,	VIDIOCQCSFRAMESKIP,	FALSE,	NULL },
	{ "quality",		VIDIOCQCGQUALITY,	VIDIOCQCSQUALITY,	FALSE,	flags_quality },
	{ "adaptive",		VIDIOCQCGADAPTIVE, 	VIDIOCQCSADAPTIVE,	FALSE,	flags_bool },
	{ "equalize",		VIDIOCQCGEQUALIZE, 	VIDIOCQCSEQUALIZE,	FALSE,	flags_bool },
	{ "userlut",		VIDIOCQCGUSERLUT, 	VIDIOCQCSUSERLUT,	FALSE,	flags_bool },
	{ "retryerrors",	VIDIOCQCGRETRYERRORS,	VIDIOCQCSRETRYERRORS,	FALSE,	flags_bool },
	{ "compatible", 	VIDIOCQCGCOMPATIBLE,	VIDIOCQCSCOMPATIBLE,	TRUE,	flags_compat },
	{ "video_nr",		VIDIOCQCGVIDEONR,	VIDIOCQCSVIDEONR,	FALSE,	NULL },
};
/* }}} */

/* {{{ [fold] print_cap() */
void print_cap(struct video_capability *vidcap, struct video_window *vidwin, 
				struct video_channel *vidchan,struct video_picture *vidpic) {
	if (vidcap) {
		printf("Name          : %s\n", vidcap->name);
		printf("Type          : ");
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
		printf("Channels      : %i\n", vidcap->channels);
		printf("Audio devices : %i\n", vidcap->audios);
		printf("Maxsize       : %i,%i\n", vidcap->maxwidth,vidcap->maxheight);
		printf("Minsize       : %i,%i\n", vidcap->minwidth,vidcap->minheight);
		printf("\n");
	}
	if (vidwin) {
		printf("Overlay coords: %i,%i\n", (int)vidwin->x, (int)vidwin->y);
		printf("Capture size  : %i,%i\n", vidwin->width, vidwin->height);
		printf("Chromakey     : %i\n", vidwin->chromakey);
		printf("Flags         : ");
		if (vidwin->flags & VIDEO_CAPTURE_ODD)    printf("odd ");
		if (vidwin->flags & VIDEO_CAPTURE_EVEN)   printf("even ");
		printf("\n");
		printf("\n");
	}
	if (vidchan) {
		printf("Channel       : %i\n", vidchan->channel);
		printf("Name          : %s\n", vidchan->name);
		printf("Tuners        : %i\n", vidchan->tuners);
		printf("Flags         : ");
		if (vidchan->flags & VIDEO_VC_TUNER) printf("tuner ");
		if (vidchan->flags & VIDEO_VC_AUDIO) printf("audio ");
#ifdef VIDEO_VC_NORM
		if (vidchan->flags & VIDEO_VC_NORM) printf("norm ");
#endif
		printf("\n");
		printf("Type          : ");
		switch (vidchan->type) {
			case VIDEO_TYPE_TV: printf("tv\n"); break;
			case VIDEO_TYPE_CAMERA: printf("camera\n"); break;
			default: printf("(unknown)\n"); break;
		}
		printf("Norm          : %i\n", vidchan->norm);
		printf("\n");
	}
	if (vidpic) {
		printf("Brightness    : %i\n", vidpic->brightness);
		printf("Hue           : %i\n", vidpic->hue);
		printf("Color         : %i\n", vidpic->colour);
		printf("Contrast      : %i\n", vidpic->contrast);
		printf("Whiteness     : %i\n", vidpic->whiteness);
		printf("Depth         : %i\n", vidpic->depth);
		printf("Palette       : ");
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
			case VIDEO_PALETTE_MJPEG:   printf("Raw MJPEG capture\n");break;
#endif
			default: printf("(unknown)\n"); break;
		}
	}
}
/* }}} */

/* {{{ [fold] read_settings() */
void read_settings(struct video_picture *vidpic) {
	if (vidpic) {
		printf("Brightness      : %i\n", vidpic->brightness);
		printf("Hue             : %i\n", vidpic->hue);
		printf("Color           : %i\n", vidpic->colour);
		printf("Contrast        : %i\n", vidpic->contrast);
		printf("Whiteness       : %i\n", vidpic->whiteness);
		printf("Depth           : %i\n", vidpic->depth);
	}
}
/* }}} */
/* {{{ [fold] write_settings() */
void write_settings(struct video_picture *vidpic) {
	int val, r;
	if (vidpic) {
		r = scanf("Brightness       : %i\n", &val); if (r>0) vidpic->brightness = val;
		r = scanf("Hue              : %i\n", &val); if (r>0) vidpic->hue = val;
		r = scanf("Color            : %i\n", &val); if (r>0) vidpic->colour = val;
		r = scanf("Contrast         : %i\n", &val); if (r>0) vidpic->contrast = val;
		r = scanf("Whiteness        : %i\n", &val); if (r>0) vidpic->whiteness = val;
		r = scanf("Depth            : %i\n", &val); if (r>0) vidpic->depth = val;
	}
}
/* }}} */

/* {{{ [fold] error() */
void error(const char *format, ...) {
	va_list ap;
	int err;
	err = errno;
	va_start(ap, format);
	fprintf(stderr, "%s: ", progname);
	if (format)
		vfprintf(stderr, format, ap);
	else
		fprintf(stderr, "error");
	if (err) fprintf(stderr, " (%s)", strerror(err));
	fprintf(stderr, "\n");
	va_end(ap);
	exit(err);
}
/* }}} */
/* {{{ [fold] help() */
void help(void) {
	int i,j;
	printf(
"%s: Logitech QuickCam driver configurator\n"
"Valid arguments:\n"
"	-h	Display this help\n"
"	-i	Display camera information\n"
"	-r	Display all known registers of camera\n"
"	-a	Display all (including empty) registers of camera\n"
"	-b val	Set brightness (camera exposure time)\n"
"	-u val	Set hue (not supported with all sensors)\n"
"	-o val	Set color strength (increase when setting hue)\n"
"	-c val	Set contrast (camera gain)\n"
"	-w val	Set whiteness (image sharpness when quality=best)\n"
"	-s r	Settings read (from STDIN)\n"
"	-s w	Settings write (to STDOUT)\n"
"	-g v	Enable gamma correction with the given gamma value\n"
"	-g rg:gg:bg\n"
"		Specify different gamma value for each color channel\n"
"	-g rg:gg:bg:rw:gw:bw\n"
"		Specify also white balance (0.55:0.55:0.55:1.0:1.0:1.0)\n"
"	-g +	Enable software lookup-table\n"
"	-e +	Same as '-g +'\n"
"	-g -	Disable software lookup-table\n"
"	-e -	Same as '-g -'\n"
"	-g '?'	Display software lookup-table status and contents\n"
"	-e '?'	Same as '-g '?''\n"
"	-e filename.ppm\n"
"		Generate static equalization based on image in given\n"
"		raw PNM/PPM file.\n"
"The \"val\" values should be between 0...65535 inclusive, 32768 is the default.\n"
"Also module parameters are accepted, e.g. \"qcdebug=15\"\n"
"The module parameters can be also queried, e.g. \"qcdebug?\"\n"
"The parameters can be also given as a symbolic list, e.g. \"qcdebug=logic,user\"\n"
"The device file name (default /dev/video0) can be inserted before the arguments.\n"
	, progname);
	printf("Possible module parameters and symbolic flags:\n");
	for (i=0; i<SIZE(ioctlinfo); i++) {
		printf("\t%s", ioctlinfo[i].name);
		if (ioctlinfo[i].flags!=0) {
			printf(": ");
			for (j=0; ioctlinfo[i].flags[j].name!=NULL; j++) {
				if (j!=0) printf(",");
				printf("%s", ioctlinfo[i].flags[j].name);
			}
		}
		printf("\n");
	}
	exit(0);
}
/* }}} */
/* {{{ [fold] print_regs() */
void print_regs(int fd, int scanall) {
	/* reg encoding: bits 31..16 contain register value, 15..0 the reg number */
	int reg, i, j, r;
	printf("Sensor registers:\n");
	printf("   ");
	for (reg=0; reg<0x10; reg++) printf(" %02X", reg);
	for (reg=0; reg<0x80; reg++) {
		if ((reg&0xF)==0) printf("\n%02X:", reg);
		r = ioctl(fd, VIDIOCQCGI2C, &reg);	if (r != 0) error("ioctl VIDIOCGI2C");
		printf(" %02X", reg >> 16);
		reg &= 0xFFFF;
	}
	printf("\n\n");

	printf("STV registers:\n");
	printf("     ");
	for (reg=0; reg<0x10; reg++) printf(" %02X", reg);
	printf("\n");
	for (i=0; i<0x10000; i+=16) {
		int printed = 0;
		/* Omit time consuming empty ranges */
		if (!scanall && (
			i<0x400 
		    || (i>0x420 && i<0x540)
		    || (i>0x560 && i<0x1400)
		    || (i>0x1700 && i<0xB000)
		    || (i>0xB000 /* && i<0xE000 */ )))
			continue;
		for (j=0; j<16; j++) {
			unsigned int val = i+j;
			r = ioctl(fd, VIDIOCQCGSTV, &val);	/* Returns error for invalid registers */
			if (r==0) {
				val >>= 16;
				if (!printed) {
					int k;
					printf("%4X:", i);
					for (k=0; k<j; k++) printf("   ");
					printed = 1;
				}
				printf(" %02X", val);
			} else if (printed) {
				printf("   ");
			}
		}
		if (printed) printf("\n");
	}
	printf("\n");
}
/* }}} */

FILE *pnm_file = NULL;

/* {{{ [fold] void pnm_nexttoken(void) */
void pnm_nexttoken(void) {
	int c;
	do {
		c = getc(pnm_file); 
		if (c==EOF) error("unexpected end of file");
		if (c=='#') {
			do {
				c = getc(pnm_file);
				if (c==EOF) error("unexpected end of file");
			} while (c!='\n');
			c = ' ';
		}
	} while (isspace(c));
	ungetc(c,pnm_file);
}
/* }}} */
/* {{{ [fold] unsigned int pnm_readnum(void) */
unsigned int pnm_readnum(void) {
	unsigned int c, val = 0;
	pnm_nexttoken();
	do {
		c = getc(pnm_file);
		if (!isdigit(c)) break;
		val = val*10 + c - '0';         /* Assume ASCII */
	} while (1);
	if (!isspace(c)) error("PNM file error");
	ungetc(c,pnm_file);
	return val;
}
/* }}} */
/* {{{ [fold] void pnm_open(unsigned char *name, unsigned int *width, unsigned int *height) */
void pnm_open(char *name, unsigned int *width, unsigned int *height) {
	char c1,c2,c3;
	unsigned int maxval;

	pnm_file = fopen(name, "rb");
	if (!pnm_file) error("can not open file `%s'", name);

	/* Check signature */
	c1 = getc(pnm_file);
	c2 = getc(pnm_file);
	c3 = getc(pnm_file);
	if (c1!='P' || c2!='6' || !isspace(c3)) error("Not raw bits PPM file");
	ungetc(c3,pnm_file);

	/* Read dimensions */
	*width  = pnm_readnum();
	*height = pnm_readnum();
	maxval  = pnm_readnum();
	if (maxval>255) error("Does not support PNM files with more depth than 8 bits");
	c1 = getc(pnm_file);
	if (!isspace(c1)) error("PNM file error");
}
/* }}} */
/* {{{ [fold] void pnm_close(void) */
void pnm_close(void)
{
	fclose(pnm_file);
	pnm_file = NULL;
}
/* }}} */
/* {{{ [fold] void pnm_getpixel(unsigned char *r, unsigned char *g, unsigned char *b) */
void pnm_getpixel(unsigned char *r, unsigned char *g, unsigned char *b)
{
	*r = getc(pnm_file);
	*g = getc(pnm_file);
	*b = getc(pnm_file);
}
/* }}} */

/* {{{ [fold] void compute_lut(unsigned int (*hist)[256], unsigned char (*lut)[256]) */
/* Compute lookup-table `lut' (256 elements) for image equalization
 * based on given image color histogram `hist'. The LUT is also smoothed and normalized. */
void compute_lut(unsigned int (*hist)[256], unsigned char *lut)
{
	static const int len = 64;			/* Smoothing kernel length, must be less than 255 */
	static const double p = 3.0 - 1;		/* Smoothing kernel order */
	unsigned int sum;
	double dlut[256];
	double min, max;
	double norm, v, r;
	int i,j;

	/* Compute lookup-table based on the histogram */
	sum = 0;
	for (i=0; i<256; i++) {
		dlut[i] = sum;
		sum += (*hist)[i];
	}

	/* Normalize lookup-table */
	norm = 255.0 / dlut[255];
	for (i=0; i<256; i++) {
		dlut[i] *= norm;
	}

	/* Compute normalization factor for smoothing kernel */
	norm = 0.0;
	for (j=-len; j<=len; j++) {
		norm += pow(j+len, p) * pow(len-j, p);
	}
	norm = 1.0 / norm;

	/* Smooth the lookup table, handle edges with point-symmetric reflections */
	min = dlut[0];
	max = dlut[255];
	for (i=0; i<256; i++) {
		r = 0.0;
		for (j=-len; j<=len; j++) {
			int x = i + j;
			if (x<0) {
				v = min - dlut[-x];
			} else if (x>255) {
				v = 2*max - dlut[511-x];
			} else {
				v = dlut[x];
			}
			r += v * norm * pow(j+len, p) * pow(len-j, p);
		}
		lut[i] = (unsigned char)CLIP(r+0.5, 0.0, 255.0);
	}
}
/* }}} */

/* {{{ [fold] main() */
int main(int argc, char *argv[])
{
	int fd = 0,i,j,r,l,l2,c;
	char *device = "/dev/video0";		/* Default should not be /dev/video which may be subdir */
	char *argp;
	struct video_capability vidcap;
	struct video_window vidwin;
	struct video_channel vidchan;
	struct video_picture vidpic;

	if (argc<=1) help();
	if (argv && argv[0]) progname = argv[0];
	if (argv && argv[0] && argv[1] && argv[1][0]=='-' && argv[1][1]=='h')
		help();
	while (*++argv) {
		if (argv[0][0]=='/' || argv[0][0]=='.') {
			/* Device filename given */
			if (fd) close(fd);
			device = argv[0];
			fd = 0;
		} else {
			if (fd==0) {
				/* Open the device, if it wasn't yet opened.
				 * Do not try to modify any settings, because if other applications have
				 * already the camera open and are capturing, it might disturb them under 2.6.x */
				fd = open(device,O_RDWR);	if (fd==-1) error("can not open %s",device);
				r = ioctl(fd, VIDIOCGCAP, &vidcap);	if (r != 0) error("ioctl VIDIOCGCAP");
				r = ioctl(fd, VIDIOCGWIN, &vidwin);	if (r != 0) error("ioctl VIDIOCGWIN");
				vidchan.channel = 0;
				r = ioctl(fd, VIDIOCGCHAN, &vidchan);	if (r != 0) error("ioctl VIDIOCGCHAN");
				r = ioctl(fd, VIDIOCGPICT, &vidpic);	if (r != 0) error("ioctl VIDIOCGPICT");
			}
			if (argv[0][0]=='-') {
				/* Option given */
				argp = NULL;
				c = 0;
				if (argv[0][1] && argv[0][2]) {
					argp = &argv[0][2];
				} else {
					if (argv[1]) {
						argp = argv[1];
						c = 1;
					}
				}
				i = argp ? atoi(argp) : 0;
				switch (argv[0][1]) {
				case 'h':	/* Help */
					help();
					/* Never returns */
				case 'i':	/* Information */
					print_cap(&vidcap,&vidwin,&vidchan,&vidpic);
					break;
				case 'r':	/* Dump known chip registers */
					print_regs(fd, 0);
					break;
				case 'a':	/* Dump all chip registers */
					print_regs(fd, 1);
					break;
				case 'b':	/* Brightness */
					if (!argp) error("missing brightness value");
					vidpic.brightness = i;
					r = ioctl(fd, VIDIOCSPICT, &vidpic);
					if (r!=0) error("ioctl VIDIOCSPICT");
					argv += c;
					break;
				case 'u':	/* Hue */
					if (!argp) error("missing hue value");
					vidpic.hue = i;
					r = ioctl(fd, VIDIOCSPICT, &vidpic);
					if (r!=0) error("ioctl VIDIOCSPICT");
					argv += c;
					break;
				case 'o':	/* Color */
					if (!argp) error("missing color value");
					vidpic.colour = i;
					r = ioctl(fd, VIDIOCSPICT, &vidpic);
					if (r!=0) error("ioctl VIDIOCSPICT");
					argv += c;
					break;
				case 'c':	/* Contrast */
					if (!argp) error("missing contrast value");
					vidpic.contrast = i;
					r = ioctl(fd, VIDIOCSPICT, &vidpic);
					if (r!=0) error("ioctl VIDIOCSPICT");
					argv += c;
					break;
				case 'w':	/* Whiteness */
					if (!argp) error("missing whiteness value");
					vidpic.whiteness = i;
					r = ioctl(fd, VIDIOCSPICT, &vidpic);
					if (r!=0) error("ioctl VIDIOCSPICT");
					argv += c;
					break;
				case 's':	/* Settings read/write */
					if (!argp) error("missing parameter to option `-%c'", argv[0][1]);
					switch (argp[0]) {
					case 'r':	/* read */
						r = ioctl(fd, VIDIOCGPICT, &vidpic);
						if (r!=0) error("ioctl VIDIOCGPICT");
						read_settings(&vidpic);
						break;
					case 'w':	/* write */
						r = ioctl(fd, VIDIOCGPICT, &vidpic);
						if (r!=0) error("ioctl VIDIOCGPICT");
						write_settings(&vidpic);		/* Modify only settings which user gives */
						r = ioctl(fd, VIDIOCSPICT, &vidpic);
						if (r!=0) error("ioctl VIDIOCSPICT");
						break;
					default:	/* unknown */
						error("unknown request");
					}
					argv += c;
					break;
				case 'g':
				case 'e': {
					struct qc_userlut userlut;
					memset(&userlut, 0, sizeof(userlut));
					if (!argp) error("missing parameter to option `-%c'", argv[0][1]);
					switch (argp[0]) {
					case '?':		/* Display software lut settings */
						userlut.flags |= QC_USERLUT_VALUES;
						r = ioctl(fd, VIDIOCQCGUSERLUT, &userlut);
						if (r!=0) error("ioctl VIDIOCGUSERLUT");
						printf("Software lookup-table status: %s\n", (userlut.flags & QC_USERLUT_ENABLE) ? "Enabled" : "Disabled");
						printf("\tRed table:\n");
						for (i=0; i<256; i++) {
							printf("%i", userlut.lut[QC_LUT_RED + i]);
							if (i!=255) printf(",");
							if (((i+1) % 20)==0 || i==255) printf("\n");
						}
						printf("\tGreen table:\n");
						for (i=0; i<256; i++) {
							printf("%i", userlut.lut[QC_LUT_GREEN + i]);
							if (i!=255) printf(",");
							if (((i+1) % 20)==0 || i==255) printf("\n");
						}
						printf("\tBlue table:\n");
						for (i=0; i<256; i++) {
							printf("%i", userlut.lut[QC_LUT_BLUE + i]);
							if (i!=255) printf(",");
							if (((i+1) % 20)==0 || i==255) printf("\n");
						}
						break;
					case '+':		/* Enable lookup-table */
						userlut.flags |= QC_USERLUT_ENABLE;
						/* Continue */
					case '-':		/* Disable lookup-table */
						r = ioctl(fd, VIDIOCQCSUSERLUT, &userlut);
						if (r!=0) error("ioctl VIDIOCSUSERLUT");
						break;
					default:
						switch (argv[0][1]) {
						case 'g': {	/* Gamma */
							double gr = 0.55;
							double gg = 0.55;
							double gb = 0.55;
							double wr = 1.0;
							double wg = 1.0;
							double wb = 1.0;
							r = sscanf(argp, "%lg:%lg:%lg:%lg:%lg:%lg", &gr, &gg, &gb, &wr, &wg, &wb);
							if (r!=1 && r!=3 && r!=6) error("bad number of arguments for -g (must be 1, 3, or 6)");
							if (r < 3) {
								gg = gr;
								gb = gr;
							}
							for (i=0; i<256; i++) {
								userlut.lut[QC_LUT_RED   + i] = (unsigned char)CLIP(pow(i/256.0, gr)*256.0*wr+0.5, 0.0, 255.0);
								userlut.lut[QC_LUT_GREEN + i] = (unsigned char)CLIP(pow(i/256.0, gg)*256.0*wg+0.5, 0.0, 255.0);
								userlut.lut[QC_LUT_BLUE  + i] = (unsigned char)CLIP(pow(i/256.0, gb)*256.0*wb+0.5, 0.0, 255.0);
							}
							userlut.flags |= QC_USERLUT_ENABLE | QC_USERLUT_VALUES;
							r = ioctl(fd, VIDIOCQCSUSERLUT, &userlut);
							if (r!=0) error("ioctl VIDIOCSUSERLUT");
						}
						case 'e': {	/* Static equalization */
							unsigned int width, height;
							unsigned char r, g, b;
							unsigned int cnt_red[256], cnt_green[256], cnt_blue[256];
							memset(cnt_red,   0, sizeof(cnt_red));
							memset(cnt_green, 0, sizeof(cnt_green));
							memset(cnt_blue,  0, sizeof(cnt_blue));
							pnm_open(argp, &width, &height);
							/* Compute histograms for each color channel */
							for (i=0; i<height; i++) for (j=0; j<width; j++) {
								pnm_getpixel(&r, &g, &b);
								cnt_red  [r]++;
								cnt_green[g]++;
								cnt_blue [b]++;
							}
							pnm_close();
							/* Compute lookup tables based on the histograms */
							compute_lut(&cnt_red,   &userlut.lut[QC_LUT_RED]);
							compute_lut(&cnt_green, &userlut.lut[QC_LUT_GREEN]);
							compute_lut(&cnt_blue,  &userlut.lut[QC_LUT_BLUE]);
							/* Send the new lookup table to the driver */
							userlut.flags |= QC_USERLUT_ENABLE | QC_USERLUT_VALUES;
							r = ioctl(fd, VIDIOCQCSUSERLUT, &userlut);
							if (r!=0) error("ioctl VIDIOCSUSERLUT");
							break;
						}
						default:
							error("this can't happen");
						}
					}
					argv += c;
					break;
				}
				default:
					error("invalid option");
				}
			} else {
				/* Configuration given */
				for (l=0; argv[0][l]!=0 && argv[0][l]!='=' && argv[0][l]!='?'; l++);
				for (i=0; i<SIZE(ioctlinfo); i++) {
					if (memcmp(ioctlinfo[i].name, argv[0], MIN(l,strlen(ioctlinfo[i].name)))==0)
						break;
				}
				if (i>=SIZE(ioctlinfo)) error("invalid configuration name");
				if (argv[0][l]=='?') {
					/* Display configuration */
					r = ioctl(fd, ioctlinfo[i].get, &c);	if (r != 0) error("ioctl get %s", ioctlinfo[i].name);
					r = 0;		/* Not first flag? */
					if (ioctlinfo[i].flags!=NULL) {
						for (j=0; ioctlinfo[i].flags[j].name!=NULL; j++) {
							l2 = ioctlinfo[i].flags[j].val;
							if (c==l2 || (ioctlinfo[i].bitfield && l2!=0 && (c&l2)==l2)) {
								if (r!=0) printf(",");
								r = 1;
								printf(ioctlinfo[i].flags[j].name);
								if (ioctlinfo[i].bitfield) c &= ~l2;
							}
						}
					}
					if (r==0 || (ioctlinfo[i].bitfield && c!=0)) printf("%i", c);
					printf("\n");
				} else
				if (argv[0][l]=='=') {
					/* Set configuration */
					c = 0;
					while (argv[0][l]!=0) {
						r = 0;		/* Is it a recognized string? */
						for (l2=l+1; argv[0][l2]!=0 && argv[0][l2]!=','; l2++);
						if (ioctlinfo[i].flags!=NULL) {
							for (j=0; ioctlinfo[i].flags[j].name!=NULL; j++) {
								if (strlen(ioctlinfo[i].flags[j].name)==(l2-l-1) && memcmp(ioctlinfo[i].flags[j].name, &argv[0][l+1], l2-l-1)==0) {
									c |= ioctlinfo[i].flags[j].val;
									r = 1;
									break;
								}
							}
						}
						if (r==0) {
							if (!isdigit(argv[0][l+1])) error("unrecognized option parameter");
							c |= atoi(&argv[0][l+1]);
						}
						l = l2;
						if (!ioctlinfo[i].bitfield && argv[0][l]!=0) error("multiple flags given for a non-bitfield option");
					}
					r = ioctl(fd, ioctlinfo[i].set, &c);	if (r != 0) error("ioctl set %s", ioctlinfo[i].name);
					printf("%i\n", c);
				} else {
					error("invalid configuration request type");
				}
			}
		}
	}

	close(fd);
	return 0;
}
/* }}} */

/* End of file */
