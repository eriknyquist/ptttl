import sys
import os
import wave
import math
import struct
import subprocess
import tempfile

from ptttl_parser import PTTTLParser

# Wave types
SINE_WAVE = 0
SQUARE_WAVE = 1

MAX_AMPLITUDE = 0.9
NUM_CHANNELS = 1
DATA_SIZE = 2
SAMPLE_RATE = 44100
MP3_BITRATE = 128
MAX_SAMPLE_VALUE = float(int((2 ** (DATA_SIZE * 8)) / 2) - 1)

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

def _sine_wave_table(freq, rate, period, num, amp):
    return [_gen_sample(amp, freq, period, rate, i) for i in range(period)]

def _square_wave_table(freq, rate, period, num, amp):
    ret = []
    for s in _sine_wave_table(freq, rate, period, num ,amp):
        ret.append(amp if s > 0 else -amp)

    return ret

def _gen_wave(tablefunc, freq, rate, num, amp):
    period = int(rate / freq)
    if amp > MAX_AMPLITUDE: amp = MAX_AMPLITUDE
    if amp < 0.0: amp = 0.0
    period = int(rate / freq)
    table = tablefunc(freq, rate, period, num, amp)
    return [table[i % period] for i in range(num)]

def _sine_wave(freq=440.0, rate=44100, num=44100, amp=0.5):
    return _gen_wave(_sine_wave_table, freq, rate, num, amp)

def _square_wave(freq=440.0, rate=44100, num=44100, amp=0.5):
    return _gen_wave(_square_wave_table, freq, rate, num, amp)

def _serialize_samples(samples):
    return bytes(b'').join([bytes(struct.pack('h', int(s * MAX_SAMPLE_VALUE))) for s in samples])

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
        amp += 0.005

def _generate_samples(parsed, amplitude, wavetype):
    if wavetype == SINE_WAVE:
        wavegen = _sine_wave
    elif wavetype == SQUARE_WAVE:
        wavegen = _square_wave
    else:
        raise ValueError("Invalid wave type '%s'" % wavetype)

    ret = [0 for _ in range(int(SAMPLE_RATE / 4.0))]

    for slot in parsed:
        tones = []

        for pitch, time in slot:
            numsamples = int(float(SAMPLE_RATE) * time)
            if pitch > 0.0:
                tone = wavegen(pitch, SAMPLE_RATE, numsamples, amplitude/len(slot))

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

def ptttl_to_wav(ptttl_data, wav_filename, amplitude=0.5, wavetype=SINE_WAVE):
    parser = PTTTLParser()
    data = parser.parse(ptttl_data)
    
    samples = _generate_samples(data, amplitude, wavetype)
    _write_wav_file(samples, wav_filename)

def ptttl_to_mp3(ptttl_data, mp3_filename, amplitude=0.5, wavetype=SINE_WAVE):
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

    ptttl_to_mp3(ptttl_data, sys.argv[2], 10, SQUARE_WAVE)
