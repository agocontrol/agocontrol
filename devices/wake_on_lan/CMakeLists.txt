cmake_minimum_required (VERSION 3.0)

set(EXTRA_FILES pinger.py configobj.py)
AgoPythonProgram(wake_on_lan
    DESCRIPTION "ago control Wake on LAN handler"
    EXTRA_FILES "${EXTRA_FILES}"
    RUN_AS root)
