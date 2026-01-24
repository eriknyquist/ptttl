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
#   any note or rest. Note and rest lengths that do not match any of these options
#   will be quantized to the nearest valid length, and again this will mess up
#   the timing of your song.
#
# - Any notes playing simultaneously must be on separate tracks in the MIDI file.
#   No notes can overlap within an individual MIDI track.
#
# Erik K. Nyquist 2026

import sys
from mido import MidiFile, tempo2bpm, tick2second

class PtttlNote:
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

    def note_string(self, bpm: int):
        if self.piano_key == 0:
            note = "p"
            octave = ""
        else:
            octave = ((self.piano_key + 20) // 12) - 1
            note_index = (self.piano_key + 8) % len(self.note_octave_map)
            note = self.note_octave_map[note_index]

        # Figure out how many times the note duration fits inside a whole note for the given BPM
        whole_note_ms = (60000.0 / float(bpm)) * 4.0
        whole_note_fraction = whole_note_ms / self.length_ms
        duration_ratio = self.length_ms / whole_note_ms

        duration = None
        dot = False
        smallest_error = float("inf")

        for dur in self.note_durations:
            fraction = 1.0 / float(dur)

            # straight note
            error = abs(duration_ratio - fraction)
            if error < smallest_error:
                smallest_error = error
                duration = dur
                dot = False

            # dotted note
            dotted = fraction * 1.5
            error = abs(duration_ratio - dotted)
            if error < smallest_error:
                smallest_error = error
                duration = dur
                dot = True

        if duration is None:
            raise RuntimeError(f"Unsupported duration (ratio={duration_ratio:.5f}, length={self.length_ms}ms)")

        ret = f"{duration}{note}{octave}"
        if dot:
            ret += "."

        return ret

def find_time_info(mid):
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
    ret = []
    notes_on = {}
    current_tick = 0

    for msg in track:
        current_tick += int(msg.time)
        ms_elapsed = int(tick2second(current_tick, midi.ticks_per_beat, usecs_per_quarternote) * 1000)

        if msg.type == "note_on":
            if msg.note in notes_on:
                raise RuntimeError(f"note_on seen twice in a row without note_off (MIDI note {msg.note})")

            # Figure out if a pause came before this note
            if len(ret) > 0:
                last_note_end = ret[-1].start_ms + ret[-1].length_ms
                if last_note_end < ms_elapsed:
                    ret.append(PtttlNote(0, ms_elapsed - last_note_end, last_note_end))
            else:
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
    mid = MidiFile(midi_filename)
    # Find tempo/time sig. settings
    tempo, time_sig = find_time_info(mid)
    if any(x is None for x in [tempo, time_sig]):
        raise RuntimeError(f"Unable to find time_signature or set_tempo messages types in MIDI file")

    if (time_sig.numerator != 4) or (time_sig.denominator != 4):
        raise RuntimeError(f"Only 4/4 time signature is supported")

    bpm = int(tempo2bpm(tempo.tempo, (4, 4)))

    ret = f"midi_to_pttl.py outpu1Gt :\nb={bpm}, d=4, o=4, f=7, v=10:\n"
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
