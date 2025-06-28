import sys
import os
import wave
import math
import struct
import subprocess
import tempfile

from ptttl.parser import PTTTLParser, PTTTLData

from tones.mixer import Mixer
from tones import SINE_WAVE

SAMPLE_RATE = 44100
MP3_BITRATE = 128
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

def _generate_samples(parsed, amplitude, wavetype):
    mixer = Mixer(SAMPLE_RATE, amplitude)
    numchannels = 0

    for i in range(len(parsed.tracks)):
        mixer.create_track(i, wavetype=wavetype, attack=0.01, decay=0.01)

    for i in range(len(parsed.tracks)):
        for note in parsed.tracks[i]:
            if note.pitch <= 0.0:
                mixer.add_silence(i, duration=note.duration)
            else:
                mixer.add_tone(i, frequency=note.pitch, duration=note.duration,
                               vibrato_frequency=note.vibrato_frequency,
                               vibrato_variance=note.vibrato_variance)

    return mixer.mix()

def _generate_wav_file(parsed, amplitude, wavetype, filename):
    sampledata = _generate_samples(parsed, amplitude, wavetype).serialize()
    Mixer(SAMPLE_RATE, amplitude).write_wav(filename, sampledata)

def ptttl_to_samples(ptttl_data, amplitude=0.5, wavetype=SINE_WAVE):
    """
    Convert a PTTTLData object to a list of audio samples.

    :param PTTTLData ptttl_data: PTTTL/RTTTL source text
    :param float amplitude: Output signal amplitude, between 0.0 and 1.0.
    :param int wavetype: Waveform type for output signal. Must be one of\
        tones.SINE_WAVE, tones.SQUARE_WAVE, tones.TRIANGLE_WAVE, or tones.SAWTOOTH_WAVE.
    :return: list of audio samples
    :rtype: tones.tone.Samples
    """
    parser = PTTTLParser()
    data = parser.parse(ptttl_data)
    return _generate_samples(data, amplitude, wavetype)

def ptttl_to_wav_samples(ptttl_data, amplitude=0.5, wavetype=SINE_WAVE):
    """
    Convert a PTTTLData object to a list of audio samples, packed into string
    and ready for writing to .wav files.

    :param str ptttl_data: PTTTL/RTTTL source text
    :param float amplitude: Output signal amplitude, between 0.0 and 1.0.
    :param int wavetype: Waveform type for output signal. Must be one of\
        tones.SINE_WAVE, tones.SQUARE_WAVE, tones.TRIANGLE_WAVE, or tones.SAWTOOTH_WAVE.
    :return: list of audio samples
    :rtype: str
    """
    return ptttl_to_samples(data, amplitude, wavetype).serialize()

def ptttl_to_wav(ptttl_data, wav_filename, amplitude=0.5, wavetype=SINE_WAVE):
    """
    Convert a PTTTLData object to audio data and write it to a .wav file.

    :param str ptttl_data: PTTTL/RTTTL source text
    :param str wav_filename: Filename for output .wav file
    :param float amplitude: Output signal amplitude, between 0.0 and 1.0.
    :param int wavetype: Waveform type for output signal. Must be one of\
        tones.SINE_WAVE, tones.SQUARE_WAVE, tones.TRIANGLE_WAVE, or tones.SAWTOOTH_WAVE.
    """
    parser = PTTTLParser()
    data = parser.parse(ptttl_data)
    samples = _generate_wav_file(data, amplitude, wavetype, wav_filename)

def ptttl_to_mp3(ptttl_data, mp3_filename, amplitude=0.5, wavetype=SINE_WAVE):
    """
    Convert a PTTTLData object to audio data and write it to an .mp3 file (requires
    the LAME audio mp3 encoder to be installed and in your system path).

    :param str ptttl_data: PTTTL/RTTTL source text
    :param str mp3_filename: Filename for output .mp3 file
    :param float amplitude: Output signal amplitude, between 0.0 and 1.0.
    :param int wavetype: Waveform type for output signal. Must be one of\
        tones.SINE_WAVE, tones.SQUARE_WAVE, tones.TRIANGLE_WAVE, or tones.SAWTOOTH_WAVE.
    """
    fd, wavfile = tempfile.mkstemp()
    ptttl_to_wav(ptttl_data, wavfile, amplitude, wavetype)
    _wav_to_mp3(wavfile, mp3_filename)
    os.close(fd)
    os.remove(wavfile)
