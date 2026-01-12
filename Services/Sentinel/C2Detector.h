/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>
#include <AK/NonnullOwnPtr.h>
#include <AK/String.h>
#include <AK/Vector.h>

namespace Sentinel {

// Command & Control (C2) communication and data exfiltration detection
// Milestone 0.4 Phase 6: Network Behavioral Analysis
class C2Detector {
public:
    // Beaconing detection result
    struct BeaconingAnalysis {
        float interval_regularity { 0.0f };  // Coefficient of Variation (CV): σ/μ
        u32 request_count { 0 };             // Number of requests analyzed
        Vector<float> intervals;             // Inter-request intervals (seconds)
        bool is_beaconing { false };         // True if beaconing detected
        float confidence { 0.0f };           // Detection confidence (0.0-1.0)
        String explanation;                  // Human-readable explanation
    };

    // Data exfiltration detection result
    struct ExfiltrationAnalysis {
        u64 bytes_sent { 0 };               // Total bytes uploaded
        u64 bytes_received { 0 };           // Total bytes downloaded
        float upload_ratio { 0.0f };        // bytes_sent / (bytes_sent + bytes_received)
        bool is_exfiltration { false };     // True if exfiltration detected
        float confidence { 0.0f };          // Detection confidence (0.0-1.0)
        String explanation;                 // Human-readable explanation
    };

    static ErrorOr<NonnullOwnPtr<C2Detector>> create();
    ~C2Detector() = default;

    // Analyze request intervals for beaconing patterns
    // Requires at least 5 requests for statistical significance
    ErrorOr<BeaconingAnalysis> analyze_beaconing(Vector<float> const& request_intervals);

    // Analyze upload/download ratio for data exfiltration
    // Known upload services (e.g., Google Drive, S3) are whitelisted
    ErrorOr<ExfiltrationAnalysis> analyze_exfiltration(u64 bytes_sent, u64 bytes_received, StringView domain);

private:
    C2Detector() = default;

    // Statistical analysis helpers
    static float calculate_coefficient_of_variation(Vector<float> const& intervals);
    static float calculate_mean(Vector<float> const& values);
    static float calculate_std_dev(Vector<float> const& values, float mean);

    // Known upload service whitelist (reduces false positives)
    static bool is_known_upload_service(StringView domain);

    // Detection thresholds
    static constexpr size_t MinBeaconingRequests = 5;      // Minimum for statistical significance
    static constexpr float HighlyCertainCV = 0.2f;         // CV < 0.2 = highly regular (beaconing)
    static constexpr float RegularCV = 0.4f;               // CV < 0.4 = somewhat regular
    static constexpr float SuspiciousUploadRatio = 0.7f;   // 70% uploads
    static constexpr float HighlySuspiciousUploadRatio = 0.9f; // 90% uploads
    static constexpr u64 MinExfiltrationBytes = 10 * 1024 * 1024; // 10 MB
};

}
