import wave
import sys
import os
import struct

def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <file.txt> <file.wav>")
        return -1

    with wave.open(sys.argv[2], "wb") as wfh:
        wfh.setnchannels(1)
        wfh.setsampwidth(2)
        wfh.setframerate(44100)

        with open(sys.argv[1], "r") as tfh:
            values = []
            while True:
                line = tfh.readline()
                if not line:
                    break

                values.append(int(line))

        wfh.writeframes(struct.pack(f"<{len(values)}h", *values))

    return 0

if __name__ == "__main__":
    sys.exit(main())
