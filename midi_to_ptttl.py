# Converts a multi-track MIDI file to PTTTL. This script has some considerable
# limitations, which correspond to limitations of the PTTTL format:
#
# - Only 4/4 time signature is supported
#
# - 32nd notes are the smallest notes supported-- shorter notes will be extended
#   to 32nd notes, and the timing of notes that follow will not be compensated
#   to account for this, so it will mess up the timing of your song. It's best
#   if the MIDI file doesn't have anything faster than a 32nd note.
#
# - Whole notes, half notes, quarter notes, 8th notes, 16th notes, 32nd notes,
#   and the dotted form of all these notes are the only durations supported for
#   any note or rest. The script will make an attempt to concatenate multiple
#   notes to approximate the actual note/rest length seen in the MIDI file, but
#   again this may mess up the timing of your song.
#
# - Any notes playing simultaneously must be on separate tracks in the MIDI file.
#   No notes can overlap within an individual MIDI track.
#
# Erik K. Nyquist 2026

import sys
from mido import MidiFile, tempo2bpm, tick2second


class PtttlNote:
    """
    Represents a single note or a rest seen in a MIDI track: MIDI note number,
    note length in milliseconds, and note start time (milliseconds since song start)
    """
    note_durations = [1, 2, 4, 8, 16, 32]
    note_octave_map = ["c", "c#", "d", "eb", "e", "f", "f#", "g", "g#", "a", "bb", "b"]

    def __init__(self, midi_note: int = None, length_ms: int = None, start_ms: int = None):
        if (midi_note != 0) and ((midi_note < 21) or (midi_note > 108)):
            raise RuntimeError(f"Invalid MIDI note (midi_note), valid values are 21-108")

        self.midi_note = midi_note
        # Piano key number, 1 through 88. Zero denotes a pause.
        self.piano_key = midi_note - 20 if midi_note > 0 else 0
        # Note length in milliseconds
        self.length_ms = length_ms
        # Note start time, milliseconds since start of song
        self.start_ms = start_ms

    def __str__(self):
        return (f"{self.__class__.__name__}(note={self.piano_key}, "
                f"length_ms={self.length_ms}, start_ms={self.start_ms})")

    def __repr__(self):
        return self.__str__()

    def _calculate_note_durations(self, bpm):
        """
        Returns a list of note durations required to represent the length of
        this note, given the song BPM and note length in milliseconds
        (self.length_ms). Each note duration in the list is a tuple of the form
        (int, bool), where the int is the note duration as a fraction of a whole
        note-- 1, 2, 4, 8, 16 or 32 -- and the bool indicates a dotted note,
        True for dotted, False for not dotted.
        """
        ret = []
        length_ms = self.length_ms
        tolerance_ms = 2
        whole_note_ms = (60000.0 / float(bpm)) * 4.0
        durations = self.note_durations.copy()

        while length_ms >= tolerance_ms:
            whole_note_fraction = whole_note_ms / length_ms
            duration_ratio = length_ms / whole_note_ms

            dot = False
            smallest_error = float("inf")

            for dur in durations:
                fraction = 1.0 / float(dur)

                # straight note
                error = abs(duration_ratio - fraction)
                if error < smallest_error:
                    smallest_error = error
                    duration = dur
                    dot = False

                # dotted note
                dotted = fraction * 1.5
                dotted_length_ms = int(whole_note_ms * dotted)

                # Only consider dotted rhythm if it would be the last note
                if (length_ms - dotted_length_ms) <= tolerance_ms:
                    error = abs(duration_ratio - dotted)
                    if error < smallest_error:
                        smallest_error = error
                        duration = dur
                        dot = True

            curr_length_ms = whole_note_ms * (1.0 / duration)
            if dot:
                curr_length_ms *= 1.5

            if int(curr_length_ms) > length_ms:
                # The current note we've selected is too long, so we need
                # try with shorter notes-- pop the longest note duration off the
                # list and try again
                durations.pop(0)
            else:
                length_ms -= int(curr_length_ms)
                ret.append((duration, dot))

        return ret

    def note_string(self, bpm: int):
        """
        Returns the string that represents this note in PTTTL. May return a string
        that contains multiple comma-separated PTTTL notes, if multiple sequential
        PTTTL notes are required to approximate the duration of this note.
        """
        if self.piano_key == 0:
            note = "p"
            octave = ""
        else:
            octave = ((self.piano_key + 20) // 12) - 1
            note_index = (self.piano_key + 8) % len(self.note_octave_map)
            note = self.note_octave_map[note_index]

        durations = self._calculate_note_durations(bpm)

        notes = []
        for duration, dot in durations:
            n = f"{duration}{note}{octave}"
            if dot:
                n += "."

            notes.append(n)

        return ",".join(notes)

def find_time_info(mid):
    """
    Finds the tempo and time signature meta messages in a mido.MidiFile object
    and returns them as a tuple of the form (tempo, time_sig). 'tempo' or 'time_sig'
    may be None if the corresponding meta message could not be found in the MIDI file.
    """
    tempo = None
    time_sig = None

    for track in mid.tracks:
        for msg in track:
            if msg.is_meta:
                if msg.type == "time_signature":
                    time_sig = msg
                elif msg.type == "set_tempo":
                    tempo = msg

            if not any(x is None for x in [tempo, time_sig]):
                return tempo, time_sig

    return tempo, time_sig

def midi_track_to_ptttl_notes(midi, track, usecs_per_quarternote):
    """
    Processes a mido.MidiFile object and converts it to a two-dimensional list
    of the form:

    [
        [PtttlNote(..), PtttlNote(..), ..],   # First MIDI track
        [PtttlNote(..), PtttlNote(..), ..],   # Second MIDI track
        ..
    ]
    """
    ret = []
    notes_on = {}
    current_tick = 0

    for msg in track:
        current_tick += int(msg.time)
        ms_elapsed = int(tick2second(current_tick, midi.ticks_per_beat, usecs_per_quarternote) * 1000)

        if msg.type == "note_on":
            if msg.note in notes_on:
                raise RuntimeError(f"note_on seen twice in a row without note_off (MIDI note {msg.note})")

            # Figure out if a rest came before this note
            if len(ret) > 0:
                # rest between two notes
                last_note_end = ret[-1].start_ms + ret[-1].length_ms
                if last_note_end < ms_elapsed:
                    ret.append(PtttlNote(0, ms_elapsed - last_note_end, last_note_end))
            else:
                # rest at the beginning of the track
                if ms_elapsed > 0:
                    ret.append(PtttlNote(0, ms_elapsed, 0))

            notes_on[msg.note] = PtttlNote(midi_note=msg.note, start_ms=ms_elapsed)

        elif msg.type == "note_off":
            if msg.note not in notes_on:
                raise RuntimeError(f"note_off seen without preceding note_on (MIDI note {msg.note})")

            new_note = notes_on[msg.note]
            del notes_on[msg.note]
            new_note.length_ms = ms_elapsed - new_note.start_ms

            # Verify new note does not overlap with previous note
            if len(ret) > 0:
                if (ret[-1].start_ms + ret[-1].length_ms) > new_note.start_ms:
                    raise RuntimeError(f"Notes within a MIDI track cannot overlap")

            ret.append(new_note)

    return ret

def midi_to_ptttl(midi_filename: str):
    """
    Processes a mido.MidiFile object and returns the corresponding PTTTL file
    contents as a string
    """
    mid = MidiFile(midi_filename)
    # Find tempo/time sig. settings
    tempo, time_sig = find_time_info(mid)
    if any(x is None for x in [tempo, time_sig]):
        raise RuntimeError(f"Unable to find time_signature or set_tempo messages types in MIDI file")

    if (time_sig.numerator != 4) or (time_sig.denominator != 4):
        raise RuntimeError(f"Only 4/4 time signature is supported")

    bpm = int(tempo2bpm(tempo.tempo, (4, 4)))

    ret = f"midi_to_ptttl.py output :\nb={bpm}, d=4, o=4 :\n"
    tracks = []
    for track in mid.tracks:
        notes = midi_track_to_ptttl_notes(mid, track, tempo.tempo)
        if notes:
            tracks.append(','.join([f"{n.note_string(bpm)}" for n in notes]))

    return ret + "|\n".join(tracks)

def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <MIDI filename>")
        return

    print(midi_to_ptttl(sys.argv[1]))

if __name__ == "__main__":
    main()
