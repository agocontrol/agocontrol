cmake_minimum_required (VERSION 3.0)

set(EXTRA_FILES tellstickbase.py tellstickduo.py tellsticknet.py tellstick_auth.py td.py)

AgoPythonProgram(tellstick EXTRA_FILES ${EXTRA_FILES})