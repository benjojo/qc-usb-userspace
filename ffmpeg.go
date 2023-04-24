package main

import (
	"bytes"
	"flag"
	"image"
	"image/jpeg"
	"io"
	"os"
	"os/exec"
)

var (
	outputV4LoopbackDev = flag.String("device", "/dev/video0", "The V4L2Loopback Device you want to use")
)

func ffmpegSendToV4L2(frames chan image.Image) {
	// ffmpeg -f mjpeg -i - -pix_fmt yuv420p -f v4l2 /dev/video0
	ffmpegPid := exec.Command("ffmpeg", "-f", "mjpeg", "-i", "pipe:0", "-pix_fmt", "yuv420p", "-f", "v4l2", *outputV4LoopbackDev)
	inputImages, _ := ffmpegPid.StdinPipe()
	outputVideo, _ := ffmpegPid.StdoutPipe()
	ffmpegPid.Stderr = os.Stderr
	ffmpegPid.Start()

	videoBuf := bytes.Buffer{}
	outputClosed := make(chan bool)
	go func() {
		io.Copy(&videoBuf, outputVideo)
		outputClosed <- true
		ffmpegPid.Wait()
	}()

	// f, _ := os.Create("debug.mjpeg")
	for img := range frames {
		buf := bytes.NewBuffer(nil)
		jpeg.Encode(buf, img, nil)

		fin := []byte("\n--myboundary\nContent-Type: image/jpeg\n\n")
		fin = append(fin, buf.Bytes()...)
		inputImages.Write(fin)
		// f.Write(fin)
	}

	inputImages.Close()
	<-outputClosed

	// MP4 should be in videoBuf
	return
}
