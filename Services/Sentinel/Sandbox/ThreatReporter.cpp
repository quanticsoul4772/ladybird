/*
 * Copyright (c) 2025, Ladybird contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "ThreatReporter.h"
#include <AK/NonnullOwnPtr.h>
#include <AK/StringBuilder.h>

namespace Sentinel::Sandbox {

ErrorOr<NonnullOwnPtr<ThreatReporter>> ThreatReporter::create()
{
    return adopt_nonnull_own_or_enomem(new (nothrow) ThreatReporter());
}

ThreatReporter::~ThreatReporter() = default;

void ThreatReporter::reset_statistics()
{
    m_stats = Statistics {};
}

ErrorOr<String> ThreatReporter::get_severity_emoji(SandboxResult::ThreatLevel level) const
{
    switch (level) {
    case SandboxResult::ThreatLevel::Clean:
        return String::from_utf8("\xF0\x9F\x9F\xA2"sv); // 🟢
    case SandboxResult::ThreatLevel::Suspicious:
        return String::from_utf8("\xF0\x9F\x9F\xA1"sv); // 🟡
    case SandboxResult::ThreatLevel::Malicious:
        return String::from_utf8("\xF0\x9F\x9F\xA0"sv); // 🟠
    case SandboxResult::ThreatLevel::Critical:
        return String::from_utf8("\xF0\x9F\x94\xB4"sv); // 🔴
    }
    return String::from_utf8("\xE2\x9A\xAA"sv); // ⚪ (default)
}

ErrorOr<String> ThreatReporter::get_severity_label(SandboxResult::ThreatLevel level) const
{
    switch (level) {
    case SandboxResult::ThreatLevel::Clean:
        return "LOW"_string;
    case SandboxResult::ThreatLevel::Suspicious:
        return "MEDIUM"_string;
    case SandboxResult::ThreatLevel::Malicious:
        return "HIGH"_string;
    case SandboxResult::ThreatLevel::Critical:
        return "CRITICAL"_string;
    }
    return "UNKNOWN"_string;
}

ErrorOr<String> ThreatReporter::get_confidence_label(float confidence) const
{
    if (confidence >= 0.8f)
        return "High"_string;
    if (confidence >= 0.5f)
        return "Medium"_string;
    return "Low"_string;
}

ErrorOr<String> ThreatReporter::format_detection_summary(SandboxResult const& result) const
{
    StringBuilder builder;
    builder.append("## Detection Summary\n"sv);

    // YARA detection
    if (result.yara_score > 0.5f) {
        builder.append("\xE2\x9C\x93"sv); // ✓
        builder.append(" YARA: Detected "sv);
        if (!result.triggered_rules.is_empty()) {
            builder.append(result.triggered_rules[0]);
            builder.append(" signature"sv);
        } else {
            builder.append("malicious signature"sv);
        }
        builder.append('\n');
    } else {
        builder.append("\xE2\x9C\x97"sv); // ✗
        builder.append(" YARA: No signature match\n"sv);
    }

    // ML detection
    if (result.ml_score > 0.5f) {
        builder.append("\xE2\x9C\x93"sv); // ✓
        builder.appendff(" Machine Learning: {}% probability of malware\n", static_cast<int>(result.ml_score * 100));
    } else {
        builder.append("\xE2\x9C\x97"sv); // ✗
        builder.append(" Machine Learning: Low threat probability\n"sv);
    }

    // Behavioral detection
    if (result.behavioral_score > 0.5f) {
        builder.append("\xE2\x9C\x93"sv); // ✓
        builder.append(" Behavioral Analysis: "sv);
        if (!result.detected_behaviors.is_empty()) {
            builder.append(result.detected_behaviors[0]);
            builder.append(" detected"sv);
        } else {
            builder.append("Suspicious patterns detected"sv);
        }
        builder.append('\n');
    } else {
        builder.append("\xE2\x9C\x97"sv); // ✗
        builder.append(" Behavioral Analysis: No suspicious patterns\n"sv);
    }

    return builder.to_string();
}

ErrorOr<String> ThreatReporter::format_threat_behaviors(SandboxResult const& result) const
{
    if (result.detected_behaviors.is_empty() && result.file_operations == 0 && result.network_operations == 0)
        return "No suspicious behaviors detected."_string;

    StringBuilder builder;
    builder.append("## Threat Behaviors\n"sv);

    // List detected behaviors from analysis
    for (auto const& behavior : result.detected_behaviors) {
        builder.append("\xE2\x80\xA2"sv); // •
        builder.append(' ');
        builder.append(behavior);
        builder.append('\n');
    }

    // Add metrics-based behaviors
    if (result.file_operations > 50) {
        builder.append("\xE2\x80\xA2"sv); // •
        builder.appendff(" File Operations: {} file system operations detected\n", result.file_operations);
    }

    if (result.network_operations > 0) {
        builder.append("\xE2\x80\xA2"sv); // •
        builder.appendff(" Network: {} network operations attempted\n", result.network_operations);
    }

    if (result.process_operations > 0) {
        builder.append("\xE2\x80\xA2"sv); // •
        builder.appendff(" Process Control: {} process operations detected\n", result.process_operations);
    }

    if (result.memory_operations > 20) {
        builder.append("\xE2\x80\xA2"sv); // •
        builder.appendff(" Memory Operations: {} memory allocations/modifications\n", result.memory_operations);
    }

    return builder.to_string();
}

ErrorOr<String> ThreatReporter::get_action_for_threat_level(SandboxResult::ThreatLevel level) const
{
    switch (level) {
    case SandboxResult::ThreatLevel::Clean:
        return "\xE2\x9C\x85 File appears safe. Proceed with caution."_string; // ✅
    case SandboxResult::ThreatLevel::Suspicious:
        return "\xE2\x9A\xA0\xEF\xB8\x8F File exhibits suspicious patterns. Review carefully before opening."_string; // ⚠️
    case SandboxResult::ThreatLevel::Malicious:
        return "\xE2\x9B\x94 This file has been QUARANTINED and will not execute."_string; // ⛔
    case SandboxResult::ThreatLevel::Critical:
        return "\xF0\x9F\x9A\xA8 SEVERE THREAT. File blocked and quarantined. Report to security team."_string; // 🚨
    }
    return "File has been analyzed. Review results carefully."_string;
}

ErrorOr<String> ThreatReporter::get_reason_for_threat_level(SandboxResult const& result) const
{
    StringBuilder builder;

    if (result.threat_level >= SandboxResult::ThreatLevel::Malicious) {
        if (result.yara_score > 0.5f && result.ml_score > 0.5f) {
            builder.append("Multiple independent detection methods confirm malicious intent"sv);
        } else if (result.yara_score > 0.7f) {
            builder.append("Known malware signature detected"sv);
        } else if (result.ml_score > 0.7f) {
            builder.append("Machine learning model indicates high probability of malware"sv);
        } else if (result.behavioral_score > 0.7f) {
            builder.append("Behavioral analysis detected malicious patterns"sv);
        } else {
            builder.append("Composite threat score indicates malicious behavior"sv);
        }
    } else if (result.threat_level == SandboxResult::ThreatLevel::Suspicious) {
        if (result.behavioral_score > 0.5f) {
            builder.append("Behavioral analysis detected suspicious patterns"sv);
        } else if (result.ml_score > 0.4f) {
            builder.append("File characteristics match known suspicious patterns"sv);
        } else {
            builder.append("Some indicators suggest potential risk"sv);
        }
    } else {
        builder.append("No significant threat indicators detected"sv);
    }

    return builder.to_string();
}

ErrorOr<String> ThreatReporter::format_recommendation(SandboxResult const& result) const
{
    StringBuilder builder;
    builder.append("## Recommendation\n"sv);

    auto action = TRY(get_action_for_threat_level(result.threat_level));
    builder.append(action);
    builder.append("\n\n"sv);

    // Action guidance
    if (result.threat_level >= SandboxResult::ThreatLevel::Malicious) {
        builder.append("\xE2\x86\x92 Action: Delete this file immediately\n"sv); // →
    } else if (result.threat_level == SandboxResult::ThreatLevel::Suspicious) {
        builder.append("\xE2\x86\x92 Action: Review file origin and delete if suspicious\n"sv); // →
    } else {
        builder.append("\xE2\x86\x92 Action: File appears safe but verify source before opening\n"sv); // →
    }

    // Reasoning
    builder.append("\xE2\x86\x92 Why: "sv); // →
    auto reason = TRY(get_reason_for_threat_level(result));
    builder.append(reason);
    builder.append('\n');

    // Documentation link
    builder.append("\xE2\x86\x92 Learn More: https://ladybird.org/docs/sentinel/threat-detection\n"sv); // →

    return builder.to_string();
}

ErrorOr<String> ThreatReporter::format_technical_details(SandboxResult const& result) const
{
    StringBuilder builder;
    builder.append("## Technical Details\n"sv);

    // Scoring breakdown
    builder.appendff("YARA Score: {:.2f} (40% weight)\n", result.yara_score);
    builder.appendff("ML Score: {:.2f} (35% weight)\n", result.ml_score);
    builder.appendff("Behavioral Score: {:.2f} (25% weight)\n", result.behavioral_score);
    builder.appendff("Composite: {:.2f}\n", result.composite_score);

    // Execution time
    if (result.execution_time.to_milliseconds() > 0) {
        builder.appendff("Analysis Time: {}ms\n", result.execution_time.to_milliseconds());
    }

    // Triggered rules (YARA)
    if (!result.triggered_rules.is_empty()) {
        builder.append("\nTriggered Rules:\n"sv);
        for (auto const& rule : result.triggered_rules) {
            builder.append("  - "sv);
            builder.append(rule);
            builder.append('\n');
        }
    }

    return builder.to_string();
}

ErrorOr<String> ThreatReporter::format_verdict(SandboxResult const& result, String const& filename) const
{
    // Update statistics
    m_stats.total_reports++;
    switch (result.threat_level) {
    case SandboxResult::ThreatLevel::Clean:
        m_stats.clean_reports++;
        break;
    case SandboxResult::ThreatLevel::Suspicious:
        m_stats.suspicious_reports++;
        break;
    case SandboxResult::ThreatLevel::Malicious:
        m_stats.malicious_reports++;
        break;
    case SandboxResult::ThreatLevel::Critical:
        m_stats.critical_reports++;
        break;
    }

    StringBuilder builder;

    // Header with emoji and severity
    auto emoji = TRY(get_severity_emoji(result.threat_level));
    auto severity = TRY(get_severity_label(result.threat_level));
    builder.append(emoji);
    builder.append(' ');
    builder.append(severity);
    builder.append(" THREAT DETECTED\n\n"sv);

    // File information
    builder.append("File: "sv);
    builder.append(filename);
    builder.append('\n');

    // Threat level and confidence
    builder.append("Threat Level: "sv);
    auto threat_name = [](SandboxResult::ThreatLevel level) -> StringView {
        switch (level) {
        case SandboxResult::ThreatLevel::Clean: return "Clean"sv;
        case SandboxResult::ThreatLevel::Suspicious: return "Suspicious"sv;
        case SandboxResult::ThreatLevel::Malicious: return "Malicious"sv;
        case SandboxResult::ThreatLevel::Critical: return "Critical"sv;
        }
        return "Unknown"sv;
    };
    builder.append(threat_name(result.threat_level));
    builder.append(" (Confidence: "sv);
    auto confidence_label = TRY(get_confidence_label(result.confidence));
    builder.append(confidence_label);
    builder.append(")\n"sv);

    // Composite score
    builder.appendff("Composite Score: {:.2f}/1.0\n\n", result.composite_score);

    // Detection summary
    auto detection_summary = TRY(format_detection_summary(result));
    builder.append(detection_summary);
    builder.append('\n');

    // Threat behaviors
    auto behaviors = TRY(format_threat_behaviors(result));
    builder.append(behaviors);
    builder.append('\n');

    // Recommendation
    auto recommendation = TRY(format_recommendation(result));
    builder.append(recommendation);
    builder.append('\n');

    // Technical details
    auto technical = TRY(format_technical_details(result));
    builder.append(technical);

    return builder.to_string();
}

ErrorOr<String> ThreatReporter::format_summary(SandboxResult const& result, String const& filename) const
{
    // Update statistics
    m_stats.total_reports++;
    switch (result.threat_level) {
    case SandboxResult::ThreatLevel::Clean:
        m_stats.clean_reports++;
        break;
    case SandboxResult::ThreatLevel::Suspicious:
        m_stats.suspicious_reports++;
        break;
    case SandboxResult::ThreatLevel::Malicious:
        m_stats.malicious_reports++;
        break;
    case SandboxResult::ThreatLevel::Critical:
        m_stats.critical_reports++;
        break;
    }

    StringBuilder builder;

    // Emoji and threat level
    auto emoji = TRY(get_severity_emoji(result.threat_level));
    auto severity = TRY(get_severity_label(result.threat_level));
    builder.append(emoji);
    builder.append(' ');
    builder.append(severity);
    builder.append(" THREAT: "sv);
    builder.append(filename);
    builder.append('\n');

    // Quick action
    auto action = TRY(get_action_for_threat_level(result.threat_level));
    builder.append(action);
    builder.append('\n');

    // Score
    builder.appendff("Score: {:.2f}/1.0", result.composite_score);

    return builder.to_string();
}

}
