# Always build the main daemon
add_subdirectory(daemon)

# Only build development tools in debug mode
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_subdirectory(modelconfigtool)
    message(STATUS "Debug mode: Building development tools")
else()
    message(STATUS "Release mode: Skipping development tools")
endif()



