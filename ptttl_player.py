import time
import sys
import math
from array import array
from time import sleep

import pygame

from pygame.mixer import Sound, get_init, pre_init

from ptttl_parser import PTTTLParser

pre_init(44100, -16, 1, 1024)
pygame.mixer.init()

FREQ, FMT, _ = get_init()

class Note(Sound):
    def __init__(self, frequency, duration, volume=.1):
        self.frequency = frequency
        self.duration = duration

        if self.frequency > 0:
            # This ugly try/except handles a silly thing that changed
            # between pygame versions
            try:
                Sound.__init__(self, buffer=self.build_samples())
            except TypeError:
                Sound.__init__(self, self.build_samples())

            self.set_volume(volume)

    def build_samples(self):
        period = int(round(FREQ / self.frequency))
        samples = array("h", [0] * period)
        amplitude = 2 ** (abs(FMT) - 1) - 1
        for time in xrange(period):
            if time < period / 2:
                samples[time] = amplitude
            else:
                samples[time] = -amplitude

        return samples

    def __str__(self):
        return '(%s, %s)' % (self.frequency, self.duration)

    def __repr__(self):
        return self.__str__()

class PTTTLPlayer(object):
    def __init__(self, pttl_string):
        self.parser = PTTTLParser()
        self.notes = []

        for slot in self.parser.parse(pttl_string):
            self.notes.append([])
            for pitch, dur in slot:
                self.notes[-1].append(Note(pitch, dur))

    def play(self):
        for slot in self.notes:
            start = time.time()

            i = 0
            if slot[0].frequency <= 0:
                time.sleep(slot[0].duration)
                continue

            for note in slot:
                note.play(-1)

            while i < len(slot):
                elapsed = time.time() - start

                while i < len(slot) and slot[i].duration <= elapsed:
                    slot[i].fadeout(5)
                    i += 1

                if i < len(slot):
                    time.sleep(slot[i].duration - elapsed)

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print "Usage: %s <.rtttl/.pttl file>" % sys.argv[0]
        sys.exit(1)

    with open(sys.argv[1], 'r') as fh:
        t = PTTTLPlayer(fh.read())

    t.play()
