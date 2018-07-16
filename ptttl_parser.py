import math
import sys

NOTES = {
    "c": 261.626,
    "c#": 277.183,
    "db": 277.183,
    "d": 293.665,
    "d#": 311.127,
    "eb": 311.127,
    "e": 329.628,
    "e#": 349.228,
    "f": 349.228,
    "f#": 369.994,
    "gb": 369.994,
    "g": 391.995,
    "g#": 415.305,
    "ab": 415.305,
    "a": 440.0,
    "a#": 466.164,
    "bb": 466.164,
    "b": 493.883
}

def unrecognised_setting(key):
    raise SyntaxError("unrecognised setting '%s' in PTTTL script" % key)

def missing_setting(key):
    raise SyntaxError("missing setting '%s' in PTTTL script" % key)

def invalid_setting(key):
    raise SyntaxError("invalid configuration setting '%s' in PTTTL script"
        % key)

def invalid_value(key, val):
    raise ValueError("invalid value '%s' for setting '%s' in PTTTL script"
        % (val, key))

def invalid_note_duration(note):
    raise ValueError("invalid note duration '%s'" % note)

def invalid_note(note):
    raise ValueError("invalid note '%s'" % note)

def invalid_octave(note):
    raise ValueError("invalid octave in note '%s'" % note)

def int_setting(key, val):
    ret = None

    try:
        ret = int(val)
    except ValueError:
        raise ValueError("expecting an integer for '%s' setting in "
            "PTTTL script" % key)

    return ret

def ignore_line(line):
    return (line == "") or line.startswith('#')

class PTTTLParser(object):
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
            raise SyntaxError("no valid configuration found in PTTTL script")

        for value in values:
            fields = value.split('=')
            if len(fields) != 2:
                invalid_setting(value)

            key = fields[0].strip().lower()
            val = fields[1].strip().lower()

            if not key or not val:
                invalid_setting(value)

            if key == 'b':
                bpm = int_setting(key, val)
            elif key == 'd':
                default = int_setting(key, val)
                if not self._is_valid_duration(default):
                    invalid_value(key, val)
            elif key == 'o':
                octave = int_setting(key, val)
                if not self._is_valid_octave(octave):
                    invalid_value(key, val)
            else:
                unrecognised_setting(key)

        if not octave:
            missing_setting('o')

        if not bpm:
            missing_setting('b')

        if not default:
            missing_setting('d')

        return bpm, default, octave

    def _note_time_to_secs(self, note_time, bpm):
        # Time in seconds for a whole note (4 beats) given current BPM.
        whole = (60.0 / float(bpm)) * 4.0

        return whole / float(note_time)

    def _parse_note(self, string, bpm, default, octave):
        i = 0
        sawdot = False
        dur = default

        while string[i].isdigit():
            if i > 1:
                invalid_note_duration(string)

            i += 1

        if i > 0:
            try:
                dur = int(string[:i])
            except ValueError:
                invalid_note_duration(string)

        duration = self._note_time_to_secs(dur, bpm)
        string = string[i:].lstrip()
        i = 0
        while i < len(string) and (string[i].isalpha() or string[i] == '#'):
            i += 1

        note = string[:i].strip().lower()
        if i < len(string) and string[i] == '.':
            i += 1
            sawdot = True

        string = string[i:].strip()
        i = 0

        if note == 'p':
            pitch = -1

        else:
            if note not in NOTES:
                invalid_note(note)

            raw_pitch = NOTES[note]

            while i < len(string) and string[i].isdigit():
                i += 1

            if string[:i] != '':
                try:
                    octave = int(string[:i])
                except ValueError:
                    invalid_octave(note)

                if not self._is_valid_octave(octave):
                    invalid_octave(note)

            if octave < 4:
                pitch = raw_pitch / math.pow(2, (4 - octave))
            elif octave > 4:
                pitch = raw_pitch * math.pow(2, (octave - 4))
            else:
                pitch = raw_pitch

        if sawdot or ((i < len(string)) and string[-1] == '.'):
            duration += (duration / 2.0)

        return duration, pitch

    def _parse_notes(self, notes_list, bpm, default, octave):
        ret = []

        for raw in notes_list:
            line = self._clean_statement(raw)
            if line.strip() == "":
                continue

            buf = []
            fields = line.split('|')
            for note in fields:
                time, pitch = self._parse_note(note.strip(),
                    bpm, default, octave)

                buf.append((pitch, time))

            buf.sort(key=lambda x: x[1])
            ret.append(buf)

        return ret

    def parse(self, pttl_string):
        fields = [f.strip() for f in pttl_string.split(':')]
        if len(fields) != 3:
            raise SyntaxError('expecting 3 colon-seperated fields')

        self.name = fields[0].strip()
        bpm, default, octave = self._parse_config_line(fields[1])
        return self._parse_notes(fields[2].split(','), bpm, default, octave)

if __name__ == "__main__":
    p = PTTTLParser()
    with open(sys.argv[1], 'r') as fh:
        data = fh.read()

    print p.parse(data)