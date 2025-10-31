/*
 * Copyright (c) 2018-2020, Andreas Kling <andreas@ladybird.org>
 * Copyright (c) 2023, Andrew Kaster <akaster@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/ByteString.h>
#include <AK/Format.h>
#include <AK/LexicalPath.h>
#include <AK/StringView.h>
#include <AK/Vector.h>
#include <LibCore/ArgsParser.h>
#include <LibCore/EventLoop.h>
#include <LibCore/Process.h>
#include <LibCore/System.h>
#include <LibFileSystem/FileSystem.h>
#include <LibIPC/SingleServer.h>
#include <LibMain/Main.h>
#include <RequestServer/Cache/DiskCache.h>
#include <RequestServer/ConnectionFromClient.h>
#include <RequestServer/Resolver.h>
#include <RequestServer/SecurityTap.h>
#include <unistd.h>

#if defined(AK_OS_MACOS)
#    include <LibCore/Platform/ProcessStatisticsMach.h>
#endif

namespace RequestServer {

extern Optional<DiskCache> g_disk_cache;
SecurityTap* g_security_tap { nullptr };

}

ErrorOr<int> ladybird_main(Main::Arguments arguments)
{
    AK::set_rich_debug_enabled(true);

    Vector<ByteString> certificates;
    StringView mach_server_name;
    bool enable_http_disk_cache = false;
    bool wait_for_debugger = false;

    Core::ArgsParser args_parser;
    args_parser.add_option(certificates, "Path to a certificate file", "certificate", 'C', "certificate");
    args_parser.add_option(mach_server_name, "Mach server name", "mach-server-name", 0, "mach_server_name");
    args_parser.add_option(enable_http_disk_cache, "Enable HTTP disk cache", "enable-http-disk-cache");
    args_parser.add_option(wait_for_debugger, "Wait for debugger", "wait-for-debugger");
    args_parser.parse(arguments);

    if (wait_for_debugger)
        Core::Process::wait_for_debugger_and_break();

    // FIXME: Update RequestServer to support multiple custom root certificates.
    if (!certificates.is_empty())
        RequestServer::set_default_certificate_path(certificates.first());

    Core::EventLoop event_loop;

#if defined(AK_OS_MACOS)
    if (!mach_server_name.is_empty())
        Core::Platform::register_with_mach_server(mach_server_name);
#endif

    if (enable_http_disk_cache) {
        if (auto cache = RequestServer::DiskCache::create(); cache.is_error())
            warnln("Unable to create disk cache: {}", cache.error());
        else
            RequestServer::g_disk_cache = cache.release_value();
    }

    // Initialize SecurityTap for Sentinel integration
    auto security_tap = RequestServer::SecurityTap::create();
    if (security_tap.is_error()) {
        // Sentinel not available - try to auto-start it
        dbgln("RequestServer: SecurityTap initialization failed: {}", security_tap.error());
        dbgln("RequestServer: Attempting to auto-start Sentinel daemon...");

        // Try to find and start Sentinel binary
        // Get the directory containing the current executable
        auto exe_path_result = Core::System::current_executable_path();
        if (exe_path_result.is_error()) {
            dbgln("RequestServer: Failed to get current executable path: {}", exe_path_result.error());
            dbgln("RequestServer: Continuing without Sentinel security scanning");
        } else {
            auto exe_path = exe_path_result.release_value();
            auto exe_dir = LexicalPath::dirname(exe_path);

            // Look in the same directory as RequestServer (libexec)
            auto sentinel_path = ByteString::formatted("{}/Sentinel", exe_dir);

            // Alternative: check in bin directory (one level up from libexec)
            if (!FileSystem::exists(sentinel_path)) {
                sentinel_path = ByteString::formatted("{}/../bin/Sentinel", exe_dir);
            }

            if (FileSystem::exists(sentinel_path)) {
            dbgln("RequestServer: Found Sentinel at: {}", sentinel_path);

            // Spawn Sentinel as a detached background process
            auto spawn_result = Core::Process::spawn({
                .executable = sentinel_path,
                .search_for_executable_in_path = false,
                .arguments = Vector<ByteString> { "Sentinel"sv }
            });

            if (!spawn_result.is_error()) {
                dbgln("RequestServer: Sentinel daemon started successfully (PID: {})", spawn_result.value().pid());

                // Wait briefly for Sentinel to initialize its socket
                usleep(500000); // 500ms

                // Retry SecurityTap connection
                security_tap = RequestServer::SecurityTap::create();
                if (!security_tap.is_error()) {
                    RequestServer::g_security_tap = security_tap.release_value().leak_ptr();
                    dbgln("RequestServer: SecurityTap connected to auto-started Sentinel");
                } else {
                    dbgln("RequestServer: Failed to connect to Sentinel after auto-start: {}", security_tap.error());
                    dbgln("RequestServer: Continuing without Sentinel security scanning");
                }
            } else {
                dbgln("RequestServer: Failed to start Sentinel: {}", spawn_result.error());
                dbgln("RequestServer: Continuing without Sentinel security scanning");
            }
            } else {
                dbgln("RequestServer: Sentinel binary not found at expected locations");
                dbgln("RequestServer: Continuing without Sentinel security scanning");
            }
        }
    } else {
        RequestServer::g_security_tap = security_tap.release_value().leak_ptr();
        dbgln("RequestServer: SecurityTap initialized successfully");
    }

    auto client = TRY(IPC::take_over_accepted_client_from_system_server<RequestServer::ConnectionFromClient>());

    return event_loop.exec();
}
