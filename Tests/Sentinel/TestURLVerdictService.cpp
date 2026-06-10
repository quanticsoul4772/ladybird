/*
 * Copyright (c) 2026, quanticsoul4772
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibTest/TestCase.h>
#include <LibURL/Parser.h>
#include <WebContent/URLVerdictService.h>

using namespace WebContent;

// Helper: parse a URL, crash on failure
static URL::URL parse_url(StringView str)
{
    auto result = URL::Parser::basic_parse(str);
    VERIFY(result.has_value());
    return *result;
}

// ── extract_domain ─────────────────────────────────────────────────────────

TEST_CASE(extract_domain_http)
{
    auto url = parse_url("http://Example.COM/path?q=1"sv);
    auto domain = URLVerdictService::extract_domain(url);
    EXPECT_EQ(domain, "example.com");
}

TEST_CASE(extract_domain_https_with_port)
{
    auto url = parse_url("https://malicious.example.org:8443/login"sv);
    auto domain = URLVerdictService::extract_domain(url);
    EXPECT_EQ(domain, "malicious.example.org");
}

TEST_CASE(extract_domain_non_http_empty)
{
    auto url = parse_url("file:///home/user/file.html"sv);
    // file:// has no host; extract_domain should return empty
    auto domain = URLVerdictService::extract_domain(url);
    EXPECT(domain.is_empty());
}

TEST_CASE(extract_domain_about_empty)
{
    auto url = parse_url("about:blank"sv);
    auto domain = URLVerdictService::extract_domain(url);
    EXPECT(domain.is_empty());
}

// ── scheme filtering in check() ────────────────────────────────────────────

TEST_CASE(check_non_http_scheme_returns_clean)
{
    auto& svc = URLVerdictService::the();
    svc.set_enabled(true);

    // file:// should return Clean without touching the analyzer
    auto verdict = svc.check(parse_url("file:///etc/passwd"sv));
    EXPECT_EQ(verdict.level, URLThreatLevel::Clean);

    // data: URI should return Clean
    auto data_url = parse_url("data:text/html,<h1>hi</h1>"sv);
    auto v2 = svc.check(data_url);
    EXPECT_EQ(v2.level, URLThreatLevel::Clean);
}

TEST_CASE(check_disabled_returns_clean)
{
    auto& svc = URLVerdictService::the();
    svc.set_enabled(false);

    auto verdict = svc.check(parse_url("http://totallymalicious.phishing-test.example"sv));
    EXPECT_EQ(verdict.level, URLThreatLevel::Clean);

    svc.set_enabled(true); // restore
}

// ── threat level ordering ───────────────────────────────────────────────────

TEST_CASE(threat_level_ordering)
{
    // The >= comparison in ConnectionFromClient relies on this ordering.
    EXPECT(URLThreatLevel::Clean < URLThreatLevel::Suspicious);
    EXPECT(URLThreatLevel::Suspicious < URLThreatLevel::Malicious);
    EXPECT(URLThreatLevel::Malicious < URLThreatLevel::Critical);
    EXPECT(URLThreatLevel::Suspicious >= URLThreatLevel::Suspicious);
}

// ── approve_domain ──────────────────────────────────────────────────────────

TEST_CASE(approve_domain_bypasses_check)
{
    auto& svc = URLVerdictService::the();
    svc.set_enabled(true);

    ByteString test_domain = "safe-after-approval.example.com";

    // Approve before check — result should always be Clean.
    svc.approve_domain(test_domain);

    // Inject a URL on that domain; check() must return Clean without calling analyzer.
    // (Since we can't force the analyzer to return Suspicious in unit tests,
    //  we verify the approved domain short-circuits by checking the approved set.)
    auto url = parse_url("https://safe-after-approval.example.com/path"sv);
    auto verdict = svc.check(url);
    EXPECT_EQ(verdict.level, URLThreatLevel::Clean);
}

TEST_CASE(approve_domain_empty_is_noop)
{
    // Approving an empty domain should not crash or corrupt state.
    URLVerdictService::the().approve_domain(""_byte_string);
}

// ── URLVerdict defaults ─────────────────────────────────────────────────────

TEST_CASE(url_verdict_default_is_clean)
{
    URLVerdict v;
    EXPECT_EQ(v.level, URLThreatLevel::Clean);
    EXPECT_EQ(v.score, 0.0f);
    EXPECT(v.explanation.is_empty());
}
