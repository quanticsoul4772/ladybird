set(CMAKE_AUTOMOC OFF)
set(CMAKE_AUTORCC OFF)
set(CMAKE_AUTOUIC OFF)

set(SOURCES
    AuditLog.cpp
    DatabaseMigrations.cpp
    FlowInspector.cpp
    HealthCheck.cpp
    InputValidator.cpp
    PolicyGraph.cpp
    SentinelMetrics.cpp
    SentinelServer.cpp
)

add_library(sentinelservice STATIC ${SOURCES})

add_executable(Sentinel main.cpp)
add_executable(TestPolicyGraph TestPolicyGraph.cpp)
add_executable(TestPhase3Integration TestPhase3Integration.cpp)
add_executable(TestBackend TestBackend.cpp)
add_executable(TestDownloadVetting TestDownloadVetting.cpp)
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

target_link_libraries(sentinelservice PRIVATE LibCore LibCrypto LibDatabase LibFileSystem LibIPC LibURL)
target_link_libraries(Sentinel PRIVATE sentinelservice LibCore LibMain)
target_link_libraries(TestPolicyGraph PRIVATE sentinelservice LibCore LibMain)
target_link_libraries(TestPhase3Integration PRIVATE sentinelservice LibCore LibMain LibFileSystem)
target_link_libraries(TestBackend PRIVATE sentinelservice LibCore LibMain LibFileSystem)
target_link_libraries(TestDownloadVetting PRIVATE sentinelservice LibCore LibMain LibFileSystem LibCrypto)
target_link_libraries(sentinel-cli PRIVATE sentinelservice LibCore LibMain LibFileSystem requestserverservice)

# Install targets
install(TARGETS Sentinel sentinel-cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY ${CMAKE_SOURCE_DIR}/Base/res/ladybird/sentinel/rules DESTINATION ${CMAKE_INSTALL_DATADIR}/ladybird/sentinel)
