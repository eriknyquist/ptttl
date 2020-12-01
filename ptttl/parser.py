import math
import sys

NOTES = {
    "c": 261.625565301,
    "c#": 277.182630977,
    "db": 277.182630977,
    "d": 293.664767918,
    "d#": 311.126983723,
    "eb": 311.126983723,
    "e": 329.627556913,
    "e#": 349.228231433,
    "f": 349.228231433,
    "f#": 369.994422712,
    "gb": 369.994422712,
    "g": 391.995435982,
    "g#": 415.30469758,
    "ab": 415.30469758,
    "a": 440.0,
    "a#": 466.163761518,
    "bb": 466.163761518,
    "b": 493.883301256
}

class PTTTLSyntaxError(Exception):
    """
    Raised by PTTTLParser when ptttl data is malformed and cannot be parsed
    """
    pass

class PTTTLValueError(Exception):
    """
    Raised by PTTTLParser when ptttl data parsing completes, but an invalid
    configuration value or note value was seen.
    """
    pass

def _unrecognised_setting(key):
    raise PTTTLSyntaxError("unrecognised setting '%s' in PTTTL script" % key)

def _missing_setting(key):
    raise PTTTLSyntaxError("missing setting '%s' in PTTTL script" % key)

def _invalid_setting(key):
    raise PTTTLSyntaxError("invalid configuration setting '%s' in PTTTL script"
        % key)

def _invalid_value(key, val):
    raise PTTTLValueError("invalid value '%s' for setting '%s' in PTTTL script"
        % (val, key))

def _invalid_note_duration(note):
    raise PTTTLValueError("invalid note duration '%s'" % note)

def _invalid_note(note):
    raise PTTTLValueError("invalid note '%s'" % note)

def _invalid_octave(note):
    raise PTTTLValueError("invalid octave in note '%s'" % note)

def _int_setting(key, val):
    ret = None

    try:
        ret = int(val)
    except ValueError:
        raise PTTTLValueError("expecting an integer for '%s' setting in "
            "PTTTL script" % key)

    return ret

def _ignore_line(line):
    return (line == "") or line.startswith('!') or line.startswith('#')


class PTTTLNote(object):
    """
    Represents a single musical note, with a pitch and duration

    :ivar float pitch: Note pitch in Hz
    :ivar float duration: Note duration in seconds
    """
    def __init__(self, pitch, duration):
        self.pitch = pitch
        self.duration = duration

    def __str__(self):
        return "%s(pitch=%.4f, duration=%.4f)" % (self.__class__.__name__,
                                                  self.pitch,
                                                  self.duration)

    def __repr__(self):
        return self.__str__()


class PTTTLData(object):
    """
    Represents song data extracted from a PTTTL/RTTTL file.
    May contain multiple tracks, where each track is a list of PTTTLNote objects.

    :ivar [[PTTTLNote]] tracks: List of tracks. Each track is a list of PTTTLNote objects.
    """
    def __init__(self):
        self.tracks = []
    
    def add_track(self, notes):
        self.tracks.append(notes)

    def __str__(self):
        max_display_items = 2

        if len(self.tracks) == 0:
            contents = "[]"
        else:
            items = self.tracks[0][:max_display_items]
            contents = ', '.join([str(x) for x in items])

            if len(self.tracks[0]) > max_display_items:
                contents += ", ..."

        ret = "%s([%s]" % (self.__class__.__name__, contents)

        if len(self.tracks) > 1:
            ret += ", ..."

        return ret + ')'

    def __repr__(self):
        return self.__str__()


class PTTTLParser(object):
    """
    Converts PTTTL/RTTTL source text to a PTTTLData object.
    """
    def _is_valid_octave(self, octave):
        return octave >= 0 and octave <= 8

    def _is_valid_duration(self, duration):
        return duration in [1, 2, 4, 8, 16, 32]

    def _parse_config_line(self, conf):
        # Split comma-separated values
        stripped = [f.strip() for f in conf.split(',')]

        # Remove empty (whitespace-only) values
        values = [f for f in stripped if f != ""]

        if not values:
            raise PTTTLSyntaxError("no valid configuration found in PTTTL script")

        for value in values:
            fields = value.split('=')
            if len(fields) != 2:
                _invalid_setting(value)

            key = fields[0].strip().lower()
            val = fields[1].strip().lower()

            if not key or not val:
                _invalid_setting(value)

            if key == 'b':
                bpm = _int_setting(key, val)
            elif key == 'd':
                default = _int_setting(key, val)
                if not self._is_valid_duration(default):
                    _invalid_value(key, val)
            elif key == 'o':
                octave = _int_setting(key, val)
                if not self._is_valid_octave(octave):
                    _invalid_value(key, val)
            else:
                _unrecognised_setting(key)

        if not octave:
            _missing_setting('o')

        if not bpm:
            _missing_setting('b')

        if not default:
            _missing_setting('d')

        return bpm, default, octave

    def _note_time_to_secs(self, note_time, bpm):
        # Time in seconds for a whole note (4 beats) given current BPM.
        whole = (60.0 / float(bpm)) * 4.0

        return whole / float(note_time)

    def _parse_note(self, string, bpm, default, octave):
        i = 0
        orig = string
        sawdot = False
        dur = default

        if len(string) == 0:
            raise PTTTLSyntaxError("Missing notes after comma")

        while i < len(string) and string[i].isdigit():
            if i > 1:
                _invalid_note_duration(orig)

            i += 1

        if i > 0:
            try:
                dur = int(string[:i])
            except ValueError:
                _invalid_note_duration(orig)
        else:
            if not string[0].isalpha():
                _invalid_note(orig)

        duration = self._note_time_to_secs(dur, bpm)

        string = string[i:]

        i = 0
        while i < len(string) and (string[i].isalpha() or string[i] == '#'):
            i += 1

        note = string[:i].strip().lower()
        if note == "":
            _invalid_note(orig)

        if i < len(string) and string[i] == '.':
            i += 1
            sawdot = True

        string = string[i:].strip()
        i = 0

        if note == 'p':
            pitch = -1

        else:
            if note not in NOTES:
                _invalid_note(orig)

            raw_pitch = NOTES[note]

            while i < len(string) and string[i].isdigit():
                i += 1

            if string[:i] != '':
                try:
                    octave = int(string[:i])
                except ValueError:
                    _invalid_octave(note)

                if not self._is_valid_octave(octave):
                    _invalid_octave(note)

            if octave < 4:
                pitch = raw_pitch / math.pow(2, (4 - octave))
            elif octave > 4:
                pitch = raw_pitch * math.pow(2, (octave - 4))
            else:
                pitch = raw_pitch

        if sawdot or ((i < len(string)) and string[-1] == '.'):
            duration += (duration / 2.0)

        return PTTTLNote(pitch, duration)

    def _parse_notes(self, track_list, bpm, default, octave):
        ret = PTTTLData()

        for track in track_list:
            if track.strip() == "":
                continue

            buf = []
            fields = track.split(',')
            for note in fields:
                note = self._parse_note(note.strip(), bpm, default, octave)
                buf.append(note)

            ret.add_track(buf)

        return ret

    def parse(self, ptttl_string):
        """
        Extracts song data from ptttl/rtttl source data.

        :param str ptttl_string: PTTTL/RTTTL source text.
        :return: Song data extracted from source text.
        :rtype: PTTTLData
        """
        lines = [x.strip() for x in ptttl_string.split('\n')]
        cleaned = ''.join([x for x in lines if not _ignore_line(x)])

        fields = [f.strip() for f in cleaned.split(':')]
        if len(fields) != 3:
            raise PTTTLSyntaxError('expecting 3 colon-seperated fields')

        self.name = fields[0].strip()
        bpm, default, octave = self._parse_config_line(fields[1])

        numtracks = -1
        blocks = fields[2].split(';')
        trackdata = []

        for block in blocks:
            tracks = [x.strip().strip(',') for x in block.split('|')]
            if (numtracks > 0) and (len(tracks) != numtracks):
                raise PTTTLSyntaxError('All blocks must have the same number of'
                    'tracks')

            numtracks = len(tracks)
            trackdata.append(tracks)

        tracks = [''] * numtracks

        for i in range(len(trackdata)):
            for j in range(numtracks):
                tracks[j] += trackdata[i][j]
                if i < (len(trackdata) - 1):
                    tracks[j] += ","

        return self._parse_notes(tracks, bpm, default, octave)
