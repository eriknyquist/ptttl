import sys
import os
import wave
import math
import struct
import subprocess
import tempfile

from ptttl_parser import PTTTLParser

sys.path.insert(0, 'tones')
import tones
from tones.mixer import Mixer

SAMPLE_RATE = 44100
MP3_BITRATE = 128
LAME_BIN = 'lame'

SINE_WAVE = tones.SINE_WAVE
SQUARE_WAVE = tones.SQUARE_WAVE

def _wav_to_mp3(infile, outfile):
    args = [LAME_BIN, '--silent', '-b', str(MP3_BITRATE), infile, outfile]

    try:
        ret = subprocess.call(args)
    except OSError as e:
        os.remove(infile)
        raise OSError("Unable to run %s. Is %s installed?"
            % (LAME_BIN, LAME_BIN))

    if ret != 0:
        os.remove(infile)
        raise OSError("Error (%d) returned by lame" % ret)

def _generate_samples(parsed, amplitude, wavetype):
    if wavetype not in [tones.SINE_WAVE, tones.SQUARE_WAVE]:
        raise ValueError("Invalid wave type '%s'" % wavetype)

    mixer = Mixer(SAMPLE_RATE, amplitude)
    numchannels = 0

    for i in range(len(parsed)):
        mixer.create_track(i, wavetype=wavetype, attack=0.01, decay=0.01)

    for i in range(len(parsed)):
        for pitch, time in parsed[i]:
            if pitch <= 0.0:
                mixer.add_silence(i, duration=time)
            else:
                mixer.add_tone(i, frequency=pitch, duration=time)

    return mixer.mix()

def _generate_wav_file(parsed, amplitude, wavetype, filename):
    sampledata = _generate_samples(parsed, amplitude, wavetype).serialize()
    Mixer(SAMPLE_RATE, amplitude).write_wav(filename, sampledata)

def ptttl_to_samples(ptttl_data, amplitude=0.5, wavetype=tones.SINE_WAVE):
    parser = PTTTLParser()
    data = parser.parse(ptttl_data)
    return _generate_samples(data, amplitude, wavetype)

def ptttl_to_wav_samples(ptttl_data, amplitude=0.5, wavetype=tones.SINE_WAVE):
    return ptttl_to_samples(data, amplitude, wavetype).serialize()

def ptttl_to_wav(ptttl_data, wav_filename, amplitude=0.5, wavetype=tones.SINE_WAVE):
    parser = PTTTLParser()
    data = parser.parse(ptttl_data)
    samples = _generate_wav_file(data, amplitude, wavetype, wav_filename)

def ptttl_to_mp3(ptttl_data, mp3_filename, amplitude=0.5, wavetype=tones.SINE_WAVE):
    fd, wavfile = tempfile.mkstemp()
    ptttl_to_wav(ptttl_data, wavfile, amplitude, wavetype)
    _wav_to_mp3(wavfile, mp3_filename)
    os.close(fd)
    os.remove(wavfile)

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: %s <input.rtttl/.ptttl> <output.wav>" % sys.argv[0])
        sys.exit(1)

    if not os.path.exists(sys.argv[1]):
        raise IOError("File '%s' does not exist" % sys.argv[1])

    with open(sys.argv[1], 'r') as fh:
        ptttl_data = fh.read()

    ptttl_to_wav(ptttl_data, sys.argv[2], 0.5, tones.SINE_WAVE)
