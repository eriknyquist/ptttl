Portable C implementation of PTTTL/RTTTL parser
-----------------------------------------------

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

* **afl_fuzz_harness.c**: Implements a "harness" to fuzz the ``ptttl_parse()`` function
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

* Use ``ptttl_parser.c`` to convert your PTTTL/RTTTL source to intermediate representation
  (See ``ptttl_parser.h`` for API documentation)

* Use ``ptttl_to_wav.c`` to convert intermediate representation to .wav file
  (See ``ptttl_to_wav.h`` for API documentation)

Configuration options (``ptttl_config.h``) and how they affect `ptttl_output_t` size
====================================================================================

The size of the ``ptttl_output_t`` struct (intermediate representation of RTTTL/PTTTL source)
is fixed at compile time, and is significantly affected by the following symbols defined in ``ptttl_config.h``:

* ``PTTTL_MAX_CHANNELS_PER_FILE``
* ``PTTTL_MAX_NOTES_PER_CHANNEL``
* ``PTTTL_VIBRATO_ENABLED``

See API documentation in ``ptttl_config.h`` for more details.

The following table shows various combinations of these 3 settings, along with the
corresponding size of the ``ptttl_output_t`` struct, to give you an idea of how these
things affect each other:

+-------------------------------+-------------------------------+-------------------------+--------------------------------+
|``PTTTL_MAX_CHANNELS_PER_FILE``|``PTTTL_MAX_NOTES_PER_CHANNEL``|``PTTTL_VIBRATO_ENABLED``|``ptttl_output_t`` size in bytes|
+===============================+===============================+=========================+================================+
| 1                             | 64                            | 0                       | 520                            |
+-------------------------------+-------------------------------+-------------------------+--------------------------------+
| 1                             | 64                            | 1                       | 776                            |
+-------------------------------+-------------------------------+-------------------------+--------------------------------+
| 4                             | 64                            | 0                       | 1,300                          |
+-------------------------------+-------------------------------+-------------------------+--------------------------------+
| 4                             | 64                            | 1                       | 2,324                          |
+-------------------------------+-------------------------------+-------------------------+--------------------------------+
| 6                             | 64                            | 0                       | 1,820                          |
+-------------------------------+-------------------------------+-------------------------+--------------------------------+
| 6 (default)                   | 64 (default)                  | 1 (default)             | 3,356 (default)                |
+-------------------------------+-------------------------------+-------------------------+--------------------------------+
| 1                             | 128                           | 0                       | 776                            |
+-------------------------------+-------------------------------+-------------------------+--------------------------------+
| 1                             | 128                           | 1                       | 1,288                          |
+-------------------------------+-------------------------------+-------------------------+--------------------------------+
| 4                             | 128                           | 0                       | 2,324                          |
+-------------------------------+-------------------------------+-------------------------+--------------------------------+
| 4                             | 128                           | 1                       | 4,372                          |
+-------------------------------+-------------------------------+-------------------------+--------------------------------+
| 6                             | 128                           | 0                       | 3,356                          |
+-------------------------------+-------------------------------+-------------------------+--------------------------------+
| 6                             | 128                           | 1                       | 6,428                          |
+-------------------------------+-------------------------------+-------------------------+--------------------------------+

