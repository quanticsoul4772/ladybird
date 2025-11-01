set(CMAKE_AUTOMOC OFF)
set(CMAKE_AUTORCC OFF)
set(CMAKE_AUTOUIC OFF)

set(SOURCES
    AuditLog.cpp
    BloomFilter.cpp
    C2Detector.cpp
    ClientRateLimiter.cpp
    DatabaseMigrations.cpp
    DNSAnalyzer.cpp
    FederatedLearning.cpp
    FingerprintingDetector.cpp
    FlowInspector.cpp
    GracefulDegradation.cpp
    HealthCheck.cpp
    InputValidator.cpp
    IPFSThreatSync.cpp
    MalwareML.cpp
    PhishingURLAnalyzer.cpp
    PolicyGraph.cpp
    PolicyTemplates.cpp
    SentinelMetrics.cpp
    SentinelServer.cpp
    ThreatFeed.cpp
)

add_library(sentinelservice STATIC ${SOURCES})

# TensorFlow Lite Integration (Optional)
# Note: TFLite is not in vcpkg, so we check for manual installation
find_library(TFLITE_LIBRARY NAMES tensorflowlite tensorflow-lite
    PATHS
        /usr/local/lib
        /usr/lib
        ~/tensorflow/build
        ${CMAKE_CURRENT_SOURCE_DIR}/tensorflow-lite
    NO_DEFAULT_PATH)

find_path(TFLITE_INCLUDE_DIR tensorflow/lite/interpreter.h
    PATHS
        /usr/local/include
        /usr/include
        ~/tensorflow
        ${CMAKE_CURRENT_SOURCE_DIR}/tensorflow-lite
    NO_DEFAULT_PATH)

if(TFLITE_LIBRARY AND TFLITE_INCLUDE_DIR)
    message(STATUS "Found TensorFlow Lite: ${TFLITE_LIBRARY}")
    message(STATUS "TensorFlow Lite include: ${TFLITE_INCLUDE_DIR}")
    target_include_directories(sentinelservice PRIVATE ${TFLITE_INCLUDE_DIR})
    target_link_libraries(sentinelservice PRIVATE ${TFLITE_LIBRARY})
    target_compile_definitions(sentinelservice PRIVATE ENABLE_TFLITE)
else()
    message(STATUS "TensorFlow Lite not found. Using heuristic-based malware detection.")
    message(STATUS "To enable ML inference, install TensorFlow Lite and rebuild.")
    # Define stub mode for simplified ML implementation
    target_compile_definitions(sentinelservice PRIVATE USE_ML_STUB)
endif()

add_executable(Sentinel main.cpp)
add_executable(TestPolicyGraph TestPolicyGraph.cpp)
add_executable(TestPhase3Integration TestPhase3Integration.cpp)
add_executable(TestBackend TestBackend.cpp)
add_executable(TestDownloadVetting TestDownloadVetting.cpp)
add_executable(TestFlowInspector TestFlowInspector.cpp)
add_executable(TestMalwareML TestMalwareML.cpp)
add_executable(TestPhishingURLAnalyzer TestPhishingURLAnalyzer.cpp)
add_executable(TestFingerprintingDetector TestFingerprintingDetector.cpp)
add_executable(TestC2Detector TestC2Detector.cpp)
add_executable(TestDNSAnalyzer TestDNSAnalyzer.cpp)
add_executable(BenchmarkC2Detector BenchmarkC2Detector.cpp)
add_executable(sentinel-cli ../../Tools/sentinel-cli.cpp)

target_include_directories(sentinelservice PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/../..)
target_include_directories(sentinelservice PRIVATE ${LADYBIRD_SOURCE_DIR}/Services/)

# Find YARA library
find_library(YARA_LIBRARY NAMES yara libyara PATHS /usr/lib /usr/lib/x86_64-linux-gnu /usr/local/lib)
find_path(YARA_INCLUDE_DIR yara.h PATHS /usr/include /usr/local/include)

if(NOT YARA_LIBRARY)
    message(WARNING "YARA library not found. Sentinel will not build.")
    message(WARNING "Install YARA with: sudo apt-get install libyara-dev yara")
else()
    message(STATUS "Found YARA library: ${YARA_LIBRARY}")
    message(STATUS "Found YARA include: ${YARA_INCLUDE_DIR}")
    target_include_directories(sentinelservice PRIVATE ${YARA_INCLUDE_DIR})
    target_link_libraries(sentinelservice PRIVATE ${YARA_LIBRARY})
endif()

# Find ICU for Unicode homograph detection
find_package(ICU REQUIRED COMPONENTS uc i18n)

target_link_libraries(sentinelservice PUBLIC AK LibCore LibCrypto LibDatabase LibFileSystem LibIPC LibURL ICU::uc ICU::i18n)
target_link_libraries(Sentinel PRIVATE sentinelservice LibCore LibMain)
target_link_libraries(TestPolicyGraph PRIVATE sentinelservice LibCore LibMain)
target_link_libraries(TestPhase3Integration PRIVATE sentinelservice LibCore LibMain LibFileSystem)
target_link_libraries(TestBackend PRIVATE sentinelservice LibCore LibMain LibFileSystem)
target_link_libraries(TestDownloadVetting PRIVATE sentinelservice LibCore LibMain LibFileSystem LibCrypto)
target_link_libraries(TestFlowInspector PRIVATE sentinelservice LibCore LibMain)
target_link_libraries(TestMalwareML PRIVATE sentinelservice LibCore LibMain)
target_link_libraries(TestPhishingURLAnalyzer PRIVATE sentinelservice LibCore LibMain LibURL)
target_link_libraries(TestFingerprintingDetector PRIVATE sentinelservice LibCore LibMain)
target_link_libraries(TestC2Detector PRIVATE sentinelservice LibCore LibMain)
target_link_libraries(TestDNSAnalyzer PRIVATE sentinelservice LibCore LibMain)
target_link_libraries(BenchmarkC2Detector PRIVATE sentinelservice LibCore LibMain)
target_link_libraries(sentinel-cli PRIVATE sentinelservice LibCore LibMain LibFileSystem requestserverservice)

# Use proper test infrastructure for LibTest-based tests
ladybird_test(TestBloomFilter.cpp Sentinel LIBS sentinelservice LibDatabase LibFileSystem)

# Install targets
install(TARGETS Sentinel sentinel-cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY ${CMAKE_SOURCE_DIR}/Base/res/ladybird/sentinel/rules DESTINATION ${CMAKE_INSTALL_DATADIR}/ladybird/sentinel)
