package bayer

type nearestNeighbour struct {
	base
}

// NewNearestNeighbour instanciates a nearest neighbour algorithm to parse the CFA provided as buf.
func NewNearestNeighbour(buf []byte, opts *Options) Bayer {
	return &nearestNeighbour{
		base: base{
			buf:            buf,
			bytesPerPixels: opts.Depth / 8,
			Options:        opts,
		},
	}
}

func (byr *nearestNeighbour) At(x, y int) (r, g, b float64) {
	r = float64(byr.red(x, y))
	g = float64(byr.green(x, y))
	b = float64(byr.blue(x, y))
	return
}

func (byr *nearestNeighbour) red(x, y int) float64 {
	if byr.isGreenR(x, y) {
		return byr.pixel(x+1, y)
	}
	if byr.isGreenB(x, y) {
		return byr.pixel(x, y+1)
	}
	if byr.isBlue(x, y) {
		return byr.pixel(x+1, y+1)
	}
	return byr.pixel(x, y)
}

func (byr *nearestNeighbour) green(x, y int) float64 {
	if byr.isRed(x, y) || byr.isBlue(x, y) {
		return byr.pixel(x+1, y)
	}
	return byr.pixel(x, y)
}

func (byr *nearestNeighbour) blue(x, y int) float64 {
	if byr.isRed(x, y) {
		return byr.pixel(x+1, y+1)
	}
	if byr.isGreenR(x, y) {
		return byr.pixel(x, y+1)
	}
	if byr.isGreenB(x, y) {
		return byr.pixel(x+1, y)
	}
	return byr.pixel(x, y)
}
