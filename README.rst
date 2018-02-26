Polyphonic Tone Transfer Language
#################################

The Polyphonic Tone Transfer Language (PTTL) is a way to describe polyphonic
melodies, and is a superset of Nokia's
`RTTTL <https://en.wikipedia.org/wiki/Ring_Tone_Transfer_Language>`_ format,
used for monophonic ringtones.

Why?
####

I needed a good way to store simple tones and melodies for some project.
RTTTL looked pretty good but only works for monophonic melodies.
I needed polyphony.

PTTL format
###########

Valid RTTTL strings are also valid PTTL strings. A parser that properly handles
PTTL can also handle RTTTL.

A PTTL string is made up of three colon-seperated sections; **name** section,
**default values** section, and **data** section.

Whitespace characters, empty lines, and lines beginning with a "#" character
are ignored.

The initial "name" section is intended to contain the name of the ringtone
in the original RTTTL format. PTTL requires this field to be present, to
maintain backwards compatibility with RTTTL, but places no constraints on its
contents.

*default values* section
========================

The very first statement is the *default value* section and is identical to
the section of the same name from the RTTTL format.

::

  b=123, d=8, o=4

* *b* - beat, tempo: tempo in BPM (Beats Per Minute)
* *d* - duration: default duration of a note if none is specified
* *o* - octave: default octave of a note if none is specified

*data* section
==============

Like RTTTL, the data section in a PTTL string contains comma-seperated values.
*Unlike* RTTTL, where only single notes are separated by commas, the comma
seperated values in PTTL are *time slots*, where each time slot may contain one
or more notes. Multiple notes in a time slot are separated by a vertical pipe
character "|".

Each note in a time slot starts playing simultaneously, and each
will stop when its respective duration has elapsed. The current time slot
ends when the duration of the longest note has elapsed. A time slot can last
for the length of a single measure (4 beats), at most.

The format of a note is identical to that described by the RTTTL format. Each
note includes, in sequence; a duration specifier, a standard music note, either
a, b, c, d, e, f or g (optionally followed by '#' or 'b' for sharps and flats),
and an octave specifier. If no duration or octave specifier are present, the
default applies.

Durations
---------

Valid values for note duration:

* **1** - whole note
* **2** - half note
* **4** - quarter note
* **8** - eighth note
* **16** - sixteenth note
* **32** - thirty-second note

Dotted rhythm patterns can be formed by adding a period "." either
after the note letter (e.g. ``c#.``, or ``c#.5``), or after the octave
number (e.g. ``c#5.``)

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

Example
=======

Consider the following PTTL string:

::

    # 123 beats-per-minute, default quarter note, default 4th octave
    Test Melody:
    b=123, d=4, o=4:

    16c|16e|16g5,
    8p,
    16c|16e|16g5


This would play 3 sixteenth notes simultaneously (C, octave 4; E, octave 4;
G, octave 5), followed by an eighth note rest, followed by the same
three sixteenth notes again

Sample implementation
=====================

A sample implementation of a PTTL parser and polyphonic tone player is provided
in ``pttl.py``. Note that the ``pygame`` module is used to generate the tones,
so ``pttl.py`` will not work if you do not have ``pygame`` installed.

``pttl.py`` will work on Linux & Windows (untested on OSX), as long as
``pygame`` is installed.

``pttl.py`` will parse a text file passed on the command line, and play the
resulting tones. Try it with some of the included melodies in the ``examples``
directory:

::

   python pttl.py examples/polyphonic_example.txt
