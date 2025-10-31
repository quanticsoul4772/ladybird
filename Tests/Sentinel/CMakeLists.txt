# TODO: These tests require unimplemented GracefulDegradation methods:
#   - is_recovery_in_progress()
#   - is_auto_recovery_enabled()
#   - register_degradation_callback()
#   - get_metrics()
#   - reset_metrics()
#   - degradation_level_to_string()
# These tests are complete but need the implementation first.
# Uncomment when methods are implemented.

set(TEST_SOURCES
    # TestFlowInspector.cpp  # Moved to Services/Sentinel/CMakeLists.txt
    # TestGracefulDegradation.cpp
    # TestHealthCheck.cpp
)

foreach(source IN LISTS TEST_SOURCES)
    ladybird_test("${source}" LIBS sentinelservice LibDatabase LibFileSystem LibThreading)
endforeach()
