add_library(garbage_collector
    gc.cpp
)

target_include_directories(garbage_collector PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)