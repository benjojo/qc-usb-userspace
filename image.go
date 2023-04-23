package main

import (
	"encoding/binary"
	"image"
	"image/color"

	"github.com/bamiaux/iobit"
	"github.com/benjojo/qc-usb-userspace/bayer"
)

func (iip *ImageInProgress) ConvertIntoImage() image.Image {
	finalImg := image.NewRGBA(image.Rect(0, 0, 400, 400))
	bitreader := iobit.NewReader(iip.buf.Bytes())
	xLimit, yLimit := 352, 288
	// xLimit, yLimit := 320, 240
	bayerPixlesArray := make([]byte, 0)
	for {
		bayerPixlesArray = append(bayerPixlesArray, bitreader.Uint8(4))
		if bitreader.Error() != nil {
			break
		}
	}

	if len(bayerPixlesArray) < 5000 {
		return finalImg
	}

	nn := bayer.NewNearestNeighbour(bayerPixlesArray, &bayer.Options{
		ByteOrder:    binary.LittleEndian,
		Depth:        8,
		Width:        xLimit,
		Height:       yLimit,
		Pattern:      bayer.GRBG,
		BlackLevel:   0,
		WhiteLevel:   16,
		WhiteBalance: []float64{1, 1, 1},
	})

	for idx, _ := range bayerPixlesArray {
		if idx+3 > len(bayerPixlesArray) {
			break
		}
		x := idx % xLimit
		y := idx / xLimit

		r, g, b := nn.At(x, y)

		finalImg.SetRGBA(x, y, color.RGBA{uint8(r * 255), uint8(g * 255), uint8(b * 255), 255})
	}

	return finalImg
}
