/*
 * Copyright (c) 2025, Ladybird contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "Orchestrator.h"
#include <AK/Error.h>
#include <AK/NonnullOwnPtr.h>
#include <AK/String.h>

namespace Sentinel::Sandbox {

// ThreatReporter - User-friendly threat report formatting
// Milestone 0.5 Phase 1d: Orchestrator Integration
//
// Converts technical SandboxResult data into clear, actionable reports
// for display in browser UI. Non-technical users should understand:
// - What was detected
// - How serious the threat is
// - What action to take
class ThreatReporter {
public:
    static ErrorOr<NonnullOwnPtr<ThreatReporter>> create();

    ~ThreatReporter();

    // Generate full threat report with all details
    // Includes: severity, detection summary, behaviors, recommendation, technical details
    ErrorOr<String> format_verdict(SandboxResult const& result, String const& filename) const;

    // Generate short summary report for notifications
    // Includes: severity emoji, threat level, filename, quick action
    ErrorOr<String> format_summary(SandboxResult const& result, String const& filename) const;

    // Statistics
    struct Statistics {
        u64 total_reports { 0 };
        u64 clean_reports { 0 };
        u64 suspicious_reports { 0 };
        u64 malicious_reports { 0 };
        u64 critical_reports { 0 };
    };

    Statistics get_statistics() const { return m_stats; }
    void reset_statistics();

private:
    ThreatReporter() = default;

    // Format helper methods
    ErrorOr<String> get_severity_emoji(SandboxResult::ThreatLevel level) const;
    ErrorOr<String> get_severity_label(SandboxResult::ThreatLevel level) const;
    ErrorOr<String> get_confidence_label(float confidence) const;
    ErrorOr<String> format_detection_summary(SandboxResult const& result) const;
    ErrorOr<String> format_threat_behaviors(SandboxResult const& result) const;
    ErrorOr<String> format_recommendation(SandboxResult const& result) const;
    ErrorOr<String> format_technical_details(SandboxResult const& result) const;
    ErrorOr<String> get_action_for_threat_level(SandboxResult::ThreatLevel level) const;
    ErrorOr<String> get_reason_for_threat_level(SandboxResult const& result) const;

    mutable Statistics m_stats;
};

}
