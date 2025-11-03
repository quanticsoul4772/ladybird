/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "YARAGenerator.h"
#include <AK/StringBuilder.h>
#include <LibCore/File.h>

namespace Sentinel::ThreatIntelligence {

ErrorOr<String> YARAGenerator::generate_hash_rule(PolicyGraph::IOC const& ioc)
{
    if (ioc.type != PolicyGraph::IOC::Type::FileHash)
        return Error::from_string_literal("IOC is not a file hash");

    StringBuilder rule;

    // Generate rule name from hash prefix
    auto hash_prefix = ioc.indicator.bytes_as_string_view().substring_view(0, min(32ul, ioc.indicator.bytes_as_string_view().length()));
    auto rule_name = TRY(sanitize_rule_name(TRY(String::formatted("otx_hash_{}", hash_prefix))));

    rule.append("rule "sv);
    rule.append(rule_name);
    rule.append(" {\n"sv);

    // Meta section
    rule.append("    meta:\n"sv);
    rule.append("        description = \""sv);
    if (ioc.description.has_value())
        rule.append(TRY(escape_yara_string(ioc.description.value())));
    else
        rule.append("File hash from OTX feed"sv);
    rule.append("\"\n"sv);

    rule.append("        source = \""sv);
    rule.append(ioc.source);
    rule.append("\"\n"sv);

    rule.append("        date = \""sv);
    rule.append(TRY(String::formatted("{}", ioc.created_at.seconds_since_epoch())));
    rule.append("\"\n"sv);

    // Add tags if present
    if (!ioc.tags.is_empty()) {
        rule.append("        tags = \""sv);
        for (size_t i = 0; i < ioc.tags.size(); ++i) {
            if (i > 0)
                rule.append(", "sv);
            rule.append(ioc.tags[i]);
        }
        rule.append("\"\n"sv);
    }

    // Condition section - determine hash type from length
    rule.append("\n    condition:\n"sv);

    auto hash_len = ioc.indicator.bytes_as_string_view().length();
    if (hash_len == 64) {
        rule.append("        hash.sha256(0, filesize) == \""sv);
    } else if (hash_len == 40) {
        rule.append("        hash.sha1(0, filesize) == \""sv);
    } else if (hash_len == 32) {
        rule.append("        hash.md5(0, filesize) == \""sv);
    } else {
        return Error::from_string_literal("Unknown hash type");
    }

    rule.append(ioc.indicator);
    rule.append("\"\n"sv);
    rule.append("}\n"sv);

    return rule.to_string();
}

ErrorOr<String> YARAGenerator::generate_pattern_rule(String const& rule_name,
    Vector<String> const& patterns,
    String const& description)
{
    StringBuilder rule;

    auto sanitized_name = TRY(sanitize_rule_name(rule_name));

    rule.append("rule "sv);
    rule.append(sanitized_name);
    rule.append(" {\n"sv);

    // Meta section
    rule.append("    meta:\n"sv);
    rule.append("        description = \""sv);
    rule.append(TRY(escape_yara_string(description)));
    rule.append("\"\n"sv);

    // Strings section
    rule.append("\n    strings:\n"sv);
    for (size_t i = 0; i < patterns.size(); ++i) {
        rule.append("        $s"sv);
        rule.append(TRY(String::formatted("{}", i)));
        rule.append(" = \""sv);
        rule.append(TRY(escape_yara_string(patterns[i])));
        rule.append("\"\n"sv);
    }

    // Condition - match any string
    rule.append("\n    condition:\n"sv);
    rule.append("        any of ($s*)\n"sv);
    rule.append("}\n"sv);

    return rule.to_string();
}

ErrorOr<void> YARAGenerator::generate_rules_file(Vector<PolicyGraph::IOC> const& iocs,
    String const& output_path)
{
    StringBuilder content;

    content.append("// Auto-generated YARA rules from IOCs\n"sv);
    content.append("// Generated at: "sv);
    content.append(TRY(String::formatted("{}\n\n", UnixDateTime::now().seconds_since_epoch())));

    for (auto const& ioc : iocs) {
        if (ioc.type != PolicyGraph::IOC::Type::FileHash)
            continue; // Only generate rules for file hashes

        auto rule_result = generate_hash_rule(ioc);
        if (rule_result.is_error()) {
            dbgln("YARAGenerator: Warning - failed to generate rule for {}: {}",
                ioc.indicator, rule_result.error());
            continue;
        }

        content.append(rule_result.value());
        content.append("\n"sv);
    }

    // Write to file
    auto file = TRY(Core::File::open(output_path, Core::File::OpenMode::Write));
    TRY(file->write_until_depleted(TRY(content.to_string()).bytes()));

    dbgln("YARAGenerator: Generated rules file {}", output_path);
    return {};
}

ErrorOr<bool> YARAGenerator::validate_rule_syntax(String const& rule_content)
{
    // Basic syntax validation
    // Check for required keywords
    if (!rule_content.contains("rule"sv))
        return false;
    if (!rule_content.contains("condition:"sv))
        return false;

    // Check for balanced braces
    int brace_count = 0;
    for (auto c : rule_content.bytes_as_string_view()) {
        if (c == '{')
            brace_count++;
        else if (c == '}')
            brace_count--;
    }

    return brace_count == 0;
}

ErrorOr<String> YARAGenerator::sanitize_rule_name(String const& name)
{
    StringBuilder sanitized;

    for (auto c : name.bytes_as_string_view()) {
        // Allow alphanumeric and underscore only
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_') {
            sanitized.append(c);
        } else {
            sanitized.append('_');
        }
    }

    return sanitized.to_string();
}

ErrorOr<String> YARAGenerator::escape_yara_string(String const& str)
{
    StringBuilder escaped;

    for (auto c : str.bytes_as_string_view()) {
        if (c == '"' || c == '\\') {
            escaped.append('\\');
        }
        escaped.append(c);
    }

    return escaped.to_string();
}

}
