import unittest
import os
from setuptools import setup
from distutils.core import Command

from ptttl import __version__


HERE = os.path.abspath(os.path.dirname(__file__))
README = os.path.join(HERE, "README.rst")
REQS = os.path.join(HERE, "requirements.txt")

classifiers = [
    'License :: OSI Approved :: Apache Software License',
    'Operating System :: OS Independent',
    'Programming Language :: Python',
    'Natural Language :: English',
    'Intended Audience :: Developers',
    'Intended Audience :: Science/Research',
    'Intended Audience :: Education',
    'Intended Audience :: Information Technology',
    'Programming Language :: Python :: 2',
    'Programming Language :: Python :: 2.7',
]

with open(README, 'r') as f:
    long_description = f.read()

deps = []
with open(REQS, 'r') as f:
    deps = f.read().strip().split('\n')

print(deps)

setup(
    name='ptttl',
    version=__version__,
    description=('Converts PTTTL/RTTTL files to audio data'),
    long_description=long_description,
    url='http://github.com/eriknyquist/ptttl',
    author='Erik Nyquist',
    author_email='eknyquist@gmail.com',
    license='Apache 2.0',
    packages=['ptttl'],
    classifiers = classifiers,
    install_requires=deps
)
