/*
 * Copyright (c) 2025, Ladybird contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

// Example usage of syscall event parser
// This demonstrates the parser with sample nsjail output

#include "BehavioralAnalyzer.h"
#include <AK/String.h>
#include <AK/StringView.h>

using namespace Sentinel::Sandbox;

// Example nsjail stderr output lines
static constexpr auto EXAMPLE_LINES = Array {
    // Valid syscall events
    "[SYSCALL] open(\"/tmp/file.txt\", O_RDONLY)"sv,
    "[SYSCALL] write(3, \"data\", 4)"sv,
    "[SYSCALL] socket(AF_INET, SOCK_STREAM, 0)"sv,
    "[SYSCALL] mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)"sv,
    "[SYSCALL] getpid"sv,  // No arguments
    "[SYSCALL] read(3, 0x7ffe12345678, 1024)"sv,

    // Non-syscall lines (should be ignored)
    "[INFO] nsjail started"sv,
    "[DEBUG] Sandbox initialized"sv,
    "Some other log line"sv,
    ""sv,  // Empty line

    // Malformed syscall lines (graceful handling)
    "[SYSCALL] "sv,  // Empty content
    "[SYSCALL] ("sv,  // Empty name
    "[SYSCALL] incomplete("sv,  // Missing closing paren
};

void demonstrate_parser()
{
    dbgln("=== Syscall Event Parser Examples ===\n");

    for (auto const& line : EXAMPLE_LINES) {
        dbgln("Input: \"{}\"", line);

        // This would be called in real code (requires BehavioralAnalyzer instance)
        // auto result = analyzer->parse_syscall_event(line);
        //
        // if (result.is_error()) {
        //     dbgln("  ERROR: {}", result.error());
        // } else if (result.value().has_value()) {
        //     auto const& event = result.value().value();
        //     dbgln("  Parsed: name='{}', args={}", event.name, event.args.size());
        //     for (size_t i = 0; i < event.args.size(); ++i) {
        //         dbgln("    arg[{}]: '{}'", i, event.args[i]);
        //     }
        // } else {
        //     dbgln("  Skipped: not a syscall line");
        // }

        dbgln("");
    }
}

// Expected output for valid syscall lines:
//
// Input: "[SYSCALL] open("/tmp/file.txt", O_RDONLY)"
//   Parsed: name='open', args=2
//     arg[0]: '"/tmp/file.txt"'
//     arg[1]: 'O_RDONLY'
//
// Input: "[SYSCALL] write(3, "data", 4)"
//   Parsed: name='write', args=3
//     arg[0]: '3'
//     arg[1]: '"data"'
//     arg[2]: '4'
//
// Input: "[SYSCALL] socket(AF_INET, SOCK_STREAM, 0)"
//   Parsed: name='socket', args=3
//     arg[0]: 'AF_INET'
//     arg[1]: 'SOCK_STREAM'
//     arg[2]: '0'
//
// Input: "[SYSCALL] getpid"
//   Parsed: name='getpid', args=0
//
// Input: "[INFO] nsjail started"
//   Skipped: not a syscall line
//
// Input: "[SYSCALL] "
//   Skipped: malformed (empty content)
