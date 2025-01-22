cmake_minimum_required(VERSION 3.10)
project(GarbageCollector LANGUAGES CXX)

enable_testing()

include(cmake/BuildFlags.cmake)

include_directories(${PROJECT_SOURCE_DIR}/include)

add_subdirectory(src)
add_subdirectory(tests)

# Add Google Test
add_subdirectory(external/googletest)
