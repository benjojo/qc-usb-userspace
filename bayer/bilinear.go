package bayer

type bilinear struct {
	base
}

// NewBilinear instanciates a bilinear interpolation algorithm to parse the CFA provided as buf.
func NewBilinear(buf []byte, opts *Options) Bayer {
	return &bilinear{
		base: base{
			buf:            buf,
			bytesPerPixels: opts.Depth / 8,
			Options:        opts,
		},
	}
}

func (byr *bilinear) At(x, y int) (r, g, b float64) {
	r = float64(byr.red(x, y))
	g = float64(byr.green(x, y))
	b = float64(byr.blue(x, y))
	return
}

func (byr *bilinear) red(x, y int) float64 {
	if byr.isGreenR(x, y) {
		return (byr.pixel(x-1, y) + byr.pixel(x+1, y)) / 2
	}
	if byr.isGreenB(x, y) {
		return (byr.pixel(x, y+1) + byr.pixel(x, y+1)) / 2
	}
	if byr.isBlue(x, y) {
		return (byr.pixel(x-1, y-1) + byr.pixel(x-1, y+1) + byr.pixel(x+1, y-1) + byr.pixel(x+1, y+1)) / 4
	}
	return byr.pixel(x, y)
}

func (byr *bilinear) green(x, y int) float64 {
	if byr.isRed(x, y) || byr.isBlue(x, y) {
		return (byr.pixel(x, y-1) + byr.pixel(x, y+1) + byr.pixel(x-1, y) + byr.pixel(x+1, y)) / 4
	}
	return byr.pixel(x, y)
}

func (byr *bilinear) blue(x, y int) float64 {
	if byr.isRed(x, y) {
		return (byr.pixel(x-1, y-1) + byr.pixel(x-1, y+1) + byr.pixel(x+1, y-1) + byr.pixel(x+1, y+1)) / 4
	}
	if byr.isGreenR(x, y) {
		return (byr.pixel(x, y-1) + byr.pixel(x, y+1)) / 2
	}
	if byr.isGreenB(x, y) {
		return (byr.pixel(x-1, y) + byr.pixel(x+1, y)) / 2
	}
	return byr.pixel(x, y)
}
