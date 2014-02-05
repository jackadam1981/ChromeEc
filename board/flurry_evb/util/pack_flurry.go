
package main

import "io/ioutil"
import "encoding/binary"
import "log"
import "flag"

var infile = flag.String("infile", "./build/flurry_evb/ec.RO.flat", "Input image")
var outfile = flag.String("outfile", "./build/flurry_evb/ec.bin", "Output image")

func main() {
	flag.Parse()

	var image [0x800000]byte

	// Read image
	flat, err := ioutil.ReadFile(*infile)
	if err != nil {
		log.Fatal("Could not read image.")
	}

	// Image descriptor
	binary.LittleEndian.PutUint32(image[0x7fff00:], 0x80)
	binary.LittleEndian.PutUint32(image[0x7fff04:], 0xffffff7f)

	// Image header
	binary.LittleEndian.PutUint32(image[0x80:], 0x524c5453)
	binary.LittleEndian.PutUint32(image[0x84:], 0x2)
	binary.LittleEndian.PutUint32(image[0x88:], uint32(len(flat)))

	// Copy binary image into packed image
	copy(image[0xc0:], []byte(flat))

	// Write packed image
	ioutil.WriteFile(*outfile, image[:], 0777)
}
