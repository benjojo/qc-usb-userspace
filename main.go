package main

import (
	"bytes"
	"encoding/binary"
	"flag"
	"io"
	"log"
	"sync"

	"github.com/google/gousb"
)

var debugLogging = flag.Bool("debug", false, "Enable debug output")

func main() {
	flag.Parse()

	ctx := gousb.NewContext()
	defer ctx.Close()

	logIfDebug("Open Device")
	// idVendor=046d, idProduct=0870,
	dev, err := ctx.OpenDeviceWithVIDPID(0x046d, 0x0870)
	if err != nil || dev == nil {
		log.Fatalf("Could not open a device: %v", err)
	}

	deviceConfig, err := dev.Config(1)
	if err != nil {
		log.Printf("Failed to get USB config for device: %v", err)
		return
	}

	// quikCamOutput := gousb.Config{
	// 	Desc: gousb.ConfigDesc{
	// 		Number:       1,
	// 		SelfPowered:  false,
	// 		RemoteWakeup: false,
	// 		MaxPower:     0x5a,
	// 		Interfaces: []gousb.InterfaceDesc{
	// 			gousb.InterfaceDesc{
	// 				Number: 0,
	// 				AltSettings: []gousb.InterfaceSetting{
	// 					gousb.InterfaceSetting{
	// 						Number:    0,
	// 						Alternate: 0,
	// 						Class:     0xff,
	// 						SubClass:  0xff,
	// 						Protocol:  0xff,
	// 						Endpoints: map[gousb.EndpointAddress]gousb.EndpointDesc{
	// 							0x81: gousb.EndpointDesc{Address: 0x81,
	// 								Number:        1,
	// 								Direction:     true,
	// 								MaxPacketSize: 0,
	// 								TransferType:  0x1,
	// 								PollInterval:  1000000,
	// 								IsoSyncType:   0x0,
	// 								UsageType:     0x1,
	// 							},
	// 							0x82: gousb.EndpointDesc{Address: 0x82,
	// 								Number:        2,
	// 								Direction:     true,
	// 								MaxPacketSize: 1,
	// 								TransferType:  0x3,
	// 								PollInterval:  16000000,
	// 								IsoSyncType:   0x0,
	// 								UsageType:     0x0,
	// 							},
	// 						} /*, iInterface: 0*/},
	// 					gousb.InterfaceSetting{
	// 						Number: 0,
	// 						Alternate: 1,
	// 						Class:     0xff,
	// 						SubClass:  0xff,
	// 						Protocol:  0xff,
	// 						Endpoints: map[gousb.EndpointAddress]gousb.EndpointDesc{
	// 							0x81: gousb.EndpointDesc{Address: 0x81, Number: 1, Direction: true, MaxPacketSize: 1023, TransferType: 0x1, PollInterval: 1000000, IsoSyncType: 0x0, UsageType: 0x1},
	// 							0x82: gousb.EndpointDesc{Address: 0x82, Number: 2, Direction: true, MaxPacketSize: 1, TransferType: 0x3, PollInterval: 16000000, IsoSyncType: 0x0, UsageType: 0x0},
	// 						} /*, iInterface: 0*/},
	// 				},
	// 			},
	// 		} /*, iConfiguration: 0*/},
	// }
	logIfDebug("Device Config %#v", *deviceConfig)

	USBInterface, err := deviceConfig.Interface(0, 1)
	if err != nil {
		log.Fatalf("cannot grab control interface %v", err)
	}

	logIfDebug("Device Interfaces \n%#v", *USBInterface)
	// exampleInterface :=
	// 	gousb.Interface{Setting: gousb.InterfaceSetting{
	// 		Number:    0,
	// 		Alternate: 0,
	// 		Class:     0xff,
	// 		SubClass:  0xff,
	// 		Protocol:  0xff,
	// 		Endpoints: map[gousb.EndpointAddress]gousb.EndpointDesc{
	// 			0x81: gousb.EndpointDesc{
	// 				Address:       0x81,
	// 				Number:        1,
	// 				Direction:     true,
	// 				MaxPacketSize: 0,
	// 				TransferType:  0x1,
	// 				PollInterval:  1000000,
	// 				IsoSyncType:   0x0,
	// 				UsageType:     0x1},
	// 			0x82: gousb.EndpointDesc{Address: 0x82,
	// 				Number:        2,
	// 				Direction:     true,
	// 				MaxPacketSize: 1,
	// 				TransferType:  0x3,
	// 				PollInterval:  16000000,
	// 				IsoSyncType:   0x0,
	// 				UsageType:     0x0},
	// 		} /*, iInterface: 0*/},
	// 	/*config: (*gousb.Config)(0xc00013c640)*/}

	// dev.Control()

	isoData, err := USBInterface.InEndpoint(1)
	if err != nil {
		log.Fatalf("cannot setup USBInterface.InEndpoint %v", err)
	}
	logIfDebug("Debug %v", isoData.Desc.TransferType)

	for _, v := range setupParams {
		dev.Control(v.rType, v.request, v.val, v.idx, v.data)
	}

	PacketStreamBuffer := make([]byte, 128000)
	usbIsoDatabuf := bytes.NewBuffer(PacketStreamBuffer)
	MsgPoker := make(chan bool, 1)
	bufLock := sync.Mutex{}
	go func() {
		rs, err := isoData.NewStream(1023*10, 10)
		if err != nil {
			log.Fatalf("cannot setup isoData.NewStream %v", err)
		}
		n := 0
		for {
			aaa := make([]byte, 65000)
			bn, err := rs.Read(aaa)
			if err != nil {
				log.Fatalf("?? %v", err)
			}
			n++
			// logIfDebug("Read (%v) %#v (%v)", n, aaa[:5], bn)
			bufLock.Lock()
			_, err = usbIsoDatabuf.Write(aaa[:bn])
			bufLock.Unlock()

			if err != nil {
				log.Fatalf("PacketStreamBuffer ran out of buffer")
			}
			if n%10 == 0 {
				select {
				case MsgPoker <- true:
				default:
				}
			}

		}
	}()

	CurrentImage := ImageInProgress{}

	for {
		<-MsgPoker
		for {
			bufLock.Lock()
			var msgType, msgLen uint16
			err := binary.Read(usbIsoDatabuf, binary.BigEndian, &msgType)
			if err != nil {
				log.Printf("ooprs %v", err)
			}
			binary.Read(usbIsoDatabuf, binary.BigEndian, &msgLen)
			if msgType != 0 {
				// log.Printf("%#v %#v (%v)", msgType, msgLen, usbIsoDatabuf.Len())
			}
			msgBytes := make([]byte, msgLen)
			io.ReadFull(usbIsoDatabuf, msgBytes)
			bufLock.Unlock()

			switch msgType {
			case 0x8001, 0x8005, 0xC001, 0xC005: // New frame
				logIfDebug("New Frame setup")
			case 0x8002, 0x8006, 0xC002, 0xC006: // End of Frame
				logIfDebug("End Of Frame, Frame was %v bytes", CurrentImage.Size())
				CurrentImage = ImageInProgress{}
			case 0x0200, 0x4200:
				CurrentImage.AddImageData(msgBytes)
			default:
				if msgType != 0 {
					logIfDebug("Unknow msg %#v", msgType)
				}
			}

			if usbIsoDatabuf.Len() < 1000 {
				break
			}
		}
	}
	// deviceConfig.Desc
}

type ImageInProgress struct {
	buf *bytes.Buffer
}

func (iip *ImageInProgress) AddImageData(b []byte) {
	if iip.buf == nil {
		iip.buf = bytes.NewBuffer(nil)
	}
	iip.buf.Write(b)
}

func (iip *ImageInProgress) Size() int {
	if iip.buf == nil {
		return 0
	}
	return iip.buf.Len()
}

func logIfDebug(format string, args ...interface{}) {
	if *debugLogging {
		log.Printf(format, args...)
	}
}

type URB_CONTROL_SETUP struct {
	request uint8
	val     uint16
	idx     uint16
	rType   uint8
	data    []byte
}

var setupParams = []URB_CONTROL_SETUP{
	{4, 0x0423, 0, 0x40, []byte{0x4}},  //  1,
	{4, 0x1500, 0, 0x40, []byte{0x1d}}, //  1,
	{4, 0x0400, 0, 0x40, []byte{
		0x06, 0x08, 0x36, 0xf7, 0x2c, 0x7d, 0xfd, 0x85, 0x00, 0x15, 0x00, 0x00, 0x10, 0x0a, 0x42, 0xf7,
		0x63, 0x00, 0x08, 0x00, 0x40, 0xc9, 0x1c, 0x86, 0x8f, 0xa4, 0xc0, 0xf7, 0x2c, 0x7d, 0xfd, 0x85,
		0xaa, 0x02, 0x01}}, //  35,
	{4, 0x15c3, 0, 0x40, []byte{0x2}},        //  1,
	{4, 0x15c1, 0, 0x40, []byte{0x35, 0x03}}, //  2,
	{4, 0x0400, 0, 0x40, []byte{
		0x14, 0x16, 0x18, 0x1a, 0x00, 0x00, 0x00, 0x00, 0x2c, 0x7d, 0xfd, 0x85, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x07, 0x49, 0x5e, 0x2c, 0x7d, 0xfd, 0x85, 0xc1, 0x15, 0x00, 0x00, 0x00, 0x0a, 0x42, 0xf7,
		0xaa, 0x03, 0x01}}, //  35,
	{4, 0x1680, 0, 0x40, []byte{0x00}}, //  1,
	{4, 0x0400, 0, 0x40, []byte{
		0x1c, 0x0a, 0x0c, 0x32, 0x40, 0xc9, 0x1c, 0x86, 0x00, 0x00, 0x00, 0x00, 0x14, 0x0a, 0x42, 0xf7,
		0xc7, 0x00, 0x16, 0x00, 0x04, 0x53, 0xfd, 0x85, 0x80, 0x16, 0x00, 0x00, 0x00, 0x0a, 0x42, 0xf7,
		0xaa, 0x03, 0x01,
	}}, //  35,
	{4, 0x1446, 0, 0x40, []byte{0x00}}, //  1,
	{4, 0x1446, 0, 0x40, []byte{}},     //  1,
	{4, 0x1446, 0, 0x40, []byte{0x00}}, //  1,
	{4, 0x0400, 0, 0x40, []byte{
		0x26, 0x28, 0x2a, 0x00, 0x00, 0x0a, 0x42, 0xf7, 0xaa, 0x03, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00,
		0xb0, 0x00, 0x25, 0x00, 0x00, 0x92, 0xe8, 0x87, 0x00, 0x97, 0x01, 0x00, 0xff, 0x00, 0x00, 0x00,
		0xaa, 0x02, 0x01}}, //  35,
	{4, 0x1501, 0, 0x40, []byte{0xb6}}, //  1,
	{4, 0x1502, 0, 0x40, []byte{0xa8}}, //  1,
	{4, 0x0400, 0, 0x40, []byte{
		0x06, 0x08, 0x36, 0xf7, 0x0d, 0xca, 0x96, 0xf6, 0x00, 0x00, 0x00, 0x00, 0x04, 0x53, 0xfd, 0x85,
		0x63, 0x00, 0x08, 0x86, 0x00, 0x00, 0x00, 0x00, 0xf4, 0x09, 0x42, 0xf7, 0xed, 0x9e, 0x96, 0xf6,
		0xaa, 0x02, 0x01}}, //  35,
	{4, 0x0400, 0, 0x40, []byte{
		0x38, 0x9e, 0x96, 0xf6, 0x54, 0x86, 0xfd, 0x85, 0x47, 0xa8, 0xc0, 0xf7, 0x2c, 0x7d, 0xfd, 0x85,
		0x04, 0x15, 0x00, 0x00, 0x03, 0x0a, 0x42, 0xf7, 0x01, 0x00, 0x00, 0x00, 0x2c, 0x7d, 0xfd, 0x85,
		0xaa, 0x00, 0x01}}, //  35,
	{4, 0x1440, 0, 0x40, []byte{1}}, //  1,
	{4, 0x0400, 0, 0x40, []byte{
		0x1e, 0x20, 0x22, 0x24, 0x48, 0x82, 0x20, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x31, 0x31, 0x31, 0x31, 0xd0, 0xbc, 0xc8, 0xf7, 0xfb, 0x54, 0x95, 0xf6, 0xcc, 0xbd, 0xc8, 0xf7,
		0xaa, 0x03, 0x01}}, //  35,
}
