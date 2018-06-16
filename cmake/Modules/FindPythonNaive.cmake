
# On modern CMake versions we have https://cmake.org/cmake/help/latest/module/FindPython2.html etc
# but on cmake 3.0 we don't.

# This is losely based on https://github.com/caffe2/caffe2/issues/1676
function(pycmd python_exe outvar cmd)
    # run the actual command
    execute_process(
            COMMAND "${python_exe}" -c "${cmd}"
            RESULT_VARIABLE _exitcode
            OUTPUT_VARIABLE _output)

    if(NOT ${_exitcode} EQUAL 0)
        message(ERROR " Failed when running ${python_exe} code: \"\"\"\n${cmd}\n\"\"\"")
        message(FATAL_ERROR " Python command failed with error code: ${_exitcode}")
    endif()

    # Remove supurflous newlines (artifacts of print)
    string(STRIP "${_output}" _output)
    set(${outvar} "${_output}" PARENT_SCOPE)
endfunction()


find_program(PYTHON2_EXECUTABLE NAMES python2 python2.7 python2.6)
find_program(PYTHON3_EXECUTABLE NAMES python3)

if(PYTHON2_EXECUTABLE)
    pycmd(${PYTHON2_EXECUTABLE} Python2_SITELIB "import distutils.sysconfig; print distutils.sysconfig.get_python_lib(prefix='')")
    message(STATUS "Python2_SITELIB = ${Python2_SITELIB}")
else()
    message(ERROR "python2 executable not found. this is required for proper agocontrol operation")
endif()

if(PYTHON3_EXECUTABLE)
    pycmd(${PYTHON3_EXECUTABLE} Python3_SITELIB "import distutils.sysconfig; print(distutils.sysconfig.get_python_lib(prefix=''))")
    message(STATUS "Python3_SITELIB = ${Python3_SITELIB}")
else()
    message(STATUS "python3 executable not found")
endif()
