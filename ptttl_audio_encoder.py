import sys
import os
import wave
import math
import struct
import subprocess
import tempfile

from ptttl_parser import PTTTLParser

NUM_CHANNELS = 1
DATA_SIZE = 2
SAMPLE_RATE = 44100
AMPLITUDE = 0.5
MP3_BITRATE = 128
MAX_AMP = float(int((2 ** (DATA_SIZE * 8)) / 2) - 1)

LAME_BIN = 'lame'

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

def _gen_sample(amp, freq, period, rate, i):
    return float(amp) * math.sin(2.0 * math.pi * float(freq)
        * (float(i % period) / float(rate)))

def _sine_wave(freq=440.0, rate=44100, num=44100, amp=0.5):
    period = int(rate / freq)
    if amp > 1.0: amp = 1.0
    if amp < 0.0: amp = 0.0
    table = [_gen_sample(amp, freq, period, rate, i) for i in xrange(period)]
    return [table[i % period] for i in range(num)]

def _serialize_samples(samples):
    return ''.join([struct.pack('h', s * MAX_AMP) for s in samples])

def _write_wav_file(samples, filename):
    f = wave.open(filename, 'w')
    f.setparams((NUM_CHANNELS, DATA_SIZE, SAMPLE_RATE, len(samples),
        "NONE", "Uncompressed"))
    f.writeframesraw(_serialize_samples(samples))
    f.close()

def _fade_up(data, start, end, step=1):
    amp = 0.0
    for i in range(start, end, -1):
        if amp >= 1.0:
            break

        data[i] *= amp
        amp += 0.01

def _generate_samples(parsed):
    ret = [0 for _ in range(int(SAMPLE_RATE / 4.0))]

    for slot in parsed:
        tones = []

        for pitch, time in slot:
            numsamples = int(float(SAMPLE_RATE) * time)
            if pitch > 0.0:
                tone = _sine_wave(pitch, SAMPLE_RATE, numsamples,
                    AMPLITUDE/len(slot))

                _fade_up(tone, len(tone) - 1, -1, -1)
                _fade_up(tone, 0, len(tone))
                tones.append(tone)
            else:
                tones.append([0 for _ in range(numsamples)])

        tones = sorted(tones, key=len)[::-1]
        mixed = tones[0]
        for tone in tones[1:]:
            amp = 0.0
            for i in range(len(tone)):
                mixed[i] += tone[i]

        ret.extend(mixed)

    return ret + [0 for _ in range(int(SAMPLE_RATE / 4.0))]

def ptttl_to_wav(ptttl_filename, wav_filename):
    if not os.path.exists(ptttl_filename):
        raise IOError("File '%s' does not exist" % ptttl_filename)

    parser = PTTTLParser()

    with open(ptttl_filename, 'r') as fh:
        data = parser.parse(fh.read())
    
    samples = _generate_samples(data)
    _write_wav_file(samples, wav_filename)

def ptttl_to_mp3(ptttl_filename, mp3_filename):
    fd, wavfile = tempfile.mkstemp()
    ptttl_to_wav(ptttl_filename, wavfile)
    _wav_to_mp3(wavfile, mp3_filename)
    os.close(fd)
    os.remove(wavfile)

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print "Usage: %s <input.rtttl/.ptttl> <output.wav>" % sys.argv[0]
        sys.exit(1)

    ptttl_to_mp3(sys.argv[1], sys.argv[2])
