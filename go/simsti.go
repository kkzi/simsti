package main

import (
	"bufio"
	"encoding/binary"
	"errors"
	"flag"
	"fmt"
	"log"
	"net"
	"os"
	"sync/atomic"
	"time"
)

var STI_HEAD uint32 = 1234567890
var STI_TAIL uint32 = -STI_HEAD

type Options struct {
	input        string
	framelen     int
	fps          int
	tm_channel   int
	port         int
	time_code    int
	has_sti_head bool
}

var opts Options

func main() {
	log.SetFlags(log.Flags() | log.LUTC | log.Lshortfile)

	if err := check_args(); err != nil {
		log.Fatal(err)
		return
	}

	log.Printf("listen at %d, channel %d, time code %d\n", opts.port, opts.tm_channel, opts.time_code)
	if sock, err := net.Listen("tcp", fmt.Sprintf(":%v", opts.port)); err == nil {
		for {
			if conn, err := sock.Accept(); err == nil {
				go process_connection(conn)
			} else {
				log.Println(err)
			}
		}
	} else {
		log.Fatalln(err)
	}
}

func check_args() error {
	flag.StringVar(&opts.input, "input", "", "[required] input file path")
	flag.IntVar(&opts.framelen, "framelen", 0, "[required] frame length")
	flag.IntVar(&opts.fps, "fps", 2, "frame per seconds, default 2")
	flag.IntVar(&opts.tm_channel, "channel", 0, "tm channel, default 0")
	flag.IntVar(&opts.port, "port", 3070, "tm channel listening port, default 3070")
	flag.IntVar(&opts.time_code, "timecode", 0, "time code (0/3), default 0")

	flag.Parse()

	if len(opts.input) == 0 {
		return errors.New("argument `input` required")
	}
	if file, err := os.Open(opts.input); err != nil {
		return err
	} else {
		bytes4 := make([]byte, 4)
		file.Read(bytes4)
		opts.has_sti_head = binary.BigEndian.Uint32(bytes4) == STI_HEAD
		file.Close()
	}

	if opts.framelen <= 0 {
		return errors.New("argument `framelen` cannot <= 0")
	}
	if opts.fps <= 0 {
		return errors.New("argument `fps` cannot <= 0")
	}
	if opts.time_code != 0 && opts.time_code != 3 {
		return errors.New(fmt.Sprintf("unsupported argument `time_code` %v", opts.time_code))
	}
	return nil
}

func process_connection(conn net.Conn) {
	log.Printf("new connection from %v\n", conn.RemoteAddr().String())

	var interrupted atomic.Bool
	defer func() {
		interrupted.Store(true)
		log.Printf("connection closed %v\n", conn.RemoteAddr().String())
		conn.Close()
	}()

	reader := bufio.NewReader(conn)
	writer := bufio.NewWriter(conn)
	read_buffer := make([]byte, 64)
	for {
		n, err := reader.Read(read_buffer)
		if err != nil {
			log.Println(err)
			return
		}
		if n < 64 {
			log.Fatalf("received unsupported frame: % x", read_buffer[0:n])
			return
		}
		if head := binary.BigEndian.Uint32(read_buffer); head != 1234567890 {
			log.Fatalf("received unsupported frame: % x", read_buffer[0:n])
			return
		}
		switch data_flow := binary.BigEndian.Uint32(read_buffer[20:]); data_flow {
		case 0, 1, 2, 4, 5, 6:
			go generate_tm_frame(writer, &interrupted)
		default:
			log.Printf("invalid data flow %x\n", data_flow)
			fallthrough
		case 0x80:
			break
		}
	}
}

func generate_tm_frame(writer *bufio.Writer, interrupted *atomic.Bool) {
	const UNIT = 4

	t0 := time.Now().UnixMicro()
	last := t0
	var count uint32 = 0
	gap := int64(1e6) / int64(opts.fps)
	// flush_ps := uint32(math.Ceil(float64(opts.fps) / 20.0))

	file, err := os.Open(opts.input)
	if err != nil {
		log.Fatal(err)
		return
	}

	frame := make([]byte, opts.framelen)
	for !interrupted.Load() {
		now := time.Now().UnixMicro()
		if now-last < gap {
			continue
		}

		if n, err := file.Read(frame); err != nil || n != opts.framelen {
			file.Seek(0, 0)
		} else {
			if opts.has_sti_head {
				writer.Write(frame)
			} else {
				field0, field1 := make_time_tag(opts.time_code)
				copied := make([]byte, opts.framelen+68)
				binary.BigEndian.PutUint32(copied[0:], STI_HEAD)
				binary.BigEndian.PutUint32(copied[1*UNIT:], uint32(len(copied)))
				binary.BigEndian.PutUint32(copied[3*UNIT:], field0)
				binary.BigEndian.PutUint32(copied[4*UNIT:], field1)
				binary.BigEndian.PutUint32(copied[5*UNIT:], count)
				binary.BigEndian.PutUint32(copied[10*UNIT:], uint32(n))
				copy(copied[64:], frame)
				binary.BigEndian.PutUint32(copied[64+opts.framelen:], STI_TAIL)

				writer.Write(copied)
			}
			count += 1
		}

		// if count%flush_ps == 0 {
		// writer.Flush()
		// }

		last = now
	}
	if last > t0 {
		log.Printf("fps=%.2f\n", float64(count)*1e6/float64(last-t0))
	}
}

func make_time_tag(tag int) (uint32, uint32) {
	unit := func() uint32 {
		if tag == 3 {
			return 1000 * 1000
		}
		return 1000
	}()
	now := time.Now()

	f0 := uint32((((now.YearDay()-1)*24+now.Hour())*60+now.Minute())*60 + now.Second())
	f1 := uint32(now.Nanosecond()) / unit

	return f0, f1
}
