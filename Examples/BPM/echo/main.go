package main

import (
	"encoding/binary"
	"fmt"
	"log"
	"math"
	"os"
	sync "sync"
	"time"

	"google.golang.org/protobuf/proto"
)

var (
	//buf = bytes.NewBuffer([]make([]byte, 20000))
	buf = make([]byte, 20000)
)

const RMS = time.Millisecond * 5

func init() {
	log.SetOutput(os.Stderr)
	log.SetFlags(log.Flags() | log.Llongfile)
}

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
	bpm(p)
}

func makeDelay(duration time.Duration, mix float64) func(p *SamplePacket) {
	var sampleRate int
	var bufs [][]float64
	var once sync.Once
	var idx int

	log.Println("hi")

	return func(p *SamplePacket) {
		once.Do(func() {
			sampleRate = int(p.GetSampleRate())
			bufs = make([][]float64, p.GetNChans())
			for i := range bufs {
				bufs[i] = make([]float64, int(duration.Seconds()*float64(sampleRate)))
			}
		})

		var wg sync.WaitGroup
		for cNum, c := range p.Channels {
			wg.Add(1)
			go func(c *SamplePacket_Channel, buf []float64, cidx int) {
				for i, _ := range c.Samples {
					bufidx := (cidx + i) % len(buf)
					t := c.Samples[i]
					//c.Samples[i]
					c.Samples[i] += buf[bufidx] / 2
					buf[bufidx] *= 3
					buf[bufidx] /= 5
					buf[bufidx] += (t / 2)
				}
				wg.Done()
			}(c, bufs[cNum], idx)
		}
		wg.Wait()
		idx += int(p.GetNFrames()) % len(bufs[0])
	}
}

/*
samples = [x0, x1, x2, x3, x4, x5, x6, x7]
buf = [x0, x1, x2, x3]

rms

sampleRate = 100hz
RMS = 1s
number of samples in RMS = 100

sampleRate = 40k
RMS = 1s
number of samples in RMS = 40k

sampleRate = 40k
RMS = 1ms
number of samples in RMS = 40k / 1000 = 40
number of samples in RMS = 40k * (1 / 1000) = 40

num samples in RMS = rate * (rms / second)



*/
var ampBuffer = make([]float64, 1000)
var ampBufferIdx int

func bpm(p *SamplePacket) {
	// get a buffer to hold amplitudes
	buffer := ampBuffer[:int(p.GetSampleRate())*int(RMS/time.Second)]

	for _, c := range p.Channels {
		idx := ampBufferIdx
		for _, s := range c.Samples {
			buffer[idx] = s
		}
	}
}

func getRMS(samples []float64) float64 {
	return math.Sqrt(mean(samples))
}

func mean(xs []float64) float64 {
	if len(xs) == 0 {
		return math.NaN()
	}
	m := 0.0
	for i, x := range xs {
		m += (x - m) / float64(i+1)
	}
	return m
}
