set(SOURCES
    AutoCloseFileDescriptor.cpp
    BufferedIPCReader.cpp
    BufferedIPCWriter.cpp
    Connection.cpp
    Decoder.cpp
    Encoder.cpp
    IPFSAPIClient.cpp
    IPFSVerifier.cpp
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
        File.cpp
        MachBootstrapListener.cpp
        Message.cpp
        TransportBootstrapMach.cpp
        TransportMachPort.cpp)
elseif (UNIX)
    list(APPEND SOURCES
        Attachment.cpp
        File.cpp
        Message.cpp
        TransportSocket.cpp)
else()
    list(APPEND SOURCES
        Attachment.cpp
        FileWindows.cpp
        MessageWindows.cpp
        TransportSocketWindows.cpp)
endif()

ladybird_lib(LibIPC ipc)
target_link_libraries(LibIPC PRIVATE LibCore LibCrypto LibURL LibThreading)
