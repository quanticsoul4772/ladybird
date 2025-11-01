/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>
#include <AK/HashMap.h>
#include <AK/NonnullOwnPtr.h>
#include <AK/String.h>
#include <AK/Time.h>

namespace Sentinel {

// Browser fingerprinting detection and scoring
// Milestone 0.4 Phase 4: Fingerprinting Detection
class FingerprintingDetector {
public:
    enum class FingerprintingTechnique {
        Canvas,           // Canvas API (toDataURL, getImageData)
        WebGL,            // WebGL API (getParameter, renderer info)
        AudioContext,     // Web Audio API (oscillator, analyser)
        NavigatorEnumeration, // Excessive navigator property access
        FontEnumeration,  // Font list probing
        ScreenProperties  // Screen resolution, color depth
    };

    struct APICallEvent {
        FingerprintingTechnique technique;
        String api_name;
        UnixDateTime timestamp;
        bool had_user_interaction { false };
        size_t call_count { 1 };
    };

    struct FingerprintingScore {
        float aggressiveness_score { 0.0f }; // 0.0-1.0 (0 = benign, 1 = aggressive fingerprinting)
        float confidence { 0.0f };            // Detection confidence (0.0-1.0)

        // Individual technique flags
        bool uses_canvas { false };
        bool uses_webgl { false };
        bool uses_audio { false };
        bool uses_navigator { false };
        bool uses_fonts { false };
        bool uses_screen { false };

        // Timing analysis
        size_t total_api_calls { 0 };
        size_t techniques_used { 0 };
        double time_window_seconds { 0.0 };
        bool rapid_fire_detected { false };  // Multiple techniques in <1 second
        bool no_user_interaction { false };  // Fingerprinting before any user input

        // Detailed breakdown
        size_t canvas_calls { 0 };
        size_t webgl_calls { 0 };
        size_t audio_calls { 0 };
        size_t navigator_calls { 0 };
        size_t font_calls { 0 };
        size_t screen_calls { 0 };

        String explanation; // Human-readable explanation
    };

    static ErrorOr<NonnullOwnPtr<FingerprintingDetector>> create();
    ~FingerprintingDetector() = default;

    // Record an API call that may be part of fingerprinting
    void record_api_call(FingerprintingTechnique technique, StringView api_name, bool had_user_interaction = false);

    // Calculate fingerprinting score based on recorded calls
    FingerprintingScore calculate_score() const;

    // Reset tracking (e.g., on navigation)
    void reset();

    // Quick check - returns true if aggressiveness score > 0.6
    bool is_aggressive_fingerprinting() const;

private:
    FingerprintingDetector() = default;

    // Track API calls per technique
    HashMap<FingerprintingTechnique, Vector<APICallEvent>> m_api_calls;

    // Timestamps for timing analysis
    Optional<UnixDateTime> m_first_call_time;
    Optional<UnixDateTime> m_last_call_time;

    // User interaction tracking
    bool m_has_user_interaction { false };

    // Scoring thresholds
    static constexpr size_t RapidFireThreshold = 5;      // 5+ API calls in 1 second
    static constexpr size_t NavigatorThreshold = 10;     // 10+ navigator accesses
    static constexpr size_t FontThreshold = 20;          // 20+ font checks
    static constexpr double TimeWindowSeconds = 5.0;     // Consider calls within 5 seconds

    // Calculate individual technique scores
    float calculate_canvas_score() const;
    float calculate_webgl_score() const;
    float calculate_audio_score() const;
    float calculate_navigator_score() const;
    float calculate_font_score() const;
    float calculate_screen_score() const;

    // Helper methods
    size_t count_calls_for_technique(FingerprintingTechnique technique) const;
    bool has_technique(FingerprintingTechnique technique) const;
    size_t count_unique_techniques() const;
    double get_time_window_seconds() const;
};

}
