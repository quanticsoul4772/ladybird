/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "FingerprintingDetector.h"
#include <AK/Math.h>
#include <AK/StringBuilder.h>

namespace Sentinel {

ErrorOr<NonnullOwnPtr<FingerprintingDetector>> FingerprintingDetector::create()
{
    return adopt_own(*new FingerprintingDetector());
}

void FingerprintingDetector::record_api_call(FingerprintingTechnique technique, StringView api_name, bool had_user_interaction)
{
    auto now = UnixDateTime::now();

    // Track first and last call times
    if (!m_first_call_time.has_value())
        m_first_call_time = now;
    m_last_call_time = now;

    // Track user interaction
    if (had_user_interaction)
        m_has_user_interaction = true;

    // Record the API call
    APICallEvent event;
    event.technique = technique;
    event.api_name = MUST(String::from_utf8(api_name));
    event.timestamp = now;
    event.had_user_interaction = had_user_interaction;

    // Add to technique-specific tracking
    if (!m_api_calls.contains(technique))
        m_api_calls.set(technique, Vector<APICallEvent>());

    m_api_calls.get(technique).value().append(move(event));
}

FingerprintingDetector::FingerprintingScore FingerprintingDetector::calculate_score() const
{
    FingerprintingScore score;

    // Count techniques and calls
    score.techniques_used = count_unique_techniques();
    score.total_api_calls = 0;
    for (auto const& [technique, calls] : m_api_calls)
        score.total_api_calls += calls.size();

    if (score.total_api_calls == 0) {
        score.explanation = "No fingerprinting activity detected"_string;
        return score;
    }

    // Time window analysis
    score.time_window_seconds = get_time_window_seconds();

    // Individual technique flags and counts
    score.uses_canvas = has_technique(FingerprintingTechnique::Canvas);
    score.uses_webgl = has_technique(FingerprintingTechnique::WebGL);
    score.uses_audio = has_technique(FingerprintingTechnique::AudioContext);
    score.uses_navigator = has_technique(FingerprintingTechnique::NavigatorEnumeration);
    score.uses_fonts = has_technique(FingerprintingTechnique::FontEnumeration);
    score.uses_screen = has_technique(FingerprintingTechnique::ScreenProperties);

    score.canvas_calls = count_calls_for_technique(FingerprintingTechnique::Canvas);
    score.webgl_calls = count_calls_for_technique(FingerprintingTechnique::WebGL);
    score.audio_calls = count_calls_for_technique(FingerprintingTechnique::AudioContext);
    score.navigator_calls = count_calls_for_technique(FingerprintingTechnique::NavigatorEnumeration);
    score.font_calls = count_calls_for_technique(FingerprintingTechnique::FontEnumeration);
    score.screen_calls = count_calls_for_technique(FingerprintingTechnique::ScreenProperties);

    // Rapid fire detection (multiple API calls in short time)
    score.rapid_fire_detected = (score.total_api_calls >= RapidFireThreshold && score.time_window_seconds < 1.0);

    // No user interaction detection
    score.no_user_interaction = !m_has_user_interaction;

    // Calculate individual technique scores (0.0-1.0 each)
    float canvas_score = calculate_canvas_score();
    float webgl_score = calculate_webgl_score();
    float audio_score = calculate_audio_score();
    float navigator_score = calculate_navigator_score();
    float font_score = calculate_font_score();
    float screen_score = calculate_screen_score();

    // Weighted scoring system
    // Base score: Average of active techniques
    float base_score = 0.0f;
    if (score.techniques_used > 0) {
        base_score = (canvas_score + webgl_score + audio_score + navigator_score + font_score + screen_score)
            / static_cast<float>(score.techniques_used);
    }

    // Multipliers for suspicious patterns
    float multiplier = 1.0f;

    // Using 3+ different techniques is highly suspicious (1.5x multiplier)
    if (score.techniques_used >= 3)
        multiplier *= 1.5f;

    // Rapid fire calls suggest automated fingerprinting (1.3x multiplier)
    if (score.rapid_fire_detected)
        multiplier *= 1.3f;

    // No user interaction before fingerprinting (1.2x multiplier)
    if (score.no_user_interaction && score.total_api_calls > 5)
        multiplier *= 1.2f;

    // Calculate final aggressiveness score
    score.aggressiveness_score = AK::min(base_score * multiplier, 1.0f);

    // Confidence based on number of data points
    score.confidence = AK::min(static_cast<float>(score.total_api_calls) / 20.0f, 1.0f);

    // Generate explanation
    Vector<String> reasons;
    if (score.uses_canvas)
        reasons.append(MUST(String::formatted("Canvas fingerprinting ({} calls)", score.canvas_calls)));
    if (score.uses_webgl)
        reasons.append(MUST(String::formatted("WebGL fingerprinting ({} calls)", score.webgl_calls)));
    if (score.uses_audio)
        reasons.append(MUST(String::formatted("Audio fingerprinting ({} calls)", score.audio_calls)));
    if (score.uses_navigator && score.navigator_calls >= NavigatorThreshold)
        reasons.append(MUST(String::formatted("Excessive navigator enumeration ({} calls)", score.navigator_calls)));
    if (score.uses_fonts && score.font_calls >= FontThreshold)
        reasons.append(MUST(String::formatted("Font enumeration ({} fonts)", score.font_calls)));
    if (score.rapid_fire_detected)
        reasons.append(MUST(String::formatted("Rapid-fire API calls ({} in {:.2f}s)", score.total_api_calls, score.time_window_seconds)));
    if (score.no_user_interaction)
        reasons.append("No user interaction before fingerprinting"_string);

    if (reasons.is_empty()) {
        score.explanation = "Minimal fingerprinting activity detected"_string;
    } else {
        StringBuilder builder;
        for (size_t i = 0; i < reasons.size(); i++) {
            builder.append(reasons[i]);
            if (i < reasons.size() - 1)
                builder.append("; "_string);
        }
        score.explanation = MUST(builder.to_string());
    }

    return score;
}

void FingerprintingDetector::reset()
{
    m_api_calls.clear();
    m_first_call_time = {};
    m_last_call_time = {};
    m_has_user_interaction = false;
}

bool FingerprintingDetector::is_aggressive_fingerprinting() const
{
    auto score = calculate_score();
    // Threshold of 0.75 to avoid false positives on single canvas calls
    // Single call = 0.7, so this requires multiple calls or other indicators
    return score.aggressiveness_score > 0.75f;
}

float FingerprintingDetector::calculate_canvas_score() const
{
    if (!has_technique(FingerprintingTechnique::Canvas))
        return 0.0f;

    size_t calls = count_calls_for_technique(FingerprintingTechnique::Canvas);

    // Canvas fingerprinting typically requires:
    // - Rendering text/shapes (implicit in canvas usage)
    // - Reading pixel data (toDataURL or getImageData)
    // Score: 0.7 for any canvas reading, +0.1 for each additional call (max 1.0)
    return AK::min(0.7f + (static_cast<float>(calls - 1) * 0.1f), 1.0f);
}

float FingerprintingDetector::calculate_webgl_score() const
{
    if (!has_technique(FingerprintingTechnique::WebGL))
        return 0.0f;

    size_t calls = count_calls_for_technique(FingerprintingTechnique::WebGL);

    // WebGL fingerprinting typically queries renderer/vendor info
    // Score: 0.6 for any WebGL queries, +0.1 for each additional call (max 1.0)
    return AK::min(0.6f + (static_cast<float>(calls - 1) * 0.1f), 1.0f);
}

float FingerprintingDetector::calculate_audio_score() const
{
    if (!has_technique(FingerprintingTechnique::AudioContext))
        return 0.0f;

    size_t calls = count_calls_for_technique(FingerprintingTechnique::AudioContext);

    // Audio fingerprinting requires AudioContext + oscillator + analyzer
    // Score: 0.8 for audio fingerprinting (very specific technique), +0.05 per call (max 1.0)
    return AK::min(0.8f + (static_cast<float>(calls - 1) * 0.05f), 1.0f);
}

float FingerprintingDetector::calculate_navigator_score() const
{
    if (!has_technique(FingerprintingTechnique::NavigatorEnumeration))
        return 0.0f;

    size_t calls = count_calls_for_technique(FingerprintingTechnique::NavigatorEnumeration);

    // Excessive navigator property access (10+ is suspicious)
    // Score: 0.0-0.5 based on number of calls
    if (calls < NavigatorThreshold)
        return static_cast<float>(calls) / static_cast<float>(NavigatorThreshold) * 0.3f;

    return AK::min(0.5f + (static_cast<float>(calls - NavigatorThreshold) * 0.02f), 1.0f);
}

float FingerprintingDetector::calculate_font_score() const
{
    if (!has_technique(FingerprintingTechnique::FontEnumeration))
        return 0.0f;

    size_t calls = count_calls_for_technique(FingerprintingTechnique::FontEnumeration);

    // Font enumeration (20+ font checks is suspicious)
    // Score: 0.0-0.6 based on number of fonts checked
    if (calls < FontThreshold)
        return static_cast<float>(calls) / static_cast<float>(FontThreshold) * 0.4f;

    return AK::min(0.6f + (static_cast<float>(calls - FontThreshold) * 0.02f), 1.0f);
}

float FingerprintingDetector::calculate_screen_score() const
{
    if (!has_technique(FingerprintingTechnique::ScreenProperties))
        return 0.0f;

    size_t calls = count_calls_for_technique(FingerprintingTechnique::ScreenProperties);

    // Screen property access is generally benign unless combined with other techniques
    // Score: 0.0-0.3 (low weight)
    return AK::min(static_cast<float>(calls) * 0.1f, 0.3f);
}

size_t FingerprintingDetector::count_calls_for_technique(FingerprintingTechnique technique) const
{
    if (!m_api_calls.contains(technique))
        return 0;
    return m_api_calls.get(technique).value().size();
}

bool FingerprintingDetector::has_technique(FingerprintingTechnique technique) const
{
    return m_api_calls.contains(technique) && !m_api_calls.get(technique).value().is_empty();
}

size_t FingerprintingDetector::count_unique_techniques() const
{
    size_t count = 0;
    for (auto const& [technique, calls] : m_api_calls) {
        if (!calls.is_empty())
            count++;
    }
    return count;
}

double FingerprintingDetector::get_time_window_seconds() const
{
    if (!m_first_call_time.has_value() || !m_last_call_time.has_value())
        return 0.0;

    auto duration = m_last_call_time.value().seconds_since_epoch() - m_first_call_time.value().seconds_since_epoch();
    return static_cast<double>(duration);
}

}
