Polyphonic Tone Transfer Language
#################################

The Polyphonic Tone Transfer Language (PTTL) is a way to describe polyphonic
melodies, and is based on Nokia's
`RTTTL <https://en.wikipedia.org/wiki/Ring_Tone_Transfer_Language>`_ format,
which was used for transferring monophonic ringtones.

PTTL consists of a string divided into "statements" by a semicolon ``;``.

*default values* statement
==========================

The very first statement is the *default value* section and is identical to
the section of the same name from the RTTTL format, with the exception that it
must end with a semicolon.

::

  b=123, d=8, o=4;

* *d* - duration: default duration of a note if none is specified
* *o* - octave: default octave of a note if none is specified
* *b* - beat, tempo: tempo in BPM (Beats Per Minute)

ALl of the remaining statements after the initial *default values* statement
are *time slot* statements

*time slot* statements
======================

A time slot statement contains multiple comma-seperated notes. The format of
each note is identical to that described by the RTTTL format: each note
includes, in sequence: a duration specifier, a standard music note, either a, b,
c, d, e, f or g (optionally followed by '#' or 'b' for sharps and flats), and an
octave specifier. If no duration or octave specifier are present, the default
applies.

Durations
---------

Valid values for note duration:

* **1** - whole note
* **2** - half note
* **4** - quarter note
* **8** - eighth note
* **16** - sixteenth note
* **32** - thirty-second note

Notes
-----

Valid values for note pitch (non case-sensitive):

* **P** - rest or pause
* **A**
* **A#** / **Bb**
* **B** / **Cb**
* **C**
* **C#** / **Db**
* **D**
* **D#** / **Eb**
* **E** / **Fb**
* **F** / **E#**
* **F#** / **Gb**
* **G**
* **G#** / **Ab**

Octave
------

Valid values for note octave are between **0** and **8**.

Each note in a time slot statement starts playing simultaneously, and each
will stop when its respective duration has elapsed. The current time slot
ends when the duration of the longest note has elapsed. A time slot can last
for a single measure, at most.

Example
=======

Consider the following PTTL string:

::

    # 123 beats-per-minute, default quarter note, default 4th octave
    b=123, d=4, o=4;

    16c, 16e, 16g5;


This would play 3 sixteenth notes simultaneously; C (octave 4), E (octave 4)
and G (octave 5).


