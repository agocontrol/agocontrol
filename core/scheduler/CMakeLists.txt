cmake_minimum_required (VERSION 3.0)

set(EXTRA_FILES groups.py scheduler.py variables.py)
AgoPythonProgram(scheduler
    DESCRIPTION "ago control scheduler"
    EXTRA_FILES "${EXTRA_FILES}")
