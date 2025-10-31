/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Types.h>
#include <cstring>

namespace RequestServer {

// Configuration for size-based scanning strategy
// Implements three-tier approach to balance security and performance
struct ScanSizeConfig {
    // Small files (< small_file_threshold): Load into memory, scan normally (fast path)
    // Fast, full scanning with minimal memory overhead
    size_t small_file_threshold { 10 * 1024 * 1024 }; // 10MB

    // Medium files (small_file_threshold to medium_file_threshold): Stream in chunks
    // Incremental scanning with controlled memory usage
    size_t medium_file_threshold { 100 * 1024 * 1024 }; // 100MB

    // Large files (medium_file_threshold to max_scan_size): Partial scanning or skip
    // Scan first and last portions only, or skip entirely based on configuration
    size_t max_scan_size { 200 * 1024 * 1024 }; // 200MB

    // Chunk size for streaming scan (medium files)
    // Balance between performance and memory usage
    size_t chunk_size { 1 * 1024 * 1024 }; // 1MB

    // Whether to scan large files partially (first + last chunks) or skip entirely
    // Partial scanning: scan first/last 10MB to catch common malware locations
    // Rationale: Most malware is at beginning (headers, payloads) or end (appended data)
    bool scan_large_files_partially { true };

    // Size of partial scan for large files (first + last)
    // Total scanned = 2 * large_file_scan_bytes
    size_t large_file_scan_bytes { 10 * 1024 * 1024 }; // 10MB

    // Maximum concurrent memory usage per scan
    // For streaming: 2 * chunk_size (current + previous for context)
    // For partial: 2 * large_file_scan_bytes
    size_t max_memory_per_scan { 3 * 1024 * 1024 }; // 3MB (chunk + overhead)

    // Overlap size for chunk boundaries to catch patterns spanning chunks
    // Must be smaller than chunk_size
    size_t chunk_overlap_size { 4096 }; // 4KB

    // Whether to enable telemetry tracking
    bool enable_telemetry { true };

    // Default configuration
    static ScanSizeConfig create_default()
    {
        return ScanSizeConfig {
            .small_file_threshold = 10 * 1024 * 1024,      // 10MB
            .medium_file_threshold = 100 * 1024 * 1024,    // 100MB
            .max_scan_size = 200 * 1024 * 1024,            // 200MB
            .chunk_size = 1 * 1024 * 1024,                 // 1MB
            .scan_large_files_partially = true,
            .large_file_scan_bytes = 10 * 1024 * 1024,     // 10MB
            .max_memory_per_scan = 3 * 1024 * 1024,        // 3MB
            .chunk_overlap_size = 4096,                    // 4KB
            .enable_telemetry = true
        };
    }

    // Validate configuration
    bool is_valid() const
    {
        // Thresholds must be ordered
        if (small_file_threshold >= medium_file_threshold)
            return false;
        if (medium_file_threshold >= max_scan_size)
            return false;

        // Chunk overlap must be smaller than chunk size
        if (chunk_overlap_size >= chunk_size)
            return false;

        // Large file scan bytes must be reasonable
        if (large_file_scan_bytes > medium_file_threshold)
            return false;

        // Memory limit must be at least 2x chunk size
        if (max_memory_per_scan < 2 * chunk_size)
            return false;

        return true;
    }

    // Get size tier name for file size
    char const* get_tier_name(size_t file_size) const
    {
        if (file_size <= small_file_threshold)
            return "small";
        if (file_size <= medium_file_threshold)
            return "medium";
        if (file_size <= max_scan_size)
            return "large";
        return "oversized";
    }
};

// Telemetry data for scan operations
struct ScanTelemetry {
    size_t scans_small { 0 };
    size_t scans_medium { 0 };
    size_t scans_large_partial { 0 };
    size_t scans_oversized_skipped { 0 };

    size_t total_bytes_scanned { 0 };
    size_t total_files_scanned { 0 };

    size_t peak_memory_usage { 0 };
    size_t total_scan_time_ms { 0 };

    // Get scan count by tier
    size_t get_tier_count(char const* tier) const
    {
        if (strcmp(tier, "small") == 0)
            return scans_small;
        if (strcmp(tier, "medium") == 0)
            return scans_medium;
        if (strcmp(tier, "large") == 0)
            return scans_large_partial;
        if (strcmp(tier, "oversized") == 0)
            return scans_oversized_skipped;
        return 0;
    }

    // Reset all counters
    void reset()
    {
        scans_small = 0;
        scans_medium = 0;
        scans_large_partial = 0;
        scans_oversized_skipped = 0;
        total_bytes_scanned = 0;
        total_files_scanned = 0;
        peak_memory_usage = 0;
        total_scan_time_ms = 0;
    }
};

}
