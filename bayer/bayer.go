package bayer

import (
	"encoding/binary"
	"fmt"
)

// Based and adapted/corrected from https://github.com/BryceCicada/demosaic (MIT License)

type (
	// A Bayer allows colors' interpolation from a Color Filter Array data.
	Bayer interface {
		// At returns the RGB pixel.
		At(x, y int) (r, g, b float64)
	}

	// Options contains information for bayer interpolation.
	Options struct {
		// ByteOrder defines the endianness of the CFA.
		ByteOrder binary.ByteOrder
		// Depth defines the BitsPerSample / BitsPerPixel of the CFA.
		Depth int
		// Width defines the width of the CFA.
		Width int
		// Height defines the height of the CFA.
		Height int
		// Pattern defines the CFAPattern (e.g. RGGB, GRBG, etc.).
		Pattern Pattern
		// BlackLevel defines the zero light level.
		BlackLevel float64
		// WhiteLevel defines the saturation light level.
		WhiteLevel float64
		// WhiteBalance defines the AsShotNeutral with inverted values and then rescaled them all so that the green multiplier is 1.
		WhiteBalance []float64
	}

	base struct {
		*Options
		buf            []byte
		bytesPerPixels int
	}

	// A Pattern desribes the orientation of the 4 pixels in the top left corner of the Bayer CFA.
	Pattern int
)

// Patterns of bayer (alignment).
const (
	// ┌───┬───┐
	// │ R │ G │
	// ├───┼───┤
	// │ G │ B │
	// └───┴───┘
	RGGB Pattern = 9
	// ┌───┬───┐
	// │ G │ R │
	// ├───┼───┤
	// │ B │ G │
	// └───┴───┘
	GRBG = 7
	// ┌───┬───┐
	// │ G │ B │
	// ├───┼───┤
	// │ R │ G │
	// └───┴───┘
	GBRG = 5
	// ┌───┬───┐
	// │ B │ G │
	// ├───┼───┤
	// │ G │ R │
	// └───┴───┘
	BGGR = 3
)

// GetPattern returns the bayer pattern contant.
func GetPattern(cfaPattern []uint) (Pattern, error) {
	p := 0
	for i, v := range cfaPattern {
		p += i * int(v)
	}

	pattern := Pattern(p)
	switch pattern {
	case RGGB:
		fallthrough
	case GRBG:
		fallthrough
	case GBRG:
		fallthrough
	case BGGR:
		return pattern, nil
	default:
		return pattern, fmt.Errorf("bayer: unsupported CFAPattern %v", cfaPattern)
	}
}

func (b base) reflect(x, p1, p2 int) (r int) {
	if x >= p1 && x <= p2 {
		r = x
	} else if x < p1 {
		r = p1 + abs(p1-x)
	} else {
		r = p2 - abs(p2-x)
	}
	return
}

func (b base) read(n int) (c float64) {
	switch b.Depth {
	case 16:
		c = float64(b.ByteOrder.Uint16(b.buf[n : n+2]))
	default:
		c = float64(b.buf[n]) // default: 8 bits depth
	}
	return (c - b.BlackLevel) / (b.WhiteLevel - b.BlackLevel) // Rescale/Linearize value to range [0,1]
}

func (b base) pixel(x, y int) float64 {
	X := b.reflect(x, 0, b.Width-1)
	Y := b.reflect(y, 0, b.Height-1)
	switch {
	case b.isRed(X, Y):
		return b.read(X*b.bytesPerPixels+Y*b.Width*b.bytesPerPixels) * b.WhiteBalance[0]
	case b.isGreenR(X, Y) || b.isGreenB(X, Y):
		return b.read(X*b.bytesPerPixels+Y*b.Width*b.bytesPerPixels) * b.WhiteBalance[1]
	case b.isBlue(X, Y):
		return b.read(X*b.bytesPerPixels+Y*b.Width*b.bytesPerPixels) * b.WhiteBalance[2]
	default:
		panic("Something went wrong")
	}
}

func (b base) isRed(x, y int) bool {
	switch b.Pattern {
	case RGGB:
		return x%2 == 0 && y%2 == 0
	case GRBG:
		return x%2 == 1 && y%2 == 0
	case GBRG:
		return x%2 == 0 && y%2 == 1
	case BGGR:
		return x%2 == 1 && y%2 == 1
	}

	return false
}

func (b base) isGreenR(x, y int) bool {
	switch b.Pattern {
	case RGGB:
		return x%2 == 1 && y%2 == 0
	case GRBG:
		return x%2 == 0 && y%2 == 0
	case GBRG:
		return x%2 == 1 && y%2 == 1
	case BGGR:
		return x%2 == 0 && y%2 == 1
	}

	return false
}

func (b base) isGreenB(x, y int) bool {
	switch b.Pattern {
	case RGGB:
		return x%2 == 0 && y%2 == 1
	case GRBG:
		return x%2 == 1 && y%2 == 1
	case GBRG:
		return x%2 == 0 && y%2 == 0
	case BGGR:
		return x%2 == 1 && y%2 == 0
	}

	return false
}

func (b base) isBlue(x, y int) bool {
	switch b.Pattern {
	case RGGB:
		return x%2 == 1 && y%2 == 1
	case GRBG:
		return x%2 == 0 && y%2 == 1
	case GBRG:
		return x%2 == 1 && y%2 == 0
	case BGGR:
		return x%2 == 0 && y%2 == 0
	}

	return false
}

func abs(n int) int {
	if n < 0 {
		return -n
	}
	return n
}
