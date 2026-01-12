# Sentinel Performance Benchmarks
# Add these to the appropriate CMakeLists.txt file

# Full download path benchmark
add_executable(benchmark_sentinel_full benchmark_sentinel_full.cpp)
target_link_libraries(benchmark_sentinel_full PRIVATE
    LibCore
    LibCrypto
    RequestServer
)

# Database performance benchmark
add_executable(benchmark_policydb benchmark_policydb.cpp)
target_link_libraries(benchmark_policydb PRIVATE
    LibCore
    LibFileSystem
    LibDatabase
    Sentinel
)

# Optional: Install benchmarks
install(TARGETS benchmark_sentinel_full benchmark_policydb
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
