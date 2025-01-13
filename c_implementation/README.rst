Portable C implementation of PTTTL/RTTTL parser
-----------------------------------------------

This is a reference implementation of a PTTTL/RTTTL parser in pure C, suitable for embedded
applications (no dynamic memory allocation, low static memory usage, configurable trade-offs
for performance vs. memory usage).

This implementation is split into 3 main parts, to make it easier for you to take
only the file(s) that you need:

* **ptttl_parser.c**: Reads PTTTL or RTTTL source, and produces an intermediate
  representation of the musical data which is easy to convert to audio samples.
  No dynamic memory allocation, and no loading the entire source file into memory
  (a callback must be provided to read/fetch the next source character). See
  ``ptttl_parser.h`` for more details. Requires ``stdint.h``, ``strtoul()`` from
  ``stdlib.h``, and ``memset()`` from ``string.h``.

* **ptttl_sample_generator.c**: Reads the output of ``ptttl_parser.c`` and produces
  signed 16-bit audio samples containing the tones described by the RTTTL/PTTTL source,
  as sine wave tones. The attack / decay time of the waveforms generated for notes
  is configurable. The next audio sample is produced only on your request, so there
  is no need to store a large number of samples in memory. See ``ptttl_sample_generator.h``
  for more details. Requires ``stdint.h``, ``memset()`` from ``string.h``, and ``sinf()``
  from ``math.h.``.

* **ptttl_to_wav.c**: Reads the output of ``ptttl_parser.c`` and produces a .wav file
  containing the tones described by the RTTTL/PTTTL source, as sine wave tones.
  ``ptttl_sample_generator.c`` is used to generate one sample at a time and write it
  to the .wav file immediately, so there is no need to store the entire .wav file in memory.
  Requires ``stdio.h`` and ``stdint.h``.

Some additional files, that are not required for normal usage but may be useful for
reference and/or development & testing, are also provided:

* **ptttl_cli.c**: Implements a sample command-line tool that uses ``ptttl_parser.c`` and
  ``ptttl_to_wav.c`` to convert RTTTL/PTTTL source to .wav files.

* **afl_fuzz_harness.c**: Implements a "harness" to fuzz the ``ptttl_to_wav()`` function
  using `AFL++ <https://github.com/AFLplusplus/AFLplusplus>`_

Building the sample applications
--------------------------------

`ptttl_cli`
###########

Build the `ptttl_cli` application using `make` (it's the default target):

::

    make


`afl_fuzz_harness`
##################

Build the `afl_fuzz_harness` application using the `afl_fuzz_harness` target:

::

    make afl_fuzz_harness


How to incorporate into your own applications
=============================================

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

* Use ``ptttl_to_wav.c`` to convert PTTTL/RTTTL source to .wav file
  (See ``ptttl_to_wav.h`` for API documentation)

``PTTTL_MAX_CHANNELS_PER_FILE`` setting and how it affects memory requirements
==============================================================================

The sizes of the ``ptttl_parser_t`` struct and ``ptttl_sample_generator_t`` struct
are fixed at compile time, and are significantly affected by the ``PTTTL_MAX_CHANNELS_PER_FILE``
build option.

See API documentation in ``ptttl_parser.h`` for more details.

The following table shows various values of ``PTTTL_MAX_CHANNELS_PER_FILE``, along with the
corresponding size of the ``ptttl_parser_t`` and ``ptttl_sample_generator_t`` structs, to give you an idea
of how much memory you'll need:

+-------------------------------+--------------------------------+------------------------------------------+
|``PTTTL_MAX_CHANNELS_PER_FILE``|``ptttl_parser_t`` size in bytes|``ptttl_sample_generator_t`` size in bytes|
+===============================+================================+==========================================+
| 1                             | 368                            | 72                                       |
+-------------------------------+--------------------------------+------------------------------------------+
| 2                             | 384                            | 112                                      |
+-------------------------------+--------------------------------+------------------------------------------+
| 4                             | 424                            | 192                                      |
+-------------------------------+--------------------------------+------------------------------------------+
| 8                             | 504                            | 360                                      |
+-------------------------------+--------------------------------+------------------------------------------+
| 16 (default)                  | 664                            | 688                                      |
+-------------------------------+--------------------------------+------------------------------------------+
| 32                            | 984                            | 1344                                     |
+-------------------------------+--------------------------------+------------------------------------------+
| 64                            | 1624                           | 2656                                     |
+-------------------------------+--------------------------------+------------------------------------------+

