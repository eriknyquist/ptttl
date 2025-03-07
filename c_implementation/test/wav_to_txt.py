import wave
import sys
import os
import struct

def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <file.wav>")
        return -1

    with wave.open(sys.argv[1], "rb")as fh:
        if fh.getsampwidth() != 2:
            raise RuntimeError("Only 16-bit samples are supported")
        if fh.getnchannels() != 1:
            raise RuntimeError("Only single-channel .wav files are supported")

        print(f"# {sys.argv[1]}, {fh.getframerate()}Hz")

        while True:
            frame_data = fh.readframes(1024)
            num_samples = int(len(frame_data) / fh.getsampwidth())
            samples = struct.unpack(f"<{num_samples}h", frame_data)
            print('\n'.join([str(x) for x in samples]))

            if num_samples < 1024:
                break

    return 0

if __name__ == "__main__":
    sys.exit(main())
