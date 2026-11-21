add_executable(bench_matching bench_matching.cpp)
target_link_libraries(bench_matching PRIVATE aeromatch_core benchmark::benchmark)

add_executable(bench_memory bench_memory.cpp)
target_link_libraries(bench_memory PRIVATE aeromatch_core benchmark::benchmark)

add_executable(bench_queue bench_queue.cpp)
target_link_libraries(bench_queue PRIVATE aeromatch_core benchmark::benchmark)
