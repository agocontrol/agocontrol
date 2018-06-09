cmake_minimum_required (VERSION 3.0)

set(EXTRA_FILES pinger.py configobj.py)
AgoPythonProgram(wake_on_lan EXTRA_FILES "${EXTRA_FILES}")