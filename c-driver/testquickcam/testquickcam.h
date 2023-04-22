/*
 * Logitech QuickCam Express Video Camera driver
 *
 * tiny test program
 *
 * (C) Copyright 2001 Nikolas Zimmermann <wildfox@kde.org>
 */

#include <linux/videodev.h>

static int device_fd;
static struct video_capability vidcap;
static struct video_window vidwin;
static struct video_picture vidpic;
static struct video_clip vidclips[32];
