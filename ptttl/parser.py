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


DEFAULT_VIBRATO_FREQ_HZ = 7.0
DEFAULT_VIBRATO_VAR_HZ = 20.0
DEFAULT_OCTAVE = 4
DEFAULT_DURATION = 8
DEFAULT_BPM = 123


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

def _invalid_vibrato(vdata):
    raise PTTTLValueError("invalid vibrato settings: '%s'" % note)


def _int_setting(key, val):
    ret = None

    try:
        ret = int(val)
    except ValueError:
        raise PTTTLValueError("expecting an integer for '%s' setting in "
                              "PTTTL script" % key)

    return ret

def _float_setting(key, val):
    ret = None

    try:
        ret = float(val)
    except ValueError:
        raise PTTTLValueError("expecting a floating point value for '%s' setting in "
                              "PTTTL script" % key)

    return ret

def _ignore_line(line):
    return (line == "") or line.startswith('!') or line.startswith('#')


class PTTTLNote(object):
    """
    Represents a single musical note, with a pitch and duration

    :ivar float pitch: Note pitch in Hz
    :ivar float duration: Note duration in seconds
    :ivar float vfreq: Vibrato frequency in Hz
    :ivar float vvar: Vibrato variance from main pitch in Hz
    """
    def __init__(self, pitch, duration, vfreq=None, vvar=None):
        self.pitch = pitch
        self.duration = duration
        self.vibrato_frequency = vfreq
        self.vibrato_variance = vvar

    def has_vibrato(self):
        """
        Returns True if vibrato frequency and variance are non-zero

        :return: True if vibrato is non-zero
        :rtype: bool
        """
        if None in [self.vibrato_frequency, self.vibrato_variance]:
            return False

        if 0.0 in [self.vibrato_frequency, self.vibrato_variance]:
            return False

        return (self.vibrato_frequency > 0.0) and (self.vibrato_variance > 0.0)

    def __str__(self):
        ret = "%s(pitch=%.4f, duration=%.4f" % (self.__class__.__name__,
                                                self.pitch,
                                                self.duration)
        if self.has_vibrato():
            ret += ", vibrato=%.1f:%.1f" % (self.vibrato_frequency, self.vibrato_variance)

        return ret + ")"

    def __repr__(self):
        return self.__str__()


class PTTTLData(object):
    """
    Represents song data extracted from a PTTTL/RTTTL file.
    May contain multiple tracks, where each track is a list of PTTTLNote objects.

    :ivar [[PTTTLNote]] tracks: List of tracks. Each track is a list of PTTTLNote objects.
    :ivar float bpm: playback speed in BPM (beats per minute).
    :ivar int default_octave: Default octave to use when none is specified
    :ivar int default_duration: Default note duration to use when none is specified
    :ivar float default_vibrato_freq: Default vibrato frequency when none is specified, in Hz
    :ivar float default_vibrato_var: Default vibrato variance when none is specified, in Hz
    """
    def __init__(self, bpm=DEFAULT_BPM, default_octave=DEFAULT_OCTAVE,
                 default_duration=DEFAULT_DURATION, default_vibrato_freq=DEFAULT_VIBRATO_FREQ_HZ,
                 default_vibrato_var=DEFAULT_VIBRATO_VAR_HZ):
        self.bpm = bpm
        self.default_octave = default_octave
        self.default_duration = default_duration
        self.default_vibrato_freq = default_vibrato_freq
        self.default_vibrato_var = default_vibrato_var
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

        bpm = None
        default = None
        octave = None
        vfreq = None
        vvar = None

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
            elif key == 'f':
                vfreq = _float_setting(key, val)
            elif key == 'v':
                vvar = _float_setting(key, val)
            else:
                _unrecognised_setting(key)

        if not octave:
            _missing_setting('o')

        if not bpm:
            _missing_setting('b')

        if not default:
            _missing_setting('d')

        return bpm, default, octave, vfreq, vvar

    def _note_time_to_secs(self, note_time, bpm):
        # Time in seconds for a whole note (4 beats) given current BPM.
        whole = (60.0 / float(bpm)) * 4.0

        return whole / float(note_time)

    def _parse_note(self, string, bpm, default, octave, vfreq, vvar):
        i = 0
        orig = string
        sawdot = False
        dur = default
        vibrato_freq = None
        vibrato_var = None
        vdata = None

        fields = string.split('v')
        if len(fields) == 2:
            string = fields[0]
            vdata = fields[1]

        if len(string) == 0:
            raise PTTTLSyntaxError("Missing notes after comma")

        # Get the note duration; if there is one, it should be the first thing
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

        # Calculate note duration in real seconds
        duration = self._note_time_to_secs(dur, bpm)

        # Now, look for the musical note value, which should be next
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
            # This note is a rest
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

            string = string[i:].strip()
            i = 0

        if sawdot or ((i < len(string)) and string[-1] == '.'):
            duration += (duration / 2.0)

        if vdata is not None:
            if vdata.strip() == '':
                vibrato_freq = vfreq
                vibrato_var = vvar
            else:
                fields = vdata.split('-')
                if len(fields) == 2:
                    try:
                        vibrato_freq = float(fields[0])
                        vibrato_var = float(fields[1])
                    except:
                        _invalid_vibrato(vdata)

                elif len(fields) == 1:
                    try:
                        vibrato_freq = float(vdata)
                    except:
                        _invalid_vibrato(vdata)

                    vibrato_var = vvar

        return PTTTLNote(pitch, duration, vibrato_freq, vibrato_var)

    def _parse_notes(self, track_list, bpm, default, octave, vfreq, vvar):
        ret = PTTTLData(bpm, octave, default, vfreq, vvar)

        for track in track_list:
            if track.strip() == "":
                continue

            buf = []
            fields = track.split(',')
            for note in fields:
                note = self._parse_note(note.strip(), bpm, default, octave, vfreq, vvar)
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
        bpm, default, octave, vfreq, vvar = self._parse_config_line(fields[1])

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

        if vfreq is None:
            vfreq = DEFAULT_VIBRATO_FREQ_HZ
        if vvar is None:
            vvar = DEFAULT_VIBRATO_VAR_HZ

        return self._parse_notes(tracks, bpm, default, octave, vfreq, vvar)
