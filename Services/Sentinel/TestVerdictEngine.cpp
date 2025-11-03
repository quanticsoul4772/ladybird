/*
 * Copyright (c) 2025, Ladybird contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/NonnullOwnPtr.h>
#include <LibTest/TestCase.h>

#include "Sandbox/VerdictEngine.h"

using namespace Sentinel::Sandbox;

TEST_CASE(test_weighted_scoring_calculation)
{
    auto verdict_engine = MUST(VerdictEngine::create());

    // Test case: YARA=0.8, ML=0.7, Behavioral=0.6
    // Expected: (0.8 * 0.4) + (0.7 * 0.35) + (0.6 * 0.25) = 0.32 + 0.245 + 0.15 = 0.715
    auto verdict = MUST(verdict_engine->calculate_verdict(0.8f, 0.7f, 0.6f));

    EXPECT_APPROXIMATE(verdict.composite_score, 0.715f);
    EXPECT(verdict.threat_level == SandboxResult::ThreatLevel::Malicious);
    EXPECT(verdict.composite_score >= 0.6f && verdict.composite_score < 0.8f);
}

TEST_CASE(test_threat_level_clean)
{
    auto verdict_engine = MUST(VerdictEngine::create());

    // Clean file: All scores low
    auto verdict = MUST(verdict_engine->calculate_verdict(0.1f, 0.05f, 0.15f));

    // Expected composite: (0.1 * 0.4) + (0.05 * 0.35) + (0.15 * 0.25) = 0.04 + 0.0175 + 0.0375 = 0.095
    EXPECT_APPROXIMATE(verdict.composite_score, 0.095f);
    EXPECT(verdict.threat_level == SandboxResult::ThreatLevel::Clean);
    EXPECT(verdict.composite_score < 0.3f);
    EXPECT(verdict.explanation.contains("clean"sv));
}

TEST_CASE(test_threat_level_suspicious)
{
    auto verdict_engine = MUST(VerdictEngine::create());

    // Suspicious file: Moderate scores
    auto verdict = MUST(verdict_engine->calculate_verdict(0.4f, 0.5f, 0.3f));

    // Expected composite: (0.4 * 0.4) + (0.5 * 0.35) + (0.3 * 0.25) = 0.16 + 0.175 + 0.075 = 0.41
    EXPECT_APPROXIMATE(verdict.composite_score, 0.41f);
    EXPECT(verdict.threat_level == SandboxResult::ThreatLevel::Suspicious);
    EXPECT(verdict.composite_score >= 0.3f && verdict.composite_score < 0.6f);
    EXPECT(verdict.explanation.contains("suspicious"sv));
}

TEST_CASE(test_threat_level_malicious)
{
    auto verdict_engine = MUST(VerdictEngine::create());

    // Malicious file: High scores
    auto verdict = MUST(verdict_engine->calculate_verdict(0.7f, 0.65f, 0.6f));

    // Expected composite: (0.7 * 0.4) + (0.65 * 0.35) + (0.6 * 0.25) = 0.28 + 0.2275 + 0.15 = 0.6575
    EXPECT_APPROXIMATE(verdict.composite_score, 0.6575f);
    EXPECT(verdict.threat_level == SandboxResult::ThreatLevel::Malicious);
    EXPECT(verdict.composite_score >= 0.6f && verdict.composite_score < 0.8f);
    EXPECT(verdict.explanation.contains("malicious"sv));
}

TEST_CASE(test_threat_level_critical)
{
    auto verdict_engine = MUST(VerdictEngine::create());

    // Critical threat: Very high scores
    auto verdict = MUST(verdict_engine->calculate_verdict(0.9f, 0.85f, 0.8f));

    // Expected composite: (0.9 * 0.4) + (0.85 * 0.35) + (0.8 * 0.25) = 0.36 + 0.2975 + 0.20 = 0.8575
    EXPECT_APPROXIMATE(verdict.composite_score, 0.8575f);
    EXPECT(verdict.threat_level == SandboxResult::ThreatLevel::Critical);
    EXPECT(verdict.composite_score >= 0.8f);
    EXPECT(verdict.explanation.contains("CRITICAL"sv));
}

TEST_CASE(test_confidence_high_when_all_agree)
{
    auto verdict_engine = MUST(VerdictEngine::create());

    // All detectors agree (all high)
    auto verdict_high = MUST(verdict_engine->calculate_verdict(0.85f, 0.88f, 0.82f));
    EXPECT(verdict_high.confidence >= 0.8f);

    // All detectors agree (all low)
    auto verdict_low = MUST(verdict_engine->calculate_verdict(0.05f, 0.08f, 0.03f));
    EXPECT(verdict_low.confidence >= 0.8f);
}

TEST_CASE(test_confidence_low_when_disagree)
{
    auto verdict_engine = MUST(VerdictEngine::create());

    // Detectors strongly disagree: YARA high, ML medium, Behavioral low
    auto verdict = MUST(verdict_engine->calculate_verdict(0.9f, 0.5f, 0.1f));

    // Low confidence due to high variance
    EXPECT(verdict.confidence < 0.6f);
}

TEST_CASE(test_confidence_medium_when_partial_agreement)
{
    auto verdict_engine = MUST(VerdictEngine::create());

    // Two detectors agree, one differs: YARA and ML high, Behavioral moderate
    auto verdict = MUST(verdict_engine->calculate_verdict(0.8f, 0.75f, 0.4f));

    // Medium confidence - moderate variance
    EXPECT(verdict.confidence >= 0.5f && verdict.confidence < 0.9f);
}

TEST_CASE(test_all_clean_scenario)
{
    auto verdict_engine = MUST(VerdictEngine::create());

    // Perfect clean file: All zeros
    auto verdict = MUST(verdict_engine->calculate_verdict(0.0f, 0.0f, 0.0f));

    EXPECT_APPROXIMATE(verdict.composite_score, 0.0f);
    EXPECT(verdict.threat_level == SandboxResult::ThreatLevel::Clean);
    EXPECT(verdict.confidence >= 0.9f); // High confidence - all agree
}

TEST_CASE(test_all_malicious_scenario)
{
    auto verdict_engine = MUST(VerdictEngine::create());

    // Perfect malicious file: All ones
    auto verdict = MUST(verdict_engine->calculate_verdict(1.0f, 1.0f, 1.0f));

    EXPECT_APPROXIMATE(verdict.composite_score, 1.0f);
    EXPECT(verdict.threat_level == SandboxResult::ThreatLevel::Critical);
    EXPECT(verdict.confidence >= 0.9f); // High confidence - all agree
}

TEST_CASE(test_boundary_clean_suspicious)
{
    auto verdict_engine = MUST(VerdictEngine::create());

    // Test boundary at 0.3 threshold
    auto verdict_below = MUST(verdict_engine->calculate_verdict(0.29f, 0.29f, 0.29f));
    EXPECT(verdict_below.threat_level == SandboxResult::ThreatLevel::Clean);

    auto verdict_at = MUST(verdict_engine->calculate_verdict(0.3f, 0.3f, 0.3f));
    EXPECT(verdict_at.threat_level == SandboxResult::ThreatLevel::Suspicious);
}

TEST_CASE(test_boundary_suspicious_malicious)
{
    auto verdict_engine = MUST(VerdictEngine::create());

    // Test boundary at 0.6 threshold
    auto verdict_below = MUST(verdict_engine->calculate_verdict(0.59f, 0.59f, 0.59f));
    EXPECT(verdict_below.threat_level == SandboxResult::ThreatLevel::Suspicious);

    auto verdict_at = MUST(verdict_engine->calculate_verdict(0.6f, 0.6f, 0.6f));
    EXPECT(verdict_at.threat_level == SandboxResult::ThreatLevel::Malicious);
}

TEST_CASE(test_boundary_malicious_critical)
{
    auto verdict_engine = MUST(VerdictEngine::create());

    // Test boundary at 0.8 threshold
    auto verdict_below = MUST(verdict_engine->calculate_verdict(0.79f, 0.79f, 0.79f));
    EXPECT(verdict_below.threat_level == SandboxResult::ThreatLevel::Malicious);

    auto verdict_at = MUST(verdict_engine->calculate_verdict(0.8f, 0.8f, 0.8f));
    EXPECT(verdict_at.threat_level == SandboxResult::ThreatLevel::Critical);
}

TEST_CASE(test_yara_dominant_detection)
{
    auto verdict_engine = MUST(VerdictEngine::create());

    // YARA detects malware, but ML and Behavioral don't
    auto verdict = MUST(verdict_engine->calculate_verdict(1.0f, 0.1f, 0.1f));

    // Composite: (1.0 * 0.4) + (0.1 * 0.35) + (0.1 * 0.25) = 0.4 + 0.035 + 0.025 = 0.46
    EXPECT_APPROXIMATE(verdict.composite_score, 0.46f);
    EXPECT(verdict.threat_level == SandboxResult::ThreatLevel::Suspicious);
    EXPECT(verdict.explanation.contains("Pattern matching"sv) || verdict.explanation.contains("signatures"sv));
}

TEST_CASE(test_ml_dominant_detection)
{
    auto verdict_engine = MUST(VerdictEngine::create());

    // ML detects malware, but YARA and Behavioral don't
    auto verdict = MUST(verdict_engine->calculate_verdict(0.1f, 1.0f, 0.1f));

    // Composite: (0.1 * 0.4) + (1.0 * 0.35) + (0.1 * 0.25) = 0.04 + 0.35 + 0.025 = 0.415
    EXPECT_APPROXIMATE(verdict.composite_score, 0.415f);
    EXPECT(verdict.threat_level == SandboxResult::ThreatLevel::Suspicious);
    EXPECT(verdict.explanation.contains("Machine learning"sv) || verdict.explanation.contains("model"sv));
}

TEST_CASE(test_behavioral_dominant_detection)
{
    auto verdict_engine = MUST(VerdictEngine::create());

    // Behavioral detects malware, but YARA and ML don't
    auto verdict = MUST(verdict_engine->calculate_verdict(0.1f, 0.1f, 1.0f));

    // Composite: (0.1 * 0.4) + (0.1 * 0.35) + (1.0 * 0.25) = 0.04 + 0.035 + 0.25 = 0.325
    EXPECT_APPROXIMATE(verdict.composite_score, 0.325f);
    EXPECT(verdict.threat_level == SandboxResult::ThreatLevel::Suspicious);
    EXPECT(verdict.explanation.contains("Behavioral"sv) || verdict.explanation.contains("runtime"sv));
}

TEST_CASE(test_custom_thresholds)
{
    // Create VerdictEngine with custom thresholds
    ScoringThresholds custom_thresholds;
    custom_thresholds.clean_threshold = 0.2f;
    custom_thresholds.suspicious_threshold = 0.5f;
    custom_thresholds.malicious_threshold = 0.7f;

    auto verdict_engine = MUST(VerdictEngine::create(custom_thresholds));

    // Score that would be Clean with default thresholds (0.25 < 0.3)
    // but Suspicious with custom thresholds (0.25 >= 0.2)
    auto verdict = MUST(verdict_engine->calculate_verdict(0.25f, 0.25f, 0.25f));

    EXPECT(verdict.threat_level == SandboxResult::ThreatLevel::Suspicious);
}

TEST_CASE(test_statistics_tracking)
{
    auto verdict_engine = MUST(VerdictEngine::create());

    // Initially all zero
    auto stats_initial = verdict_engine->get_statistics();
    EXPECT(stats_initial.total_verdicts == 0);

    // Generate some verdicts
    MUST(verdict_engine->calculate_verdict(0.1f, 0.1f, 0.1f)); // Clean
    MUST(verdict_engine->calculate_verdict(0.4f, 0.4f, 0.4f)); // Suspicious
    MUST(verdict_engine->calculate_verdict(0.7f, 0.7f, 0.7f)); // Malicious
    MUST(verdict_engine->calculate_verdict(0.9f, 0.9f, 0.9f)); // Critical

    auto stats = verdict_engine->get_statistics();
    EXPECT(stats.total_verdicts == 4);
    EXPECT(stats.clean == 1);
    EXPECT(stats.suspicious == 1);
    EXPECT(stats.malicious == 1);
    EXPECT(stats.critical == 1);
    EXPECT(stats.average_composite_score > 0.0f);
    EXPECT(stats.average_confidence > 0.0f);
}

TEST_CASE(test_statistics_reset)
{
    auto verdict_engine = MUST(VerdictEngine::create());

    // Generate some verdicts
    MUST(verdict_engine->calculate_verdict(0.5f, 0.5f, 0.5f));
    MUST(verdict_engine->calculate_verdict(0.6f, 0.6f, 0.6f));

    auto stats_before = verdict_engine->get_statistics();
    EXPECT(stats_before.total_verdicts == 2);

    // Reset statistics
    verdict_engine->reset_statistics();

    auto stats_after = verdict_engine->get_statistics();
    EXPECT(stats_after.total_verdicts == 0);
    EXPECT(stats_after.clean == 0);
    EXPECT(stats_after.suspicious == 0);
    EXPECT(stats_after.malicious == 0);
    EXPECT(stats_after.critical == 0);
}

TEST_CASE(test_explanation_multiple_detectors_agree)
{
    auto verdict_engine = MUST(VerdictEngine::create());

    // Multiple high scores - should mention agreement
    auto verdict = MUST(verdict_engine->calculate_verdict(0.8f, 0.75f, 0.7f));

    EXPECT(verdict.explanation.contains("Multiple"sv) || verdict.explanation.contains("agree"sv));
}

TEST_CASE(test_score_clamping)
{
    auto verdict_engine = MUST(VerdictEngine::create());

    // Test that scores beyond [0, 1] are clamped properly
    auto verdict = MUST(verdict_engine->calculate_verdict(1.5f, 1.2f, -0.1f));

    // Should still work without crashing and produce reasonable result
    EXPECT(verdict.composite_score >= 0.0f && verdict.composite_score <= 1.0f);
}

TEST_CASE(test_real_world_scenario_ransomware)
{
    auto verdict_engine = MUST(VerdictEngine::create());

    // Simulated ransomware detection:
    // - YARA: Matches ransomware signature (high)
    // - ML: Detected ransomware features (high)
    // - Behavioral: File encryption activity (very high)
    auto verdict = MUST(verdict_engine->calculate_verdict(0.85f, 0.78f, 0.92f));

    // Expected composite: (0.85 * 0.4) + (0.78 * 0.35) + (0.92 * 0.25) = 0.34 + 0.273 + 0.23 = 0.843
    EXPECT_APPROXIMATE(verdict.composite_score, 0.843f);
    EXPECT(verdict.threat_level == SandboxResult::ThreatLevel::Critical);
    EXPECT(verdict.confidence >= 0.8f); // High confidence - all agree
}

TEST_CASE(test_real_world_scenario_adware)
{
    auto verdict_engine = MUST(VerdictEngine::create());

    // Simulated adware detection:
    // - YARA: No specific signatures (low)
    // - ML: Detected some suspicious features (medium)
    // - Behavioral: Moderate suspicious behavior (medium)
    auto verdict = MUST(verdict_engine->calculate_verdict(0.1f, 0.5f, 0.45f));

    // Expected composite: (0.1 * 0.4) + (0.5 * 0.35) + (0.45 * 0.25) = 0.04 + 0.175 + 0.1125 = 0.3275
    EXPECT_APPROXIMATE(verdict.composite_score, 0.3275f);
    EXPECT(verdict.threat_level == SandboxResult::ThreatLevel::Suspicious);
}

TEST_CASE(test_real_world_scenario_false_positive)
{
    auto verdict_engine = MUST(VerdictEngine::create());

    // Simulated false positive scenario:
    // - YARA: False positive match (medium-high)
    // - ML: No malicious features (low)
    // - Behavioral: Normal behavior (very low)
    auto verdict = MUST(verdict_engine->calculate_verdict(0.6f, 0.15f, 0.05f));

    // Expected composite: (0.6 * 0.4) + (0.15 * 0.35) + (0.05 * 0.25) = 0.24 + 0.0525 + 0.0125 = 0.305
    EXPECT_APPROXIMATE(verdict.composite_score, 0.305f);
    EXPECT(verdict.threat_level == SandboxResult::ThreatLevel::Suspicious);
    EXPECT(verdict.confidence < 0.7f); // Lower confidence due to disagreement
}

