#!/bin/bash

export AUDIODEV="hw:0,0"

FILE=${1:-"http://aol.streams.bassdrive.com:8012"}

#dd if=/dev/zero \
mpg123 -q -s "${FILE}" \
	| sox -t raw -b 16 -s -r 44100 -c 2 - -f -t raw - rate -v 192k \
		vol 0.9 lowpass -1 16k \
	| ./fmstation 2>rds-stream.raw \
	| sox -t raw -4fr 192k -c 1 - -t alsa -b 16 -s -d rate -v 192k
#	| sox -t raw -4fr 192k -c 1 - fmstereo.flac
