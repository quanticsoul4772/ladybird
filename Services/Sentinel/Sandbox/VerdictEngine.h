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

// Verdict result with multi-layer scoring
struct Verdict {
    SandboxResult::ThreatLevel threat_level { SandboxResult::ThreatLevel::Clean };
    float composite_score { 0.0f };               // 0.0-1.0 weighted score
    float confidence { 0.0f };                    // 0.0-1.0 confidence in verdict
    String explanation;                           // User-facing summary

    // Component scores (for transparency)
    float yara_weight { 0.30f };                  // 30%
    float ml_weight { 0.25f };                    // 25%
    float behavioral_weight { 0.20f };            // 20%
    float vt_weight { 0.25f };                    // 25% - VirusTotal
};

// Scoring thresholds for threat levels
struct ScoringThresholds {
    float clean_threshold { 0.3f };               // < 0.3 = Clean
    float suspicious_threshold { 0.6f };          // 0.3-0.6 = Suspicious
    float malicious_threshold { 0.8f };           // 0.6-0.8 = Malicious
                                                   // >= 0.8 = Critical
};

// VerdictEngine - Multi-layer threat scoring and verdict generation
// Milestone 0.5 Phase 1: Real-time Sandboxing
//
// Combines YARA (40%), ML (35%), and Behavioral (25%) scores into
// a final weighted verdict with confidence scoring.
class VerdictEngine {
public:
    static ErrorOr<NonnullOwnPtr<VerdictEngine>> create();
    static ErrorOr<NonnullOwnPtr<VerdictEngine>> create(ScoringThresholds const& thresholds);

    ~VerdictEngine();

    // Generate verdict from multi-layer scores (4-way scoring)
    ErrorOr<Verdict> calculate_verdict(
        float yara_score,
        float ml_score,
        float behavioral_score,
        float vt_score = 0.0f) const;

    // Update scoring thresholds (for tuning)
    void update_thresholds(ScoringThresholds const& thresholds);
    ScoringThresholds const& thresholds() const { return m_thresholds; }

    // Statistics
    struct Statistics {
        u64 total_verdicts { 0 };
        u64 clean { 0 };
        u64 suspicious { 0 };
        u64 malicious { 0 };
        u64 critical { 0 };
        float average_composite_score { 0.0f };
        float average_confidence { 0.0f };
    };

    Statistics get_statistics() const { return m_stats; }
    void reset_statistics();

private:
    explicit VerdictEngine(ScoringThresholds const& thresholds);

    // Scoring helpers
    float calculate_composite_score(float yara, float ml, float behavioral, float vt) const;
    float calculate_confidence(float yara, float ml, float behavioral, float vt) const;
    SandboxResult::ThreatLevel determine_threat_level(float composite_score) const;
    ErrorOr<String> generate_explanation(
        SandboxResult::ThreatLevel level,
        float composite_score,
        float yara,
        float ml,
        float behavioral,
        float vt) const;

    ScoringThresholds m_thresholds;
    mutable Statistics m_stats;
};

}
