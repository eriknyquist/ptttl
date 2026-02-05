import os
import argparse

from ptttl.parser import PTTTLParser
from ptttl.audio import ptttl_to_wav
from tones import SINE_WAVE, SQUARE_WAVE, TRIANGLE_WAVE, SAWTOOTH_WAVE


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-w', '--wave-type', default='sine', dest='wave_type',
                        choices=['sine', 'square', 'triangle', 'sawtooth'],
                        help="Set the type of waveform to be used")
    parser.add_argument('-f', '--output-file', default='ptttl_audio.wav',
                        dest='output_file', help="Filename for output audio file")
    parser.add_argument('filename')
    args = parser.parse_args()

    if not os.path.exists(args.filename):
        raise IOError("File '%s' does not exist" % args.filename)

    with open(args.filename, 'r') as fh:
        ptttl_data = fh.read()

    wavetype = None
    if args.wave_type == 'sine':
        wavetype = SINE_WAVE
    elif args.wave_type == 'square':
        wavetype = SQUARE_WAVE
    elif args.wave_type == 'triangle':
        wavetype = TRIANGLE_WAVE
    else:
        wavetype = SAWTOOTH_WAVE

    ptttl_to_wav(ptttl_data, args.output_file, 0.5, wavetype)


if __name__ == "__main__":
    main()
