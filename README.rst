.. contents:: Table of Contents

Polyphonic Tone Transfer Language
#################################

``ptttl`` is a command-line utility for converting PTTTL and
`RTTTL <https://en.wikipedia.org/wiki/Ring_Tone_Transfer_Language>`_ files to
.wav audio files. `ptttl` also provides an API for parsing PTTTL and
`RTTTL <https://en.wikipedia.org/wiki/Ring_Tone_Transfer_Language>`_ files to convert them
into usable musical data.

The Polyphonic Tone Text Transfer Language (PTTTL) is a way to describe polyphonic
melodies, and is a superset of Nokia's
`RTTTL <https://en.wikipedia.org/wiki/Ring_Tone_Transfer_Language>`_ format, extending
it to enable polyphony and vibrato.


Python reference implementation
###############################

A reference implementation in python is provided for general purpose applications.

API documentation
==================
API documentation `can be found here <https://ptttl.readthedocs.io/>`_


Install
=======

Install from pip

::

    pip install -r ptttl


Converting PTTTL/RTTTL files to .wav files from the command line
================================================================

::

   python -m ptttl input.ptttl -f output.wav

Run ``python -m ptttl -h`` to see available options.


Parsing PTTTL/RTTTL files in a python script
============================================

::

   >>> from ptttl.parser import PTTTLParser
   >>> with open('input.pttl', 'r') as fh:
   ...     ptttl_source = fh.read()
   ...
   >>> parser = PTTTLParser()
   >>> ptttl_data = parser.parse(ptttl_source)
   >>> ptttl_data
   PTTTLData([PTTTLNote(pitch=195.9977, duration=0.5625), PTTTLNote(pitch=195.9977, duration=0.2812), ...], ...)


Converting PTTTL/RTTTL files to .wav in a python script
=======================================================

::

   >>> from ptttl.audio import ptttl_to_wav
   >>> with open('input.pttl', 'r') as fh:
   ...     ptttl_source = fh.read()
   ...
   >>> ptttl_to_wav(ptttl_source, 'output.wav')


C reference implementation
##########################

A reference implemention written in C is provided, which is suitable for use in
embedded applications, under the ``c_implementation`` directory.

`See the README for the C implementation here. <c_implementation/README.rst>`_

PTTTL format
############

Because PTTTL is a superset of RTTTL, valid RTTTL strings are also valid PTTTL strings.
A parser that properly handles PTTTL can also handle RTTTL.

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

The very first statement is the *default values* section, and it is the same as
the *default values* section from the RTTTL format, except with two additional
vibrato-related settings:

::

  b=123, d=8, o=4, f=7, v=10

* *b* - beat, tempo: tempo in BPM (Beats Per Minute)
* *d* - duration: default duration of a note if none is specified
* *o* - octave: default octave of a note if none is specified
* *f* - frequency: default vibrato frequency if none is specified, in Hz
* *v* - variance: default vibrato variance from the main pitch if none is specified, in Hz

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

Vibrato
-------

Optionally, vibrato may be enabled and configured for an individual note. This is
done by appending a ``v`` to the end of the note, and optionally frequency and variance
values seperated by a ``-`` character. For example:

* ``4c#v`` refers to a C# quarter note with vibrato enabled, using default settings
* ``4c#v10-15`` refers to a C# quarter note with vibrato enabled, using a vibrato frequency of 10Hz,
  with a maximum vibrato variance of 15Hz from the main pitch.

Example
-------

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
seperates *concurrent* tracks within a single polyphonic melody, the semicolon
character seperates multiple *sequential* polyphonic melodies within a single
data section. Blocks of notes seperated by semicolons will be "stitched together",
or concatenated, in the final output.

The semicolon does not affect any of the timings or pitch of the generated
tones; it just makes the PTTTL source a bit more readable, and gives you more
options for organizing the lines when writing music. Have a look at this larger 
PTTTL file, with 4 simultaneous melodies, for a good example of why the
semicolon is useful:

::

    All Star but it's a Bach chorale:
    d=4,o=5,b=100, f=7, v=10:

    #some   bo  -   dy      once    told    me      the     world   was     go -

    4gb5v,  8db6,   8bb5,   4bb5,   8ab5v,  8gb5,   8gb5,   4b5v,   8bb5,   8bb5 |
    4gb4,   8gb4,   8gb4,   4gb4,   8f4,    8gb4,   8gb4,   4ab4,   8g4,    8g4  |
    4gb4,   8bb4,   8db5,   4db5,   8db5,   8db5,   8db5,   4eb5,   8db5,   8db5 |
    4gb3,   8gb3,   8gb3,   4gb3,   8ab3,   8bb3,   8bb3,   4ab3,   8bb3,   8bb3 ;



    #-na    roll    me,     I       aint    the     sharp - est     tool    in

    8ab5,   8ab5v,  4gb5,   8gb5v,  8db6v,  8bb5,   8bb5v,  8ab5,   8ab5v,  8gb5 |
    8ab4,   8eb4,   4eb4,   8eb4,   8gb4,   8gb4,   8gb4,   8f4,    8f4,    8eb4 |
    8eb5,   8eb5,   4b4,    8b4,    8db5,   8db5,   8db5,   8b4,    8b4,    8bb4 |
    8b3,    8b3,    4eb4,   8b3,    8bb3,   8b3,    8db4,   8db4,   8d4,    8eb4 ;



    #the    she  -  ed,             she     was     loo  -  king    kind    of

    8gb5,   4eb5v,  8db5v,  2p,     8gb5,   8gb5,   8db6v,  8bb5,   8bb5,   8ab5 |
    8eb4,   4b3,    8ab3,   2p,     8db4,   8db4,   8gb4,   8gb4,   8gb4,   8f4  |
    8bb4,   4gb4,   8f4,    2p,     8gb4,   8gb4,   8bb4,   8db5,   8db5,   8db5 |
    8db4,   4b3,    8ab3,   2p,     8bb3,   8ab3,   8gb3,   8gb3,   8gb3,   8ab3 ;



    #dumb   with    her     fing  - er      and     her     thumb   in      the

    8ab5v,  8gb5,   8gb5,   4b5v,   8bb5,   8bb5,   8ab5,   8ab5v,  8gb5,   8gb5 |
    8gb4,   8gb4,   8eb4,   4eb4,   8eb4,   8eb4,   8eb4,   8eb4,   8eb4,   8eb4 |
    8db5,   8db5,   8bb4,   4ab4,   8db5,   8db5,   8b4,    8b4,    8b4,    8b4  |
    8bb3,   8bb3,   8eb4,   4ab4,   8g4,    8g4,    8ab4,   8ab3,   8b3,    8b3  ;



    #shape  of      an      L       on      her     for  -  head

    4db6v,  8bb5v,  8bb5v,  4ab5v,  8gb5,   8gb5,   4ab5v,  8eb5 |
    4gb4,   8gb4,   8gb4,   4f4,    8f4,    8eb4,   4eb4,   8b3  |
    4db5,   8db5,   8db5,   4b4,    8bb4,   8bb4,   4b4,    8ab4 |
    4bb3,   8b3,    8db4,   4d4,    8eb4,   8eb4 ,  4ab4,   8ab4
