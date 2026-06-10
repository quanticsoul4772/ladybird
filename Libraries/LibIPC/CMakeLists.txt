set(SOURCES
    AutoCloseFileDescriptor.cpp
    BufferedIPCReader.cpp
    BufferedIPCWriter.cpp
    Connection.cpp
    Decoder.cpp
    Encoder.cpp
    File.cpp
    IPFSAPIClient.cpp
    IPFSVerifier.cpp
    Message.cpp
    Multibase.cpp
    Multicodec.cpp
    Multihash.cpp
    NetworkIdentity.cpp
    ProxyValidator.cpp
    TransportHandle.cpp
)

if (APPLE AND NOT IOS)
    list(APPEND SOURCES
        AttachmentMachPort.cpp
        MachBootstrapListener.cpp
        TransportBootstrapMach.cpp
        TransportMachPort.cpp)
elseif (UNIX)
    list(APPEND SOURCES
        Attachment.cpp
        TransportSocket.cpp)
else()
    list(APPEND SOURCES
        Attachment.cpp
        TransportSocketWindows.cpp)
endif()

ladybird_lib(LibIPC ipc)
target_link_libraries(LibIPC PRIVATE LibCore LibCrypto LibSync LibThreading LibURL)
