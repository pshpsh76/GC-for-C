add_library(garbage_collector
    gc.cpp
    gc_impl.cpp
    gc_scheduler.cpp
    gc_pacer.cpp
)

target_include_directories(garbage_collector PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)