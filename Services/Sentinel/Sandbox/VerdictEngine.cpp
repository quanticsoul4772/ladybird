/*
 * Copyright (c) 2025, Ladybird contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Debug.h>
#include <AK/Math.h>
#include <AK/NonnullOwnPtr.h>

#include "VerdictEngine.h"

namespace Sentinel::Sandbox {

ErrorOr<NonnullOwnPtr<VerdictEngine>> VerdictEngine::create()
{
    return create(ScoringThresholds {});
}

ErrorOr<NonnullOwnPtr<VerdictEngine>> VerdictEngine::create(ScoringThresholds const& thresholds)
{
    return adopt_own(*new VerdictEngine(thresholds));
}

VerdictEngine::VerdictEngine(ScoringThresholds const& thresholds)
    : m_thresholds(thresholds)
{
}

VerdictEngine::~VerdictEngine() = default;

ErrorOr<Verdict> VerdictEngine::calculate_verdict(
    float yara_score,
    float ml_score,
    float behavioral_score) const
{
    dbgln_if(false, "VerdictEngine: Calculating verdict - YARA: {:.2f}, ML: {:.2f}, Behavioral: {:.2f}",
        yara_score, ml_score, behavioral_score);

    m_stats.total_verdicts++;

    Verdict verdict;

    // Calculate composite score (weighted average)
    verdict.composite_score = calculate_composite_score(yara_score, ml_score, behavioral_score);

    // Calculate confidence (agreement between detectors)
    verdict.confidence = calculate_confidence(yara_score, ml_score, behavioral_score);

    // Determine threat level based on composite score
    verdict.threat_level = determine_threat_level(verdict.composite_score);

    // Generate human-readable explanation
    verdict.explanation = TRY(generate_explanation(
        verdict.threat_level,
        verdict.composite_score,
        yara_score,
        ml_score,
        behavioral_score));

    // Update statistics
    switch (verdict.threat_level) {
    case SandboxResult::ThreatLevel::Clean:
        m_stats.clean++;
        break;
    case SandboxResult::ThreatLevel::Suspicious:
        m_stats.suspicious++;
        break;
    case SandboxResult::ThreatLevel::Malicious:
        m_stats.malicious++;
        break;
    case SandboxResult::ThreatLevel::Critical:
        m_stats.critical++;
        break;
    }

    if (m_stats.total_verdicts > 0) {
        m_stats.average_composite_score = (m_stats.average_composite_score * (m_stats.total_verdicts - 1) + verdict.composite_score) / m_stats.total_verdicts;
        m_stats.average_confidence = (m_stats.average_confidence * (m_stats.total_verdicts - 1) + verdict.confidence) / m_stats.total_verdicts;
    }

    dbgln_if(false, "VerdictEngine: Verdict - Level: {}, Composite: {:.2f}, Confidence: {:.2f}",
        static_cast<int>(verdict.threat_level), verdict.composite_score, verdict.confidence);

    return verdict;
}

float VerdictEngine::calculate_composite_score(float yara, float ml, float behavioral) const
{
    // Weighted average: YARA (40%) + ML (35%) + Behavioral (25%)
    float composite = (yara * 0.40f) + (ml * 0.35f) + (behavioral * 0.25f);
    return min(1.0f, max(0.0f, composite));
}

float VerdictEngine::calculate_confidence(float yara, float ml, float behavioral) const
{
    // Confidence based on agreement between detectors
    // High confidence when all agree (all high or all low)
    // Low confidence when they disagree

    float mean = (yara + ml + behavioral) / 3.0f;

    // Calculate variance (how much scores differ from mean)
    float variance = (AK::pow(yara - mean, 2.0f) + AK::pow(ml - mean, 2.0f) + AK::pow(behavioral - mean, 2.0f)) / 3.0f;

    // Standard deviation
    float stddev = AK::sqrt(variance);

    // Confidence inversely proportional to disagreement
    // Low stddev = high confidence, high stddev = low confidence
    float confidence = 1.0f - min(1.0f, stddev * 2.0f);

    // Boost confidence if all scores are extreme (all very high or very low)
    if ((yara > 0.8f && ml > 0.8f && behavioral > 0.8f) || (yara < 0.2f && ml < 0.2f && behavioral < 0.2f)) {
        confidence = max(confidence, 0.9f);
    }

    return min(1.0f, max(0.0f, confidence));
}

SandboxResult::ThreatLevel VerdictEngine::determine_threat_level(float composite_score) const
{
    if (composite_score < m_thresholds.clean_threshold)
        return SandboxResult::ThreatLevel::Clean;

    if (composite_score < m_thresholds.suspicious_threshold)
        return SandboxResult::ThreatLevel::Suspicious;

    if (composite_score < m_thresholds.malicious_threshold)
        return SandboxResult::ThreatLevel::Malicious;

    return SandboxResult::ThreatLevel::Critical;
}

ErrorOr<String> VerdictEngine::generate_explanation(
    SandboxResult::ThreatLevel level,
    float composite_score,
    float yara,
    float ml,
    float behavioral) const
{
    StringBuilder sb;

    switch (level) {
    case SandboxResult::ThreatLevel::Clean:
        sb.append("File appears clean. "sv);
        break;
    case SandboxResult::ThreatLevel::Suspicious:
        sb.append("File exhibits suspicious behavior. "sv);
        break;
    case SandboxResult::ThreatLevel::Malicious:
        sb.append("File is likely malicious. "sv);
        break;
    case SandboxResult::ThreatLevel::Critical:
        sb.append("CRITICAL THREAT DETECTED. "sv);
        break;
    }

    TRY(sb.try_appendff("Overall threat score: {:.0f}%. ", composite_score * 100.0f));

    // Explain primary detection method
    float max_score = max(max(yara, ml), behavioral);
    if (max_score == yara && yara > 0.5f) {
        TRY(sb.try_appendff("Pattern matching detected malware signatures ({:.0f}%). ", yara * 100.0f));
    } else if (max_score == ml && ml > 0.5f) {
        TRY(sb.try_appendff("Machine learning model flagged malicious features ({:.0f}%). ", ml * 100.0f));
    } else if (max_score == behavioral && behavioral > 0.5f) {
        TRY(sb.try_appendff("Behavioral analysis detected suspicious runtime activity ({:.0f}%). ", behavioral * 100.0f));
    }

    // Indicate if multiple detectors agree
    u32 high_score_count = 0;
    if (yara > 0.5f) high_score_count++;
    if (ml > 0.5f) high_score_count++;
    if (behavioral > 0.5f) high_score_count++;

    if (high_score_count >= 2) {
        TRY(sb.try_append("Multiple detection methods agree. "sv));
    }

    return sb.to_string();
}

void VerdictEngine::update_thresholds(ScoringThresholds const& thresholds)
{
    m_thresholds = thresholds;
    dbgln_if(false, "VerdictEngine: Thresholds updated - Clean: {:.2f}, Suspicious: {:.2f}, Malicious: {:.2f}",
        m_thresholds.clean_threshold, m_thresholds.suspicious_threshold, m_thresholds.malicious_threshold);
}

void VerdictEngine::reset_statistics()
{
    m_stats = Statistics {};
    dbgln_if(false, "VerdictEngine: Statistics reset");
}

}
