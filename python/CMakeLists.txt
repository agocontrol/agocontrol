cmake_minimum_required (VERSION 3.0)

configure_file (
    "../python/agoclient/_directories.py.in"
    "${CMAKE_CURRENT_BINARY_DIR}/agoclient/_directories.py"
    @ONLY
)

set(PYTHON_AGOCLIENT_SOURCE_FILES
        agoclient/agoapp.py
        agoclient/agoconnection.py
        agoclient/agoproto.py
        agoclient/agotransport.py
        agoclient/agotransport_qpid.py
        agoclient/agotransport_mqtt.py
        agoclient/config.py
        agoclient/__init__.py
        agoclient/_logging.py
)

CopyFilesFromSource(agoclient-python "${PYTHON_AGOCLIENT_SOURCE_FILES}")

InstallFiles(${PYTHON2DIST}/agoclient "${PYTHON_AGOCLIENT_SOURCE_FILES};agoclient/_directories.py")
