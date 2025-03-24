add_library(garbage_collector
    gc.cpp
    gc_impl.cpp
)

set_source_files_properties(gc_collect.S PROPERTIES LANGUAGE ASM)

target_include_directories(garbage_collector PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)