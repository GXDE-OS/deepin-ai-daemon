# Only build development tools in debug mode
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_subdirectory(embclient)
endif()



