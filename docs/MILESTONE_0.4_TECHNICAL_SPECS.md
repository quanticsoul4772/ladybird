# Milestone 0.4 Technical Specifications

**Version**: 1.0
**Date**: 2025-10-31
**Status**: Research Complete - Ready for Implementation

Based on comprehensive research of current 2025 technologies and best practices.

---

## 1. ML-Based Malware Detection

### Framework Comparison & Recommendation

#### Research Findings (2025)

**ONNX Runtime:**
- ✅ Broader platform compatibility
- ✅ Hardware acceleration support
- ❌ Accuracy degradation after conversion (FNR increase from 15.62% to 38.78% in neural networks)
- ❌ Performance inconsistency (some models slower post-conversion)
- ❌ Higher false positive rates

**TensorFlow Lite:**
- ✅ Better accuracy preservation
- ✅ Optimized for edge devices
- ✅ TinyML-compatible
- ✅ Lower false positive rates
- ⚠️ Less flexible platform support
- ⚠️ Larger binary size

**LightBench Study (VLSI 2025):**
Recent research on hardware-aware malware detection found that:
- ONNX provides broader compatibility but introduces accuracy/performance challenges
- TinyML design paradigm (TensorFlow Lite Micro) better for embedded scenarios
- Hardware-assisted malware detection requires careful framework selection

### Recommendation: TensorFlow Lite

**Rationale:**
1. Accuracy is critical for malware detection (false negatives are dangerous)
2. Ladybird targets desktop/laptop platforms (not ultra-constrained embedded)
3. Lower FPR means better user experience
4. TinyML compatibility allows future mobile ports

### Implementation Architecture

```cpp
// Services/Sentinel/MalwareML.h
namespace Sentinel {

class MalwareMLDetector {
public:
    struct Features {
        u64 file_size;
        float entropy;          // Shannon entropy (0.0-8.0)
        u32 pe_header_anomalies; // PE structure analysis
        u32 suspicious_strings; // High-entropy strings, URLs, IPs
        float code_section_ratio; // Code vs. data ratio
        u32 import_table_size;
    };

    struct Prediction {
        float malware_probability; // 0.0-1.0
        float confidence;          // Model confidence
        String explanation;        // Feature importance
    };

    static ErrorOr<NonnullOwnPtr<MalwareMLDetector>> create(
        ByteString const& model_path);

    ErrorOr<Features> extract_features(ByteBuffer const& file_data);
    ErrorOr<Prediction> predict(Features const& features);

private:
    // TensorFlow Lite interpreter
    OwnPtr<void> m_interpreter; // tflite::Interpreter*
    Vector<float> m_input_tensor;
    Vector<float> m_output_tensor;
};

}
```

### Feature Extraction Pipeline

```cpp
ErrorOr<MalwareMLDetector::Features> MalwareMLDetector::extract_features(
    ByteBuffer const& file_data)
{
    Features features;

    // 1. File size (normalized)
    features.file_size = file_data.size();

    // 2. Shannon entropy
    features.entropy = calculate_shannon_entropy(file_data);

    // 3. PE header analysis (if Windows executable)
    if (is_pe_file(file_data)) {
        features.pe_header_anomalies = analyze_pe_structure(file_data);
        features.code_section_ratio = calculate_code_ratio(file_data);
        features.import_table_size = extract_import_table_size(file_data);
    }

    // 4. String analysis
    features.suspicious_strings = count_suspicious_strings(file_data);

    return features;
}
```

### Integration with SecurityTap

```cpp
// Services/RequestServer/SecurityTap.cpp
auto ml_detector = TRY(MalwareMLDetector::create("/usr/share/ladybird/models/malware.tflite"));
auto features = TRY(ml_detector->extract_features(file_data));
auto prediction = TRY(ml_detector->predict(features));

if (prediction.malware_probability > 0.8f) {
    // High confidence malware
    return InspectionResult {
        .verdict = InspectionVerdict::Block,
        .rule_name = "ML_Malware_Detection"_string,
        .confidence = prediction.confidence
    };
}
```

### Performance Requirements

- Model size: <10MB (quantized int8)
- Inference time: <100ms per file
- Memory footprint: <50MB
- CPU usage: <5% sustained

### Training Pipeline

```python
# Tools/ml-training/train_malware_detector.py
import tensorflow as tf

# Dataset: VirusTotal benign + malware samples
# Features: 6-dimensional feature vector
# Architecture: 3-layer DNN [6, 32, 16, 2]
# Output: Binary classification (benign/malware)

model = tf.keras.Sequential([
    tf.keras.layers.Dense(32, activation='relu', input_shape=(6,)),
    tf.keras.layers.Dropout(0.3),
    tf.keras.layers.Dense(16, activation='relu'),
    tf.keras.layers.Dense(2, activation='softmax')
])

# Quantize for edge deployment
converter = tf.lite.TFLiteConverter.from_keras_model(model)
converter.optimizations = [tf.lite.Optimize.DEFAULT]
tflite_model = converter.convert()
```

---

## 2. Federated Threat Intelligence

### Architecture: Hybrid Approach

Based on 2025 research, we'll use a combination of:
1. **Bloom Filters** for set membership (lightweight, privacy-preserving)
2. **IPFS/libp2p** for decentralized threat feeds
3. **Differential Privacy** for federated learning

### Bloom Filter Threat Cache

```cpp
// Services/Sentinel/ThreatFeed.h
class ThreatFeed {
public:
    // Bloom filter for 100M hashes, 0.1% false positive rate
    static constexpr size_t FILTER_SIZE = 120_000_000; // ~120MB

    ErrorOr<void> add_threat_hash(String const& sha256_hash);
    bool probably_malicious(String const& sha256_hash) const;

    // IPFS integration
    ErrorOr<void> sync_from_peers();
    ErrorOr<void> publish_local_threats();

private:
    BloomFilter m_filter;
    IPFS::Node m_ipfs_node;
};
```

### Differential Privacy for Federated Learning

```cpp
// Services/Sentinel/FederatedLearning.h
class FederatedLearning {
public:
    struct LocalGradient {
        Vector<float> weights;
        float epsilon;  // Privacy budget (smaller = more private)
        u64 sample_count;
    };

    // Add Laplace noise for differential privacy
    static LocalGradient privatize_gradient(
        Vector<float> const& raw_gradient,
        float epsilon = 0.1);  // Default: strong privacy

    // Aggregate gradients from peers
    ErrorOr<Vector<float>> aggregate_gradients(
        Vector<LocalGradient> const& peer_gradients);

private:
    static float laplace_noise(float scale);
};
```

### Privacy Guarantees

- **No PII**: Only file hashes and feature vectors shared
- **Differential Privacy**: ε = 0.1 (strong privacy guarantee)
- **K-anonymity**: Aggregation requires ≥100 participants
- **Opt-in**: All federated features require explicit user consent

---

## 3. Browser Fingerprinting Detection

### Research Findings (2025)

**Current State:**
- Canvas fingerprinting alone identifies 60% of users
- Combined techniques (Canvas + WebGL + Audio) identify 99%+ users
- Brave and Firefox have built-in protections

**Brave Approach:**
- Removes canvas/WebGL APIs from third parties by default
- Fingerprint randomization (different value each visit)
- Users can still enjoy legitimate uses

**Firefox Approach:**
- `privacy.resistFingerprinting` mode
- All users look identical (but this makes them stand out)
- Requires permission for canvas access

### Ladybird's Hybrid Approach

We'll combine the best of both: **Detection + Selective Randomization**

### Fingerprinting Taxonomy

```cpp
// Services/WebContent/FingerprintDetector.h
enum class FingerprintTechnique {
    Canvas,
    WebGL,
    AudioContext,
    FontEnumeration,
    NavigatorProperties,
    ScreenResolution,
    Timezone,
    BatteryAPI,
    WebRTC
};

struct FingerprintScore {
    float score;  // 0.0-1.0 (aggressiveness)
    Vector<FingerprintTechnique> techniques_detected;
    u32 api_call_count;
    Duration tracking_duration;
};
```

### Detection Algorithm

```cpp
class FingerprintDetector {
public:
    void on_canvas_operation(String const& operation);
    void on_webgl_query(String const& parameter);
    void on_audio_context_creation();
    void on_navigator_access(String const& property);

    FingerprintScore calculate_score() const;

private:
    // Scoring weights
    static constexpr float CANVAS_WEIGHT = 0.3f;
    static constexpr float WEBGL_WEIGHT = 0.25f;
    static constexpr float AUDIO_WEIGHT = 0.2f;
    static constexpr float NAVIGATOR_WEIGHT = 0.15f;
    static constexpr float FONT_WEIGHT = 0.1f;

    // Thresholds
    static constexpr u32 CANVAS_SUSPICIOUS_THRESHOLD = 5;
    static constexpr u32 WEBGL_SUSPICIOUS_THRESHOLD = 10;
    static constexpr u32 NAVIGATOR_SUSPICIOUS_THRESHOLD = 20;

    HashMap<FingerprintTechnique, u32> m_api_calls;
    UnixDateTime m_tracking_start;
};
```

### Integration with LibWeb

```cpp
// Libraries/LibWeb/HTML/HTMLCanvasElement.cpp
void HTMLCanvasElement::to_data_url(String const& type)
{
    // Notify fingerprint detector
    if (auto* detector = document().fingerprint_detector()) {
        detector->on_canvas_operation("toDataURL"_string);

        if (detector->calculate_score().score > 0.7f) {
            // High fingerprinting suspicion
            // Randomize output slightly
            return randomized_canvas_data_url(type);
        }
    }

    return actual_canvas_data_url(type);
}
```

### Defensive Strategies

**Score-based Response:**
- **0.0-0.3**: No action (legitimate use)
- **0.3-0.6**: Log and track
- **0.6-0.8**: Subtle randomization (noise injection)
- **0.8-1.0**: Block or prompt user

**Randomization Techniques:**
- Canvas: Add ±1-2 pixel noise to rendering
- WebGL: Slightly alter precision
- AudioContext: Add imperceptible noise
- Navigator: Rotate through small set of values

---

## 4. Advanced Phishing URL Analysis

### Multi-Layer Analysis Pipeline

```cpp
// Services/Sentinel/PhishingDetector.h
class PhishingDetector {
public:
    struct URLAnalysis {
        float homograph_score;     // Unicode lookalike detection
        float levenshtein_score;   // Distance to popular domains
        float entropy_score;       // URL structure entropy
        float certificate_score;   // SSL/TLS trust analysis
        float overall_risk;        // Combined score
        Vector<String> warnings;
    };

    ErrorOr<URLAnalysis> analyze_url(URL::URL const& url);

private:
    ErrorOr<float> detect_homograph_attack(String const& domain);
    ErrorOr<float> calculate_domain_similarity(String const& domain);
    ErrorOr<float> analyze_url_entropy(URL::URL const& url);
    ErrorOr<float> validate_certificate(String const& domain);
};
```

### Unicode Homograph Detection

**Research Findings (2025):**
- Convert hostnames to punycode and back for normalization
- Levenshtein distance after normalization
- Visual similarity mapping (е vs e, о vs o)
- Machine learning (Siamese neural networks)

```cpp
ErrorOr<float> PhishingDetector::detect_homograph_attack(String const& domain)
{
    // 1. Convert to punycode
    auto punycode = TRY(to_punycode(domain));

    // 2. Check against whitelist of top 10,000 domains
    static Vector<String> popular_domains = load_popular_domains();

    float min_distance = 1.0f;
    for (auto const& popular : popular_domains) {
        auto popular_puny = TRY(to_punycode(popular));

        // 3. Calculate Levenshtein distance
        auto distance = levenshtein_distance(punycode, popular_puny);
        auto normalized = static_cast<float>(distance) / max(punycode.bytes().size(),
                                                             popular_puny.bytes().size());

        // 4. Visual similarity check
        if (normalized < 0.3f) {  // <30% different
            auto visual_sim = calculate_visual_similarity(domain, popular);
            if (visual_sim > 0.7f) {  // >70% visually similar
                return 0.9f;  // High homograph suspicion
            }
        }

        min_distance = min(min_distance, normalized);
    }

    return 1.0f - min_distance;  // Closer = higher suspicion
}
```

### Visual Similarity Map

```cpp
static HashMap<u32, char> create_confusable_map()
{
    return {
        { 0x0430, 'a' },  // Cyrillic а -> Latin a
        { 0x0435, 'e' },  // Cyrillic е -> Latin e
        { 0x043E, 'o' },  // Cyrillic о -> Latin o
        { 0x0440, 'p' },  // Cyrillic р -> Latin p
        { 0x0441, 'c' },  // Cyrillic с -> Latin c
        // ... more mappings
    };
}

float calculate_visual_similarity(String const& domain1, String const& domain2)
{
    static auto confusables = create_confusable_map();

    auto normalized1 = normalize_with_confusables(domain1, confusables);
    auto normalized2 = normalize_with_confusables(domain2, confusables);

    return 1.0f - (levenshtein_distance(normalized1, normalized2) /
                   static_cast<float>(max(normalized1.length(), normalized2.length())));
}
```

### URL Entropy Analysis

```cpp
ErrorOr<float> PhishingDetector::analyze_url_entropy(URL::URL const& url)
{
    float entropy_score = 0.0f;

    // 1. Domain entropy (should be low for legitimate sites)
    auto domain_entropy = calculate_shannon_entropy(url.host()->serialize());
    if (domain_entropy > 4.0f) entropy_score += 0.3f;

    // 2. Path complexity (excessive subdomains)
    auto subdomain_count = url.host()->serialize().split('.').size();
    if (subdomain_count > 4) entropy_score += 0.2f;

    // 3. Suspicious parameters
    if (url.query().has_value()) {
        auto query = url.query().value();
        if (query.contains("login"sv) || query.contains("password"sv))
            entropy_score += 0.3f;
    }

    // 4. Long URL (>100 chars often phishing)
    if (url.to_string().bytes().size() > 100)
        entropy_score += 0.2f;

    return min(1.0f, entropy_score);
}
```

### SSL Certificate Validation

```cpp
ErrorOr<float> PhishingDetector::validate_certificate(String const& domain)
{
    auto cert = TRY(fetch_ssl_certificate(domain));

    float risk_score = 0.0f;

    // 1. Self-signed certificate
    if (cert.is_self_signed()) risk_score += 0.5f;

    // 2. Certificate age (phishing sites use fresh certs)
    auto age_days = cert.age().to_days();
    if (age_days < 30) risk_score += 0.3f;

    // 3. Certificate authority trust
    if (!cert.is_trusted_ca()) risk_score += 0.4f;

    // 4. Domain mismatch
    if (!cert.matches_domain(domain)) risk_score += 0.6f;

    return min(1.0f, risk_score);
}
```

---

## 5. Network Behavioral Analysis

### Research Findings (2025)

**Detection Capabilities (2025):**
- DGA detection: 97% accuracy (data plane), 99% (control plane)
- C2 communication detection via behavioral analysis
- DoH (DNS over HTTPS) traffic analysis using hierarchical ML
- Octo2 malware uses DGA for dynamic C2 (challenging to detect)

**Detection Methods:**
- Machine learning + frequency analysis for algorithmic domains
- Behavioral analytics for unusual communication patterns
- AI models trained on millions of malicious packets

### Architecture

```cpp
// Services/RequestServer/BehavioralAnalyzer.h
class BehavioralAnalyzer {
public:
    struct ConnectionPattern {
        URL::URL destination;
        u64 request_count;
        u64 bytes_sent;
        u64 bytes_received;
        Vector<Duration> intervals;  // Time between requests
        UnixDateTime first_seen;
        UnixDateTime last_seen;
    };

    struct AnomalyScore {
        float dga_score;        // Domain generation algorithm suspicion
        float beaconing_score;  // Regular interval communication
        float exfiltration_score;  // Large upload volumes
        float overall_risk;
        Vector<String> indicators;
    };

    void on_request(URL::URL const& url, u64 bytes_sent, u64 bytes_received);
    ErrorOr<AnomalyScore> analyze_domain(String const& domain);
    ErrorOr<AnomalyScore> analyze_connection_pattern(ConnectionPattern const& pattern);

private:
    HashMap<String, ConnectionPattern> m_connections;
};
```

### DGA Detection (Shannon Entropy)

```cpp
ErrorOr<float> BehavioralAnalyzer::detect_dga(String const& domain)
{
    // 1. Shannon entropy (DGA domains have high entropy)
    float entropy = calculate_shannon_entropy(domain);
    float dga_score = 0.0f;

    if (entropy > 3.5f) dga_score += 0.4f;  // High entropy
    if (entropy > 4.0f) dga_score += 0.3f;  // Very high entropy

    // 2. Character distribution (DGA has uniform distribution)
    auto char_dist = calculate_character_distribution(domain);
    if (is_uniform_distribution(char_dist, 0.1f)) {
        dga_score += 0.3f;
    }

    // 3. Consonant clusters (DGA often has unusual patterns)
    auto consonant_ratio = count_consonant_clusters(domain);
    if (consonant_ratio > 0.6f) dga_score += 0.2f;

    // 4. No vowels or excessive vowels
    auto vowel_ratio = count_vowels(domain) / static_cast<float>(domain.bytes().size());
    if (vowel_ratio < 0.2f || vowel_ratio > 0.6f) dga_score += 0.1f;

    return min(1.0f, dga_score);
}
```

### Beaconing Detection

```cpp
ErrorOr<float> BehavioralAnalyzer::detect_beaconing(ConnectionPattern const& pattern)
{
    if (pattern.intervals.size() < 5) return 0.0f;  // Need >5 samples

    // 1. Calculate interval statistics
    float mean_interval = calculate_mean(pattern.intervals);
    float stddev = calculate_stddev(pattern.intervals, mean_interval);

    // 2. Low variance = regular beaconing
    float coefficient_of_variation = stddev / mean_interval;
    if (coefficient_of_variation < 0.1f) {  // Very regular
        return 0.9f;  // High beaconing suspicion
    }
    if (coefficient_of_variation < 0.3f) {  // Somewhat regular
        return 0.6f;
    }

    return 0.0f;  // Normal variance
}
```

### Data Exfiltration Detection

```cpp
ErrorOr<float> BehavioralAnalyzer::detect_exfiltration(ConnectionPattern const& pattern)
{
    float exfil_score = 0.0f;

    // 1. Upload ratio (normal browsing: downloads > uploads)
    float upload_ratio = static_cast<float>(pattern.bytes_sent) /
                        static_cast<float>(pattern.bytes_sent + pattern.bytes_received);

    if (upload_ratio > 0.7f) exfil_score += 0.4f;  // >70% uploads
    if (upload_ratio > 0.9f) exfil_score += 0.3f;  // >90% uploads

    // 2. Total upload volume
    if (pattern.bytes_sent > 100_MB) exfil_score += 0.3f;

    // 3. Unusual destination (non-CDN, non-cloud storage)
    if (!is_known_upload_service(pattern.destination)) {
        exfil_score += 0.2f;
    }

    return min(1.0f, exfil_score);
}
```

### Integration with RequestServer

```cpp
// Services/RequestServer/ConnectionFromClient.cpp
void ConnectionFromClient::start_request(...)
{
    // Existing request handling...

    // Behavioral analysis
    m_behavioral_analyzer->on_request(url, body.bytes().size(), 0);

    // Async analysis (don't block request)
    Threading::BackgroundAction::create(
        [analyzer = m_behavioral_analyzer, domain = url.host()->serialize()]() {
            auto score = analyzer->analyze_domain(domain);
            if (score.is_error()) return;

            if (score.value().overall_risk > 0.8f) {
                // Alert user about suspicious network activity
                notify_suspicious_connection(domain, score.value());
            }
        });

    // Continue with request...
}
```

---

## Performance & Privacy Summary

### Performance Targets

| Component | Target | Measurement |
|-----------|--------|-------------|
| ML Inference | <100ms | Per file scan |
| Fingerprint Detection | <1ms | Per API call |
| URL Analysis | <50ms | Per navigation |
| Network Analysis | <10ms | Per request (async) |
| Total Overhead | <5% | Page load time |

### Privacy Guarantees

| Feature | Privacy Mechanism | Guarantee |
|---------|------------------|-----------|
| ML Detection | On-device only | No data leaves device |
| Federated Learning | Differential privacy (ε=0.1) | Strong privacy |
| Threat Feeds | Bloom filters | No query leakage |
| Fingerprint Detection | Local scoring | No external calls |
| Network Analysis | Local patterns only | No packet capture |

---

**Document Status**: Research Complete, Ready for Implementation
**Next Steps**: Phase-by-phase implementation per Milestone 0.4 plan
**Estimated Timeline**: 4-6 weeks for full implementation

---

*Created: 2025-10-31*
*Research Sources: 2025 academic papers, industry best practices, browser implementations*
*Ladybird Browser - Sentinel Advanced Threat Detection*
