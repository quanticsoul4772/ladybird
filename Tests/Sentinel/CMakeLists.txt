set(TEST_SOURCES
    TestGracefulDegradation.cpp
    TestHealthCheck.cpp
)

foreach(source IN LISTS TEST_SOURCES)
    get_filename_component(name "${source}" NAME_WE)
    add_executable("${name}" "${source}")
    target_link_libraries("${name}" PRIVATE sentinelservice LibCore LibSQL LibFileSystem LibTest LibThreading)
    add_test(NAME "${name}" COMMAND "${name}")
endforeach()
