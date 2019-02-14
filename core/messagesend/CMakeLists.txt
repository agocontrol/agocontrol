cmake_minimum_required (VERSION 3.0)

set (MESSAGESEND_LIBRARIES
    agoclient
)

# add the executable
add_executable (messagesend messagesend.cpp)
target_link_libraries (messagesend ${MESSAGESEND_LIBRARIES})

install (TARGETS messagesend RUNTIME DESTINATION ${BINDIR})
