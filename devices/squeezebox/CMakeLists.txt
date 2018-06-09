cmake_minimum_required (VERSION 3.0)

set(EXTRA_FILES pylmsserver.py pylmslibrary.py pylmsplayer.py pylmsplaylist.py)
AgoPythonProgram(squeezebox EXTRA_FILES "${EXTRA_FILES}")