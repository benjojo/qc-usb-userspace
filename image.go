package main

import (
	"image"
	"image/color"
	"log"

	"github.com/bamiaux/iobit"
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
	if yLimit == 100 {
		log.Fatalf("a")
	}

	for idx, _ := range bayerPixlesArray {
		if idx+3 > len(bayerPixlesArray) {
			break
		}
		x := idx % xLimit
		y := idx / xLimit
		// log.Printf("debug %v/%v", x, y)
		finalImg.SetRGBA(x, y, color.RGBA{bayerPixlesArray[idx] * 16, bayerPixlesArray[idx+1] * 16, bayerPixlesArray[idx+2] * 16, 255})
	}
	// x, y := 0, 0
	// for {
	// 	x++
	// 	if x == xLimit {
	// 		x = 0
	// 		y++
	// 	}

	// 	if y == yLimit {
	// 		break
	// 	}
	// 	xy := (x + x*y)
	// 	if xy > len(bayerPixlesArray)-10 {
	// 		break
	// 	}
	// 	finalImg.SetRGBA(x, y, color.RGBA{bayerPixlesArray[xy], bayerPixlesArray[xy+1], bayerPixlesArray[xy+2], 255})

	// }

	return finalImg
}

// // qc_imag_bay2rgb_horip(bayerwin, qc->sensor_data.width,
// // 	dst, 3*qc->vwin.width,
// // 	qc->vwin.width, qc->vwin.height, 3);
// // static inline void qc_imag_bay2rgb_horip(unsigned char *bay, int bay_line,
// // 	unsigned char *rgb, int rgb_line,
// // 	unsigned int columns, unsigned int rows, int bpp)
// // {

// func qc_imag_bay2rgb_horip(bayerBytes []byte, y, x int) {
// 	finalImg := image.NewRGBA(image.Rect(0, 0, 400, 400))

// 	var currentBayerArrayOffset int
// 	var bay_line2, rgb_line2 int
// 	var total_columns int

// 	/* Process 2 lines and rows per each iteration */
// 	total_columns = x / 2 /* columns >> 1 */
// 	total_rows := y / 2   /* rows >>= 1 */
// 	// bay_line2 = 2 * bay_line
// 	// rgb_line2 = 2 * rgb_line
// 	columns := total_columns
// 	rows := total_rows
// 	for rows != 0 {
// 		currentBayerArrayOffset = 0 // 	cur_bay = bay;
// 		// 	cur_rgb = rgb;
// 		columns = total_columns
// 		for columns != 0 {
// 			finalImg.Set(columns, rows, color.RGBA{
// 				R: bayerBytes[currentBayerArrayOffset+1],
// 				G: bayerBytes[currentBayerArrayOffset],
// 				B: bayerBytes[320],
// 				A: 255,
// 			})
// 			currentBayerArrayOffset += 2
// 			// 		qc_imag_writergb(cur_rgb+0,            bpp, cur_bay[1], cur_bay[0],          cur_bay[bay_line]);
// 			// 		qc_imag_writergb(cur_rgb+bpp,          bpp, cur_bay[1], cur_bay[0],          cur_bay[bay_line]);
// 			// 		qc_imag_writergb(cur_rgb+rgb_line,     bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
// 			// 		qc_imag_writergb(cur_rgb+rgb_line+bpp, bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
// 			// 		cur_bay += 2;
// 			// 		cur_rgb += 2*bpp;
// 			columns--
// 		}
// 		// 	bay += bay_line2;
// 		// 	rgb += rgb_line2;
// 		rows--
// 	}
// }

// // static inline void qc_imag_bay2rgb_noip(unsigned char *bay, int bay_line,
// // 	unsigned char *rgb, int rgb_line,
// // 	int columns, int rows, int bpp)
// // {
// // unsigned char *cur_bay, *cur_rgb;
// // int bay_line2, rgb_line2;
// // int total_columns;

// // /* Process 2 lines and rows per each iteration */
// // total_columns = columns >> 1;
// // rows >>= 1;
// // bay_line2 = 2*bay_line;
// // rgb_line2 = 2*rgb_line;
// // do {
// // 	cur_bay = bay;
// // 	cur_rgb = rgb;
// // 	columns = total_columns;
// // 	do {
// // 		qc_imag_writergb(cur_rgb+0,            bpp, cur_bay[1], cur_bay[0],          cur_bay[bay_line]);
// // 		qc_imag_writergb(cur_rgb+bpp,          bpp, cur_bay[1], cur_bay[0],          cur_bay[bay_line]);
// // 		qc_imag_writergb(cur_rgb+rgb_line,     bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
// // 		qc_imag_writergb(cur_rgb+rgb_line+bpp, bpp, cur_bay[1], cur_bay[bay_line+1], cur_bay[bay_line]);
// // 		cur_bay += 2;
// // 		cur_rgb += 2*bpp;
// // 	} while (--columns);
// // 	bay += bay_line2;
// // 	rgb += rgb_line2;
// // } while (--rows);
// // }

// var userlut_contents = []byte{
// 	/* Red */
// 	0, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18,
// 	18, 18, 18, 18, 18, 18, 18, 25, 30, 35, 38, 42,
// 	44, 47, 50, 53, 54, 57, 59, 61, 63, 65, 67, 69,
// 	71, 71, 73, 75, 77, 78, 80, 81, 82, 84, 85, 87,
// 	88, 89, 90, 91, 93, 94, 95, 97, 98, 98, 99, 101,
// 	102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113,
// 	114, 115, 116, 116, 117, 118, 119, 120, 121, 122, 123, 124,
// 	125, 125, 126, 127, 128, 129, 129, 130, 131, 132, 133, 134,
// 	134, 135, 135, 136, 137, 138, 139, 140, 140, 141, 142, 143,
// 	143, 143, 144, 145, 146, 147, 147, 148, 149, 150, 150, 151,
// 	152, 152, 152, 153, 154, 154, 155, 156, 157, 157, 158, 159,
// 	159, 160, 161, 161, 161, 162, 163, 163, 164, 165, 165, 166,
// 	167, 167, 168, 168, 169, 170, 170, 170, 171, 171, 172, 173,
// 	173, 174, 174, 175, 176, 176, 177, 178, 178, 179, 179, 179,
// 	180, 180, 181, 181, 182, 183, 183, 184, 184, 185, 185, 186,
// 	187, 187, 188, 188, 188, 188, 189, 190, 190, 191, 191, 192,
// 	192, 193, 193, 194, 195, 195, 196, 196, 197, 197, 197, 197,
// 	198, 198, 199, 199, 200, 201, 201, 202, 202, 203, 203, 204,
// 	204, 205, 205, 206, 206, 206, 206, 207, 207, 208, 208, 209,
// 	209, 210, 210, 211, 211, 212, 212, 213, 213, 214, 214, 215,
// 	215, 215, 215, 216, 216, 217, 217, 218, 218, 218, 219, 219,
// 	220, 220, 221, 221,

// 	/* Green */
// 	0, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
// 	21, 21, 21, 21, 21, 21, 21, 28, 34, 39, 43, 47,
// 	50, 53, 56, 59, 61, 64, 66, 68, 71, 73, 75, 77,
// 	79, 80, 82, 84, 86, 87, 89, 91, 92, 94, 95, 97,
// 	98, 100, 101, 102, 104, 105, 106, 108, 109, 110, 111, 113,
// 	114, 115, 116, 117, 118, 120, 121, 122, 123, 124, 125, 126,
// 	127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138,
// 	139, 140, 141, 142, 143, 144, 144, 145, 146, 147, 148, 149,
// 	150, 151, 151, 152, 153, 154, 155, 156, 156, 157, 158, 159,
// 	160, 160, 161, 162, 163, 164, 164, 165, 166, 167, 167, 168,
// 	169, 170, 170, 171, 172, 172, 173, 174, 175, 175, 176, 177,
// 	177, 178, 179, 179, 180, 181, 182, 182, 183, 184, 184, 185,
// 	186, 186, 187, 187, 188, 189, 189, 190, 191, 191, 192, 193,
// 	193, 194, 194, 195, 196, 196, 197, 198, 198, 199, 199, 200,
// 	201, 201, 202, 202, 203, 204, 204, 205, 205, 206, 206, 207,
// 	208, 208, 209, 209, 210, 210, 211, 212, 212, 213, 213, 214,
// 	214, 215, 215, 216, 217, 217, 218, 218, 219, 219, 220, 220,
// 	221, 221, 222, 222, 223, 224, 224, 225, 225, 226, 226, 227,
// 	227, 228, 228, 229, 229, 230, 230, 231, 231, 232, 232, 233,
// 	233, 234, 234, 235, 235, 236, 236, 237, 237, 238, 238, 239,
// 	239, 240, 240, 241, 241, 242, 242, 243, 243, 243, 244, 244,
// 	245, 245, 246, 246,

// 	/* Blue */
// 	0, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
// 	23, 23, 23, 23, 23, 23, 23, 30, 37, 42, 47, 51,
// 	55, 58, 61, 64, 67, 70, 72, 74, 78, 80, 82, 84,
// 	86, 88, 90, 92, 94, 95, 97, 100, 101, 103, 104, 106,
// 	107, 110, 111, 112, 114, 115, 116, 118, 119, 121, 122, 124,
// 	125, 126, 127, 128, 129, 132, 133, 134, 135, 136, 137, 138,
// 	139, 140, 141, 143, 144, 145, 146, 147, 148, 149, 150, 151,
// 	152, 154, 155, 156, 157, 158, 158, 159, 160, 161, 162, 163,
// 	165, 166, 166, 167, 168, 169, 170, 171, 171, 172, 173, 174,
// 	176, 176, 177, 178, 179, 180, 180, 181, 182, 183, 183, 184,
// 	185, 187, 187, 188, 189, 189, 190, 191, 192, 192, 193, 194,
// 	194, 195, 196, 196, 198, 199, 200, 200, 201, 202, 202, 203,
// 	204, 204, 205, 205, 206, 207, 207, 209, 210, 210, 211, 212,
// 	212, 213, 213, 214, 215, 215, 216, 217, 217, 218, 218, 220,
// 	221, 221, 222, 222, 223, 224, 224, 225, 225, 226, 226, 227,
// 	228, 228, 229, 229, 231, 231, 232, 233, 233, 234, 234, 235,
// 	235, 236, 236, 237, 238, 238, 239, 239, 240, 240, 242, 242,
// 	243, 243, 244, 244, 245, 246, 246, 247, 247, 248, 248, 249,
// 	249, 250, 250, 251, 251, 253, 253, 254, 254, 255, 255, 255,
// 	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
// 	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
// 	255, 255, 255, 255,
// }
