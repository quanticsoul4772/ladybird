/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "C2Detector.h"
#include <AK/Math.h>
#include <AK/StringBuilder.h>

namespace Sentinel {

ErrorOr<NonnullOwnPtr<C2Detector>> C2Detector::create()
{
    return adopt_own(*new C2Detector());
}

ErrorOr<C2Detector::BeaconingAnalysis> C2Detector::analyze_beaconing(Vector<float> const& request_intervals)
{
    BeaconingAnalysis analysis;

    // Validate input: need at least 5 requests for statistical significance
    if (request_intervals.size() < MinBeaconingRequests) {
        return Error::from_string_literal("Need at least 5 requests for beaconing analysis");
    }

    // Store intervals and count
    analysis.intervals = request_intervals;
    analysis.request_count = static_cast<u32>(request_intervals.size());

    // Calculate Coefficient of Variation (CV) = σ / μ
    // where σ = standard deviation, μ = mean
    // Low CV indicates regular intervals (potential beaconing)
    analysis.interval_regularity = calculate_coefficient_of_variation(request_intervals);

    // Determine if beaconing based on CV thresholds
    if (analysis.interval_regularity < HighlyCertainCV) {
        // CV < 0.2: Highly regular intervals (strong beaconing signal)
        analysis.is_beaconing = true;
        analysis.confidence = 0.95f;
    } else if (analysis.interval_regularity < RegularCV) {
        // CV < 0.4: Somewhat regular intervals (possible beaconing)
        analysis.is_beaconing = true;
        analysis.confidence = 0.75f;
    } else {
        // CV >= 0.4: Irregular intervals (normal browsing)
        analysis.is_beaconing = false;
        analysis.confidence = 0.85f;
    }

    // Calculate mean interval for explanation
    float mean_interval = calculate_mean(request_intervals);

    // Generate human-readable explanation
    StringBuilder builder;
    if (analysis.is_beaconing) {
        if (analysis.interval_regularity < HighlyCertainCV) {
            builder.append("Highly regular intervals (CV="_string);
        } else {
            builder.append("Regular intervals (CV="_string);
        }
        builder.appendff("{:.4f}), {} requests with {:.1f}s period",
            analysis.interval_regularity,
            analysis.request_count,
            mean_interval);
    } else {
        builder.appendff("Irregular intervals (CV={:.4f}), normal browsing pattern",
            analysis.interval_regularity);
    }
    analysis.explanation = TRY(builder.to_string());

    return analysis;
}

ErrorOr<C2Detector::ExfiltrationAnalysis> C2Detector::analyze_exfiltration(
    u64 bytes_sent, u64 bytes_received, StringView domain)
{
    ExfiltrationAnalysis analysis;

    // Store raw values
    analysis.bytes_sent = bytes_sent;
    analysis.bytes_received = bytes_received;

    // Handle edge case: no data transferred
    if (bytes_sent == 0 && bytes_received == 0) {
        return Error::from_string_literal("No data transferred");
    }

    // Calculate upload ratio: bytes_sent / (bytes_sent + bytes_received)
    u64 total_bytes = bytes_sent + bytes_received;
    analysis.upload_ratio = static_cast<float>(bytes_sent) / static_cast<float>(total_bytes);

    // Whitelist known upload services (reduces false positives)
    bool is_upload_service = is_known_upload_service(domain);

    // Determine if exfiltration based on upload ratio and volume
    if (is_upload_service) {
        // Known upload service: require very high ratio to flag
        // (Users legitimately upload files to these services)
        analysis.is_exfiltration = false;
        analysis.confidence = 0.9f;
    } else if (analysis.upload_ratio > HighlySuspiciousUploadRatio && bytes_sent > MinExfiltrationBytes) {
        // Upload ratio > 90% AND > 10 MB: highly suspicious
        analysis.is_exfiltration = true;
        analysis.confidence = 0.90f;
    } else if (analysis.upload_ratio > SuspiciousUploadRatio && bytes_sent > MinExfiltrationBytes) {
        // Upload ratio > 70% AND > 10 MB: suspicious
        analysis.is_exfiltration = true;
        analysis.confidence = 0.75f;
    } else {
        // Normal upload/download ratio
        analysis.is_exfiltration = false;
        analysis.confidence = 0.85f;
    }

    // Generate human-readable explanation
    StringBuilder builder;
    if (analysis.is_exfiltration) {
        float ratio_percent = analysis.upload_ratio * 100.0f;
        u64 mb_sent = bytes_sent / (1024 * 1024);
        u64 mb_received = bytes_received / (1024 * 1024);

        builder.append("High upload ratio ("_string);
        builder.appendff("{:.0f}%), ", ratio_percent);
        builder.appendff("{} MB sent vs {} MB received", mb_sent, mb_received);
    } else if (is_upload_service) {
        builder.append("Known upload service ("_string);
        builder.append(domain);
        builder.append("), benign"_string);
    } else {
        float ratio_percent = analysis.upload_ratio * 100.0f;
        builder.appendff("Normal upload ratio ({:.0f}%)", ratio_percent);
    }
    analysis.explanation = TRY(builder.to_string());

    return analysis;
}

// Calculate mean (average) of a vector of floats
float C2Detector::calculate_mean(Vector<float> const& values)
{
    if (values.is_empty())
        return 0.0f;

    float sum = 0.0f;
    for (float value : values)
        sum += value;

    return sum / static_cast<float>(values.size());
}

// Calculate standard deviation
// Formula: σ = sqrt(Σ(x - μ)² / N)
// where μ = mean, N = count
float C2Detector::calculate_std_dev(Vector<float> const& values, float mean)
{
    if (values.size() <= 1)
        return 0.0f;

    float sum_squared_diffs = 0.0f;
    for (float value : values) {
        float diff = value - mean;
        sum_squared_diffs += diff * diff;
    }

    float variance = sum_squared_diffs / static_cast<float>(values.size());
    return AK::sqrt(variance);
}

// Calculate Coefficient of Variation (CV)
// Formula: CV = σ / μ
// where σ = standard deviation, μ = mean
// Lower CV = more regular intervals (potential beaconing)
float C2Detector::calculate_coefficient_of_variation(Vector<float> const& intervals)
{
    if (intervals.is_empty())
        return 0.0f;

    float mean = calculate_mean(intervals);

    // Handle edge case: zero mean (shouldn't happen with time intervals)
    if (mean == 0.0f)
        return 0.0f;

    float std_dev = calculate_std_dev(intervals, mean);

    return std_dev / mean;
}

// Check if domain is a known upload service (whitelist)
// This reduces false positives for legitimate file upload scenarios
bool C2Detector::is_known_upload_service(StringView domain)
{
    // Cloud storage providers
    if (domain.contains("s3.amazonaws.com"sv))
        return true;
    if (domain.contains("storage.googleapis.com"sv))
        return true;
    if (domain.contains("blob.core.windows.net"sv))
        return true;
    if (domain.contains("digitaloceanspaces.com"sv))
        return true;

    // File sharing services
    if (domain.contains("drive.google.com"sv))
        return true;
    if (domain.contains("dropbox.com"sv))
        return true;
    if (domain.contains("box.com"sv))
        return true;
    if (domain.contains("onedrive.live.com"sv))
        return true;
    if (domain.contains("icloud.com"sv))
        return true;
    if (domain.contains("mega.nz"sv))
        return true;

    // Development platforms
    if (domain.contains("github.com"sv))
        return true;
    if (domain.contains("gitlab.com"sv))
        return true;
    if (domain.contains("bitbucket.org"sv))
        return true;

    // Media/content platforms
    if (domain.contains("youtube.com"sv))
        return true;
    if (domain.contains("vimeo.com"sv))
        return true;
    if (domain.contains("imgur.com"sv))
        return true;
    if (domain.contains("flickr.com"sv))
        return true;

    // Backup services
    if (domain.contains("backblaze.com"sv))
        return true;
    if (domain.contains("crashplan.com"sv))
        return true;

    // Cloud platforms
    if (domain.contains("cloudflare.com"sv))
        return true;
    if (domain.contains("firebase.google.com"sv))
        return true;
    if (domain.contains("netlify.com"sv))
        return true;
    if (domain.contains("vercel.app"sv))
        return true;

    // Social media (users upload photos/videos)
    if (domain.contains("facebook.com"sv))
        return true;
    if (domain.contains("instagram.com"sv))
        return true;
    if (domain.contains("twitter.com"sv))
        return true;
    if (domain.contains("linkedin.com"sv))
        return true;

    return false;
}

}
