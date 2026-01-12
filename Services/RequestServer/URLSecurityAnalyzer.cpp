/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "URLSecurityAnalyzer.h"
#include <AK/JsonObject.h>
#include <AK/StringBuilder.h>

namespace RequestServer {

ErrorOr<NonnullOwnPtr<URLSecurityAnalyzer>> URLSecurityAnalyzer::create()
{
    auto phishing_analyzer = TRY(Sentinel::PhishingURLAnalyzer::create());
    return adopt_own(*new URLSecurityAnalyzer(move(phishing_analyzer)));
}

URLSecurityAnalyzer::URLSecurityAnalyzer(NonnullOwnPtr<Sentinel::PhishingURLAnalyzer> analyzer)
    : m_phishing_analyzer(move(analyzer))
{
}

ErrorOr<URLSecurityAnalyzer::URLThreatAnalysis> URLSecurityAnalyzer::analyze_url(URL::URL const& url)
{
    // Convert URL to string for analysis
    auto url_string = url.to_string();
    auto url_sv = url_string.bytes_as_string_view();

    // Run phishing analysis
    auto phishing_result = TRY(m_phishing_analyzer->analyze_url(url_sv));

    // Convert to URLThreatAnalysis
    URLThreatAnalysis analysis;
    analysis.phishing_score = phishing_result.phishing_score;
    analysis.confidence = phishing_result.confidence;
    analysis.is_homograph_attack = phishing_result.is_homograph_attack;
    analysis.is_typosquatting = phishing_result.is_typosquatting;
    analysis.has_suspicious_tld = phishing_result.has_suspicious_tld;
    analysis.domain_entropy = phishing_result.domain_entropy;
    analysis.explanation = move(phishing_result.explanation);
    analysis.closest_legitimate_domain = move(phishing_result.closest_legitimate_domain);

    // Mark as suspicious if score exceeds threshold
    analysis.is_suspicious = (analysis.phishing_score >= 0.3f);

    return analysis;
}

ErrorOr<bool> URLSecurityAnalyzer::is_dangerous_url(URL::URL const& url)
{
    auto analysis = TRY(analyze_url(url));
    return analysis.threat_level() == URLThreatAnalysis::ThreatLevel::Dangerous;
}

ErrorOr<ByteString> URLSecurityAnalyzer::generate_security_alert_json(URLThreatAnalysis const& analysis, URL::URL const& url)
{
    JsonObject alert;
    alert.set("type"sv, JsonValue("phishing_url_detected"sv));
    alert.set("url"sv, JsonValue(url.to_byte_string()));
    alert.set("phishing_score"sv, JsonValue(analysis.phishing_score));
    alert.set("confidence"sv, JsonValue(analysis.confidence));
    alert.set("is_homograph_attack"sv, JsonValue(analysis.is_homograph_attack));
    alert.set("is_typosquatting"sv, JsonValue(analysis.is_typosquatting));
    alert.set("has_suspicious_tld"sv, JsonValue(analysis.has_suspicious_tld));
    alert.set("domain_entropy"sv, JsonValue(analysis.domain_entropy));
    alert.set("explanation"sv, JsonValue(analysis.explanation.to_byte_string()));

    if (!analysis.closest_legitimate_domain.is_empty())
        alert.set("closest_legitimate_domain"sv, JsonValue(analysis.closest_legitimate_domain.to_byte_string()));

    // Add threat level classification
    StringView threat_level_str;
    switch (analysis.threat_level()) {
    case URLThreatAnalysis::ThreatLevel::Safe:
        threat_level_str = "safe"sv;
        break;
    case URLThreatAnalysis::ThreatLevel::Suspicious:
        threat_level_str = "suspicious"sv;
        break;
    case URLThreatAnalysis::ThreatLevel::Dangerous:
        threat_level_str = "dangerous"sv;
        break;
    }
    alert.set("threat_level"sv, JsonValue(threat_level_str));

    // Convert JsonObject to JsonValue and serialize
    return JsonValue(alert).serialized().to_byte_string();
}

}
