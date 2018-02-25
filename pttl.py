import time
from array import array
from time import sleep

import pygame

from pygame.mixer import Sound, get_init, pre_init

pre_init(44100, -16, 1, 1024)
pygame.init()

FREQ, FMT, _ = get_init()

NOTES = {
    "c": 16.351,
    "c#": 17.324,
    "db": 17.324,
    "d": 18.354,
    "d#": 19.445,
    "eb": 19.445,
    "e": 20.601,
    "f": 21.827,
    "f#": 23.124,
    "gb": 23.124,
    "g": 24.499,
    "g#": 25.956,
    "ab": 25.956,
    "a": 27.5,
    "a#": 29.135,
    "bb": 29.135,
    "b": 30.868
}

class Note(Sound):
    def __init__(self, frequency, duration, volume=.1):
        self.frequency = frequency
        self.duration = duration

        if self.frequency > 0:
            Sound.__init__(self, self.build_samples())
            self.set_volume(volume)

    def build_samples(self):
        period = int(round(FREQ / self.frequency))
        samples = array("h", [0] * period)
        amplitude = 2 ** (abs(FMT) - 1) - 1
        for time in xrange(period):
            if time < period / 2:
                samples[time] = amplitude
            else:
                samples[time] = -amplitude

        return samples

    def __str__(self):
        return '(%s, %s)' % (self.frequency, self.duration)

    def __repr__(self):
        return self.__str__()

def ignore_line(line):
    return (line == "") or line.startswith('#')

def unrecognised_setting(key):
    raise SyntaxError("unrecognised setting '%s' in PTTL script" % key)

def missing_setting(key):
    raise SyntaxError("missing setting '%s' in PTTL script" % key)

def invalid_setting(key):
    raise SyntaxError("invalid configuration setting '%s' in PTTL script"
        % key)

def invalid_value(key, val):
    raise ValueError("invalid value '%s' for setting '%s' in PTTL script"
        % (val, key))

def invalid_note_duration(note):
    raise ValueError("invalid note duration '%s'" % note)

def invalid_note(note):
    raise ValueError("invalid note '%s'" % note)

def invalid_octave(note):
    raise ValueError("invalid note '%s'" % note)

def int_setting(key, val):
    ret = None

    try:
        ret = int(val)
    except ValueError:
        raise ValueError("expecting an integer for '%s' setting in "
            "PTTL script" % key)

    return ret

class PTTLPlayer(object):
    def __init__(self, pttl_string):
        self.notes = []
        self.default = None
        self.octave = None
        self.bpm = None

        if pttl_string:
            self.from_string(pttl_string)

    def _is_valid_octave(self, octave):
        return octave >= 0 and octave <= 8

    def _is_valid_duration(self, duration):
        return duration in [1, 2, 4, 8, 16, 32]

    def _clean_statement(self, stmt):
        return ''.join([x for x in stmt.splitlines() if not ignore_line(x)])

    def _parse_config_line(self, conf):
        line = self._clean_statement(conf)
        # Split comma-separated values
        stripped = [f.strip() for f in line.split(',')]

        # Remove empty (whitespace-only) values
        values = [f for f in stripped if f != ""]

        if not values:
            raise SyntaxError("no valid configuration found in PTTL script")

        for value in values:
            fields = value.split('=')
            if len(fields) != 2:
                invalid_setting(value)

            key = fields[0].strip().lower()
            val = fields[1].strip().lower()

            if not key or not val:
                invalid_setting(value)

            if key == 'b':
                self.bpm = int_setting(key, val)
            elif key == 'd':
                self.default = int_setting(key, val)
                if not self._is_valid_duration(self.default):
                    invalid_value(key, val)
            elif key == 'o':
                self.octave = int_setting(key, val)
                if not self._is_valid_octave(self.octave):
                    invalid_value(key, val)
            else:
                unrecognised_setting(key)
        if not self.bpm:
            missing_setting('b')

        if not self.default:
            missing_setting('d')

    def _note_time_to_secs(self, note_time):
        # Time in seconds for a whole note (4 beats) given current BPM.
        whole = (60.0 / float(self.bpm)) * 4.0
        
        return whole / float(note_time)

    def _parse_note(self, string):
        i = 0
        dur = self.default
        octave = self.octave

        while string[i].isdigit():
            if i > 1:
                invalid_note_duration(string)

            i += 1
        
        if i > 0:
            try:
                dur = int(string[:i])
            except ValueError:
                invalid_note_duration(string)
        
        string = string[i:].lstrip()
        i = 0
        while i < len(string) and (string[i].isalpha() or string[i] == '#'):
            i += 1

        note = string[:i].strip().lower()
        if note == 'p':
            return self._note_time_to_secs(dur), -1

        if note not in NOTES:
            invalid_note(note)

        raw_pitch = NOTES[note]

        if string[i:] != '':
            try:
                octave = int(string[i:])
            except ValueError:
                invalid_octave(string)

            if not self._is_valid_octave(octave):
                invalid_octave(string)
    
        pitch = raw_pitch * (2 ** octave)
        return self._note_time_to_secs(dur), pitch

    def _parse_notes(self, notes_list):
        for raw in notes_list:
            line = self._clean_statement(raw)
            if line.strip() == "":
                continue
            
            buf = []
            fields = line.split(',')
            for note in fields:
                time, pitch = self._parse_note(note.strip())
                buf.append(Note(pitch, time))

            self.notes.append(buf)
    
    def play(self):
        for slot in self.notes:
            slot.sort(key=lambda x: x.duration)
            start = time.time()

            i = 0
            if slot[0].frequency <= 0:
                time.sleep(note.duration)
                continue

            for note in slot:
                note.play(-1)

            while i < len(slot):
                elapsed = time.time() - start

                while i < len(slot) and slot[i].duration <= elapsed:
                    slot[i].stop()
                    i += 1

                if i < len(slot):
                    time.sleep(slot[i].duration - elapsed)

    def from_string(self, pttl_string):
        fields = [f.strip() for f in pttl_string.split(';')]
        self._parse_config_line(fields[0])
        self._parse_notes(fields[1:])

if __name__ == "__main__":
    with open('test_melody.txt', 'r') as fh:
        t = PTTLPlayer(fh.read())

    while True:
        t.play()
