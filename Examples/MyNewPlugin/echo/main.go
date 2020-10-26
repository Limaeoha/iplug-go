package main

import (
	"encoding/binary"
	"fmt"
	"os"

	"google.golang.org/protobuf/proto"
)

var (
	//buf = bytes.NewBuffer([]make([]byte, 20000))
	buf = make([]byte, 20000)
)

var p SamplePacket

func main() {
	for {
		var size uint64
		if err := binary.Read(os.Stdin, binary.LittleEndian, &size); err != nil {
			panic(err)
		}

		var cur int
		for uint64(cur) < size {
			fmt.Fprintf(os.Stderr, "filling buffer. cur: %d; size: %d\n", cur, size)
			n, err := os.Stdin.Read(buf[cur:int(size)])
			if err != nil {
				panic("error reading from stdin:" + err.Error())
			}
			cur += n
		}

		if err := proto.Unmarshal(buf[:size], &p); err != nil {
			panic(err)
		}

		process(&p)

		bb, err := proto.Marshal(&p)
		if err != nil {
			panic(err)
		}

		if err := binary.Write(os.Stdout, binary.LittleEndian, int64(len(bb))); err != nil {
			panic(err)
		}

		if n, err := os.Stdout.Write(bb); err != nil {
			fmt.Fprintf(os.Stderr, "error writing to stdout: %v", err)
		} else {
			fmt.Fprintf(os.Stderr, "wrote %d bytes successfully\n", n)
		}
	}
}

func process(p *SamplePacket) {
	gain(p, 1.2)
}

func gain(p *SamplePacket, amp float64) {
	for _, c := range p.Channels {
		for i, s := range c.Samples {
			c.Samples[i] = s * amp
		}
	}
}
