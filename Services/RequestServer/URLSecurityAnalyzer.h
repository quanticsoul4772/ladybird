/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>
#include <AK/NonnullOwnPtr.h>
#include <AK/String.h>
#include <LibURL/URL.h>
#include <Sentinel/PhishingURLAnalyzer.h>

namespace RequestServer {

// URL security analyzer - integrates Sentinel's PhishingURLAnalyzer
// into RequestServer's request flow for real-time phishing detection
class URLSecurityAnalyzer {
public:
    static ErrorOr<NonnullOwnPtr<URLSecurityAnalyzer>> create();
    ~URLSecurityAnalyzer() = default;

    struct URLThreatAnalysis {
        bool is_suspicious { false };      // true if any threat indicators detected
        float phishing_score { 0.0f };     // 0.0-1.0 (0 = benign, 1 = phishing)
        float confidence { 0.0f };         // Detection confidence (0.0-1.0)
        bool is_homograph_attack { false };
        bool is_typosquatting { false };
        bool has_suspicious_tld { false };
        float domain_entropy { 0.0f };
        String explanation;
        String closest_legitimate_domain;

        // Threat level classification
        enum class ThreatLevel {
            Safe,       // score < 0.3
            Suspicious, // score 0.3-0.6
            Dangerous   // score > 0.6
        };

        ThreatLevel threat_level() const {
            if (phishing_score < 0.3f)
                return ThreatLevel::Safe;
            if (phishing_score < 0.6f)
                return ThreatLevel::Suspicious;
            return ThreatLevel::Dangerous;
        }
    };

    // Analyze a URL for phishing indicators
    ErrorOr<URLThreatAnalysis> analyze_url(URL::URL const& url);

    // Quick check - returns true if URL appears dangerous (score > 0.6)
    ErrorOr<bool> is_dangerous_url(URL::URL const& url);

    // Generate JSON alert for IPC security_alert message
    ErrorOr<ByteString> generate_security_alert_json(URLThreatAnalysis const& analysis, URL::URL const& url);

private:
    URLSecurityAnalyzer(NonnullOwnPtr<Sentinel::PhishingURLAnalyzer> analyzer);

    NonnullOwnPtr<Sentinel::PhishingURLAnalyzer> m_phishing_analyzer;
};

}
