/*
 * Test quickcam: Logitech QuickCam Express Video Camera driver test tool.
 * Copyright (C) 2001 Nikolas Zimmermann
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
 * <wildfox@kde.org>
 * 
 */

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#include "testquickcam.h"

#define VERSION "$Id: testquickcam.c,v 1.3 2004/07/28 10:13:12 tuukkat Exp $"

int open_camera(const char *devicename)
{
    device_fd = open(devicename, O_RDWR);
    if(device_fd <= 0)
    {
	printf("Device %s couldn't be opened\n", devicename);
	return 0;
    }
    return 1;
}

void close_camera(void)
{
    close(device_fd);
}

void get_camera_info(void)
{
    ioctl(device_fd, VIDIOCGCAP, &vidcap);
    ioctl(device_fd, VIDIOCGWIN, &vidwin);
    ioctl(device_fd, VIDIOCGPICT, &vidpic);
    
    vidwin.clips = vidclips;
    vidwin.clipcount = 0;
}

void print_camera_info(void)
{

    printf("    *** Camera Info ***\n");
    printf("Name:           %s\n", vidcap.name);
    printf("Type:           %d\n", vidcap.type);
    printf("Minimum Width:  %d\n", vidcap.minwidth);
    printf("Maximum Width:  %d\n", vidcap.maxwidth);
    printf("Minimum Height: %d\n", vidcap.minheight);
    printf("Maximum Height: %d\n", vidcap.maxheight);
    printf("X:              %d\n", vidwin.x);
    printf("Y:              %d\n", vidwin.y);
    printf("Width:          %d\n", vidwin.width);
    printf("Height:         %d\n", vidwin.height);
    printf("Depth:          %d\n", vidpic.depth);

    if(vidcap.type & VID_TYPE_MONOCHROME)
	printf("Color           false\n");
    else
	printf("Color           true\n");	
    printf("Version:        %s\n", VERSION);
}

static void hexdump_data(const unsigned char *data, int len)
{
    const int bytes_per_line = 32;
    char tmp[128];
    int i = 0, k = 0;

    for(i = 0; len > 0; i++, len--)
    {
	if(i > 0 && ((i % bytes_per_line) == 0))
	{
    	    printf("%s\n", tmp);
            k = 0;
        }
        if ((i % bytes_per_line) == 0)
    	    k += sprintf(&tmp[k], "[%04x]: ", i);
        k += sprintf(&tmp[k], "%02x ", data[i]);
    }
    
    if (k > 0)

	printf("%s\n", tmp);
}

void read_test(int quiet)
{
    unsigned char *buffer = malloc(vidcap.maxwidth * vidcap.maxheight * 3);
    int len = 0;
    len = read(device_fd, buffer, vidcap.maxwidth * vidcap.maxheight * 3);
    if(!quiet)
    {
	printf(" *** read() test ***\n");
	printf("Read length: %d\n", len);
	printf("Raw data: \n\n");
	hexdump_data(buffer, len);
    }
}

void mmap_test(int quiet)
{
    struct video_mbuf vidbuf;
    struct video_mmap vm;
    int numframe = 0;
    unsigned char *buffer;
    ioctl(device_fd, VIDIOCGMBUF, &vidbuf);
    buffer = mmap(NULL, vidbuf.size, PROT_READ, MAP_SHARED, device_fd, 0);

    vm.format = VIDEO_PALETTE_RGB24;
    vm.frame  = 0;
    vm.width  = 352;
    vm.height = 288;
    ioctl(device_fd, VIDIOCMCAPTURE, &vm);

    ioctl(device_fd, VIDIOCSYNC, &numframe);

    if(!quiet)
    {
	printf(" *** mmap() test ***\n");
	printf("Read length: %d\n", vidbuf.size);
	printf("Raw data: \n\n");
	hexdump_data(buffer, vidbuf.size);
    }
}

void read_loop(void)
{
    int loop = 0;
    while(1)
    {
	loop++;
	read_test(1);
	printf("*** Read frames: %d times!\n", loop);
    }
}

int main(int argc, char *argv[])
{
    char *switchtwo = "";
    char *switchthree = "";
    char *switchfour = "";
    
    if(argc == 1)
    {
	printf(" *** Usage ***\n");
	printf("./testquickcam DEVICE [ -r | -m | -l ]\n\n");
	printf(" -r reads one frame via read() from the camera\n");
	printf(" -m reads one frame via mmap() from the camera\n");
	printf(" -l read() loop...good for debugging gain etc \n");
	exit(1);
    }
    
    if(argc >= 3)
        switchtwo = argv[2];

    if(argc >= 4)
        switchthree = argv[3];
    
    if(open_camera(argv[1]) == 1)
    {
	get_camera_info();
	print_camera_info();
	if(strcmp(switchtwo, "-r") == 0)
	    read_test(0);
	if(strcmp(switchtwo, "-m") == 0)
	    mmap_test(0);
	if(strcmp(switchtwo, "-l") == 0)
	    read_loop();
	if(strcmp(switchthree, "-r") == 0)
	    read_test(0);
	if(strcmp(switchthree, "-m") == 0)
	    mmap_test(0);
	if(strcmp(switchthree, "-l") == 0)
	    read_loop();
	if(strcmp(switchfour, "-r") == 0)
	    read_test(0);
	if(strcmp(switchfour, "-m") == 0)
	    mmap_test(0);
	if(strcmp(switchfour, "-l") == 0)
	    read_loop();
	close_camera();
    }
    exit(1);
}

