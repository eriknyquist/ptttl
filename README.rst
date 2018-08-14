Polyphonic Tone Transfer Language
#################################

The Polyphonic Tone Text Transfer Language (PTTTL) is a way to describe polyphonic
melodies, and is a superset of Nokia's
`RTTTL <https://en.wikipedia.org/wiki/Ring_Tone_Transfer_Language>`_ format,
used for monophonic ringtones.

Why?
####

I needed a good way to store simple tones and melodies for some project.
RTTTL looked pretty good but only works for monophonic melodies.
I needed polyphony.

PTTTL format
###########

Valid RTTTL strings are also valid PTTTL strings. A parser that properly handles
PTTTL can also handle RTTTL.

A PTTTL string is made up of three colon-seperated sections; **name** section,
**default values** section, and **data** section.

Whitespace characters, empty lines, and lines beginning with a "#" character
are ignored.

The initial "name" section is intended to contain the name of the ringtone
in the original RTTTL format. PTTTL requires this field to be present, to
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

The PTTTL data section is just like the RTTTL data section, in that a melody
consists of multiple comma-seperated notes to be played sequentially. *Unlike*
RTTTL, PTTTL allows multiple melodys to be defined, separated by the vertical
pipe character ``|``, all of which will be played in unison.

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

Consider the following PTTTL string:

::

    # 123 beats-per-minute, default quarter note, default 4th octave
    Test Melody:
    b=123, d=4, o=4:

    16c, 8p, 16c | 16e, 8p, 16e | 16g5, 8p, 16g5


This would play 3 sixteenth notes simultaneously (C, octave 4; E, octave 4;
G, octave 5), followed by an eighth note rest, followed by the same
three sixteenth notes again

Note that the above sample is much easier to read if we put each melody on a new
line and align the notes in columns. This is the recommended way to write
PTTTL:

::

    # Nicely aligned
    Test Melody:
    b=123, d=4, o=4:

    16c,  8p,  16c  |
    16e,  8p,  16e  |
    16g5, 8p,  16g5

In order to keep things readable for large PTTTL files with multiple
concurrent tracks, a semicolon character ``;`` can be used further break up
melodies into more practical blocks. Just as the veritcal pipe character ``|``
seperates *concurrent* tracks within a single melody, the semicolon character
seperates multiple *sequential* melodies within a single data section. Melodies
seperated by semicolons will be stitched together, one after the other, in the
final output.

The semicolon does not affect any of the timings or pitch of the generated
tones; it just makes the PTTTL source a bit more readable. Have a look at this
larger PTTTL file, with 4 simultaneous melodies, for a good example of why the
semicolon is useful:

::

    All Star but it's a Bach chorale:
    d=4,o=5,b=40:

    #some   bo  -   dy      once    told    me      the     world   was     go -

    16g3.,  32g3.,  32g3.,  16g3.,  32f#.,  32g.,   32g.,   16a3.,  32g#.,  32g#. |
    16g.,   32g.,   32g.,   16d.,   32a3.,  32b3.,  32b3.,  16a.,   32b.,   32b.  |
    16g4.,  32d.6,  32b.,   16g.,   32a.,   32d.,   32d.,   16e.,   32b3.,  32b3. |
    16g4.,  32b.4,  32d.,   16b.,   32d.,   32d.,   32d.,   16c6.,  32d.,   32d.  ;



    #-na    roll    me,     I       aint    the     sharp - est     tool    in

    32a.,   32a.,   16g.,   32g.,   32g.,   32g.,   32g.,   32f#.,  32d#4., 32g.  |
    32c4.,  32c4.,  16e.,   32c.,   32d6.,  32b.,   32d4.,  32d4.,  32f#.,  32e4. |
    32e.,   32e.,   16e4.,  32c4.,  32b3.,  32c4.,  32b.,   32c.,   32a.,   32b.4 |
    32e.,   32e.,   16c.,   32e.,   32d.,   32d.,   32d.,   32a.,   32c.,   32e.  ;



    #the    she  -  ed,             she     was     loo  -  king    kind    of

    32g.,   16c4.,  32a3.,  8p.,    32g.,   32g.,   32g3.,  32g3.,  32g3.,  32f#. |
    32d4.,  16e.,   32d.,   8p.,    32g4.,  32g4.,  32g.,   32g.,   32g.,   32a3. |
    32b.4,  16c.,   32f#4., 8p.,    32b3.,  32a3.,  32b4.,  32b.,   32b.,   32a.  |
    32e.,   16g4.,  32a4.,  8p.,    32d.,   32d.,   32d6.,  32d.,   32d.,   32d.  ;



    #dumb   with    her     fing  - er      and     her     thumb   in      the

    32g.,   32g.,   32g.,   16a4.,  32b.,   32b.,   32a.,   32a3.,  32g.,   32g.  |
    32a.,   32b3.,  32e4.,  16e.,   32g#4., 32g#4., 32c.,   32c.,   32b4.,  32c.  |
    32b3.,  32d.,   32b4.,  16c6.,  32e.,   32e.,   32e.,   32a.,   32c4.,  32c4. |
    32d.,   32d.,   32e.,   16c6.,  32d.,   32d.,   32a4.,  32e.,   32e.,   32e.  ;



    #shape  of      an      L       on      her     for  -  head

    16g.,   32g.,   32g.,   16a.,   32g.,   32g.,   16a.,   32a4. |
    16d.,   32b.,   32d4.,  16f#.,  32e4.,  32e4.,  16a4.,  32d.  |
    16d6.,  32c4.,  32b.,   16c.,   32b4.,  32b4.,  16f#.,  32e.  |
    16b3.,  32d.,   32d.,   16d#4., 32e.,   32e.,   16c.,   32e.

Sample implementation
=====================

A sample implementation of a PTTTL parser and .wav encoder is
provided in ``ptttl_parser.py`` and ``ptttl_audio_encoder.py``.

These sample scripts will work on Linux & Windows (untested on OSX).

Try the sanmple scripts out with some of the included melodies in the
``rtttl_examples`` directory.

Cloning sample implementation
-----------------------------

::

    git clone https://github.com/eriknyquist/ptttl
    cd ptttl
    git submodule update --init --recursive

Using sample implementation
---------------------------

Converting a PTTTL file into audible tones in a .wav file:

::

   python ptttl_audio_encoder.py polyphonic_example.txt monty.wav
