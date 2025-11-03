/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>
#include <AK/String.h>
#include <AK/Vector.h>
#include <Services/Sentinel/PolicyGraph.h>

namespace Sentinel::ThreatIntelligence {

// YARA rule generator for IOCs
// Generates YARA rules from file hashes, string patterns, and behavioral indicators
class YARAGenerator {
public:
    // Generate YARA rule from file hash IOC
    static ErrorOr<String> generate_hash_rule(PolicyGraph::IOC const& ioc);

    // Generate YARA rule from string pattern
    static ErrorOr<String> generate_pattern_rule(String const& rule_name,
        Vector<String> const& patterns,
        String const& description);

    // Generate YARA rules file from multiple IOCs
    static ErrorOr<void> generate_rules_file(Vector<PolicyGraph::IOC> const& iocs,
        String const& output_path);

    // Validate YARA rule syntax (basic check)
    static ErrorOr<bool> validate_rule_syntax(String const& rule_content);

private:
    // Sanitize string for YARA rule name (remove special chars)
    static ErrorOr<String> sanitize_rule_name(String const& name);

    // Escape string for YARA string literal
    static ErrorOr<String> escape_yara_string(String const& str);
};

}
