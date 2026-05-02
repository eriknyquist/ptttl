import wave
import sys
import os
import struct

def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <input.wav> <output.bin>")
        return -1

    with wave.open(sys.argv[1], "rb")as ih:
        if ih.getsampwidth() != 2:
            raise RuntimeError("Only 16-bit samples are supported")
        if ih.getnchannels() != 1:
            raise RuntimeError("Only single-channel .wav files are supported")

        with open(sys.argv[2], "wb") as oh:
            while True:
                frame_data = ih.readframes(1024)
                num_samples = int(len(frame_data) / ih.getsampwidth())
                oh.write(frame_data)

                if num_samples < 1024:
                    break

    return 0

if __name__ == "__main__":
    sys.exit(main())
