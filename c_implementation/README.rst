Portable C implementation of PTTTL/RTTTL parser
-----------------------------------------------

This implementation is split into 3 parts, to make it easier for you to take
only the file(s) that you need:

* **ptttl_parser.c**: Reads PTTTL or RTTTL source, and produces an intermediate
  representation of the musical data which is easy to convert to audio samples.
  No dynamic memory allocation, and no loading the entire source file into memory
  (a callback must be provided to read/fetch the next source character). See
  ``ptttl_parser.h`` for more details. Requires ``stdint.h``, ``strtoul()`` from
  ``stdlib.h``, and ``memset()`` from ``string.h``.

* **ptttl_sample_generator.c**: Reads the output of ``ptttl_parser.c`` and produces
  16-bit audio samples containing the tones described by the RTTTL/PTTTL source, as sine
  wave tones. The global attack and decay time for all notes is configurable. The next
  audio sample is produced only on your request, so there is no need to store a large
  number of samples in memory. See ``ptttl_sample_generator.h`` for more details.
  Requires ``stdint.h``, ``memset()`` from ``string.h``, and ``sinf()`` from ``math.h.``.

* **ptttl_to_wav.c**: Reads the output of ``ptttl_parser.c`` and produces a .wav file
  containing the tones described by the RTTTL/PTTTL source, as sine wave tones.
  ``ptttl_sample_generator.c`` is used to generate one sample at a time and write it
  to the .wav file immediately, so there is no need to store the entire .wav file in memory.

How to use
==========

There are two supported scenarios...

You want to read PTTTL/RTTTL text and generate audio samples at a specific sample rate
######################################################################################

* Compile ``ptttl_parser.c`` and ``ptttl_sample_generator.c`` along with your project

* Use ``ptttl_parser.c`` to convert your PTTTL/RTTTL source to intermediate representation
  (See ``ptttl_parser.h`` for API documentation)

* Use ``ptttl_sample_generator.c`` to convert intermediate representation to samples
  (See ``ptttl_sample_generator.h`` for API documentation)

You want to read PTTTL/RTTTL text and generate a .wav file
##########################################################

* Compile ``ptttl_parser.c``, ``ptttl_sample_generator.c`` and ``ptttl_to_wav.c``
  along with your project

* Use ``ptttl_to_wav.c`` to convert intermediate representation to .wav file
  (See ``ptttl_to_wav.h`` for API documentation)

