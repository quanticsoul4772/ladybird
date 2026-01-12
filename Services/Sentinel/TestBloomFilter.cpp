/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "BloomFilter.h"
#include "FederatedLearning.h"
#include "IPFSThreatSync.h"
#include "ThreatFeed.h"
#include <AK/Random.h>
#include <AK/StringBuilder.h>
#include <LibCore/System.h>
#include <LibTest/TestCase.h>

using namespace Sentinel;

TEST_CASE(bloom_filter_basic_operations)
{
    // Create a small bloom filter for testing
    auto filter_result = BloomFilter::create(10000, 5);
    EXPECT(!filter_result.is_error());
    auto filter = filter_result.release_value();

    // Test adding and checking strings
    String test_string = MUST(String::from_utf8("test_string_123"sv));
    filter->add(test_string);
    EXPECT(filter->contains(test_string));

    // Test that non-added strings are not found
    String not_added = MUST(String::from_utf8("not_added_456"sv));
    EXPECT(!filter->contains(not_added));

    // Test adding multiple items
    for (size_t i = 0; i < 100; ++i) {
        String item = String::formatted("item_{}", i).release_value_but_fixme_should_propagate_errors();
        filter->add(item);
        EXPECT(filter->contains(item));
    }
}

TEST_CASE(bloom_filter_false_positive_rate)
{
    // Create a larger bloom filter
    auto filter_result = BloomFilter::create(100000, 7);
    EXPECT(!filter_result.is_error());
    auto filter = filter_result.release_value();

    // Add 1000 items
    size_t num_items = 1000;
    for (size_t i = 0; i < num_items; ++i) {
        String item = String::formatted("added_{}", i).release_value_but_fixme_should_propagate_errors();
        filter->add(item);
    }

    // Check false positive rate with non-added items
    size_t false_positives = 0;
    size_t test_count = 10000;

    for (size_t i = 0; i < test_count; ++i) {
        String test_item = String::formatted("not_added_{}", i).release_value_but_fixme_should_propagate_errors();
        if (filter->contains(test_item)) {
            false_positives++;
        }
    }

    double actual_fpr = static_cast<double>(false_positives) / test_count;
    double estimated_fpr = filter->estimated_false_positive_rate();

    // The actual FPR should be close to the estimated FPR
    // Allow for some variation (within 50% of estimated)
    EXPECT(actual_fpr < estimated_fpr * 1.5);

    // The FPR should be reasonably low
    EXPECT(actual_fpr < 0.05); // Less than 5%

    outln("Bloom filter FPR test: actual={:.3f}%, estimated={:.3f}%",
        actual_fpr * 100, estimated_fpr * 100);
}

TEST_CASE(bloom_filter_serialization)
{
    // Create and populate a filter
    auto filter1_result = BloomFilter::create(50000, 5);
    EXPECT(!filter1_result.is_error());
    auto filter1 = filter1_result.release_value();

    // Add test data
    Vector<String> test_data;
    for (size_t i = 0; i < 100; ++i) {
        String item = String::formatted("serialize_test_{}", i).release_value_but_fixme_should_propagate_errors();
        test_data.append(item);
        filter1->add(item);
    }

    // Serialize
    auto serialize_result = filter1->serialize();
    EXPECT(!serialize_result.is_error());
    auto serialized = serialize_result.release_value();

    // Deserialize
    auto filter2_result = BloomFilter::deserialize(serialized);
    EXPECT(!filter2_result.is_error());
    auto filter2 = filter2_result.release_value();

    // Verify all items are still found
    for (auto const& item : test_data) {
        EXPECT(filter2->contains(item));
    }

    // Verify parameters match
    EXPECT_EQ(filter1->size_bits(), filter2->size_bits());
    EXPECT_EQ(filter1->num_hashes(), filter2->num_hashes());
    EXPECT_EQ(filter1->bits_set(), filter2->bits_set());
}

TEST_CASE(bloom_filter_merge)
{
    // Create two filters with same parameters
    auto filter1_result = BloomFilter::create(20000, 4);
    EXPECT(!filter1_result.is_error());
    auto filter1 = filter1_result.release_value();

    auto filter2_result = BloomFilter::create(20000, 4);
    EXPECT(!filter2_result.is_error());
    auto filter2 = filter2_result.release_value();

    // Add different items to each filter
    Vector<String> items1, items2;
    for (size_t i = 0; i < 50; ++i) {
        String item = String::formatted("filter1_{}", i).release_value_but_fixme_should_propagate_errors();
        items1.append(item);
        filter1->add(item);
    }

    for (size_t i = 0; i < 50; ++i) {
        String item = String::formatted("filter2_{}", i).release_value_but_fixme_should_propagate_errors();
        items2.append(item);
        filter2->add(item);
    }

    // Merge filter2 into filter1
    auto merge_result = filter1->merge(*filter2);
    EXPECT(!merge_result.is_error());

    // Check that filter1 now contains all items
    for (auto const& item : items1) {
        EXPECT(filter1->contains(item));
    }
    for (auto const& item : items2) {
        EXPECT(filter1->contains(item));
    }

    // Test merging filters with different parameters should fail
    auto filter3_result = BloomFilter::create(30000, 5); // Different parameters
    EXPECT(!filter3_result.is_error());
    auto filter3 = filter3_result.release_value();

    auto bad_merge = filter1->merge(*filter3);
    EXPECT(bad_merge.is_error());
}

TEST_CASE(threat_feed_basic_operations)
{
    // Create threat feed
    auto feed_result = ThreatFeed::create();
    EXPECT(!feed_result.is_error());
    auto feed = feed_result.release_value();

    // Add some threat hashes
    StringBuilder builder1;
    for (int i = 0; i < 64; i++) builder1.append('a');
    String hash1 = MUST(builder1.to_string());

    StringBuilder builder2;
    for (int i = 0; i < 64; i++) builder2.append('b');
    String hash2 = MUST(builder2.to_string());

    StringBuilder builder3;
    for (int i = 0; i < 64; i++) builder3.append('c');
    String hash3 = MUST(builder3.to_string());

    auto add1 = feed->add_threat_hash(hash1, ThreatFeed::ThreatCategory::Malware, 8);
    EXPECT(!add1.is_error());

    auto add2 = feed->add_threat_hash(hash2, ThreatFeed::ThreatCategory::Phishing, 6);
    EXPECT(!add2.is_error());

    auto add3 = feed->add_threat_hash(hash3, ThreatFeed::ThreatCategory::Exploit, 9);
    EXPECT(!add3.is_error());

    // Check that hashes are detected
    EXPECT(feed->probably_malicious(hash1));
    EXPECT(feed->probably_malicious(hash2));
    EXPECT(feed->probably_malicious(hash3));

    // Check that non-threat hashes are not detected
    StringBuilder builder4;
    for (int i = 0; i < 64; i++) builder4.append('d');
    String safe_hash = MUST(builder4.to_string());
    EXPECT(!feed->probably_malicious(safe_hash));

    // Get statistics
    auto stats = feed->get_statistics();
    EXPECT(stats.total_threats >= 3);
    EXPECT(stats.malware_count >= 1);
    EXPECT(stats.phishing_count >= 1);
    EXPECT(stats.exploit_count >= 1);
}

TEST_CASE(ipfs_threat_sync_mock)
{
    // Create IPFS sync (will use mock implementation)
    auto sync_result = IPFSThreatSync::create();
    EXPECT(!sync_result.is_error());
    auto sync = sync_result.release_value();

    // Should not have IPFS available (mock mode)
    EXPECT(!sync->is_ipfs_available());

    // Get mock threats
    auto threats_result = sync->fetch_latest_threats();
    EXPECT(!threats_result.is_error());
    auto threats = threats_result.release_value();

    // Should have some mock threats
    EXPECT(threats.size() > 0);

    // Verify threat data format
    for (auto const& threat : threats) {
        // SHA256 hash should be 64 characters
        EXPECT_EQ(threat.hash.bytes().size(), 64u);
        // Severity should be 1-10
        EXPECT(threat.severity >= 1 && threat.severity <= 10);
    }

    // Test peer management
    auto peers = sync->get_connected_peers();
    EXPECT(peers.size() > 0);

    // Test subscribing to feeds
    String mock_feed = MUST(String::from_utf8("QmMockFeedHash123"sv));
    auto subscribe_result = sync->subscribe_to_feed(mock_feed);
    EXPECT(!subscribe_result.is_error());

    // Test node status
    auto status = sync->get_node_status();
    EXPECT(!status.is_online); // Mock mode
    EXPECT(status.num_peers > 0);
}

TEST_CASE(federated_learning_gradient_privatization)
{
    // Create federated learning instance
    FederatedLearning::PrivacyConfig config;
    config.epsilon = 0.5f;
    config.delta = 1e-5f;
    config.sensitivity = 1.0f;
    config.min_participants = 3;

    auto fl_result = FederatedLearning::create(config);
    EXPECT(!fl_result.is_error());
    auto fl = fl_result.release_value();

    // Generate synthetic gradient
    auto gradient = FederatedLearning::generate_synthetic_gradient(100);
    EXPECT_EQ(gradient.size(), 100u);

    // Privatize gradient with Laplace noise
    auto privatized = FederatedLearning::privatize_gradient(gradient, 0.5f);
    EXPECT_EQ(privatized.weights.size(), gradient.size());
    EXPECT_EQ(privatized.epsilon, 0.5f);

    // Privatize with Gaussian noise
    auto privatized_gaussian = fl->privatize_gradient_gaussian(gradient);
    EXPECT_EQ(privatized_gaussian.weights.size(), gradient.size());

    // Verify noise was added (weights should differ)
    bool has_difference = false;
    for (size_t i = 0; i < gradient.size(); ++i) {
        if (gradient[i] != privatized.weights[i]) {
            has_difference = true;
            break;
        }
    }
    EXPECT(has_difference);
}

TEST_CASE(federated_learning_aggregation)
{
    // Create federated learning instance
    FederatedLearning::PrivacyConfig config;
    config.epsilon = 1.0f;
    config.min_participants = 2;

    auto fl_result = FederatedLearning::create(config);
    EXPECT(!fl_result.is_error());
    auto fl = fl_result.release_value();

    // Create multiple gradients
    Vector<FederatedLearning::LocalGradient> gradients;

    for (size_t i = 0; i < 5; ++i) {
        auto gradient = FederatedLearning::generate_synthetic_gradient(50);
        auto privatized = FederatedLearning::privatize_gradient(gradient, 1.0f);
        privatized.sample_count = 100 + i * 10; // Different sample counts
        gradients.append(move(privatized));
    }

    // Aggregate gradients
    auto aggregated_result = fl->aggregate_gradients(gradients);
    EXPECT(!aggregated_result.is_error());
    auto aggregated = aggregated_result.release_value();

    EXPECT_EQ(aggregated.weights.size(), 50u);
    EXPECT_EQ(aggregated.num_participants, 5u);
    EXPECT(aggregated.total_samples > 0);

    // Test federated average
    auto avg_result = fl->federated_average(gradients);
    EXPECT(!avg_result.is_error());
    auto avg = avg_result.release_value();
    EXPECT_EQ(avg.size(), 50u);

    // Test privacy requirements check
    EXPECT(fl->meets_privacy_requirements(gradients));

    // Test with too few participants
    Vector<FederatedLearning::LocalGradient> few_gradients;
    few_gradients.append(gradients[0]);
    EXPECT(!fl->meets_privacy_requirements(few_gradients));
}

TEST_CASE(federated_learning_gradient_clipping)
{
    // Test gradient clipping
    Vector<float> gradient;
    for (size_t i = 0; i < 10; ++i) {
        gradient.append(static_cast<float>(i));
    }

    // Clip to max norm of 5
    auto clipped = FederatedLearning::clip_gradient(gradient, 5.0f);
    EXPECT_EQ(clipped.size(), gradient.size());

    // Calculate norm of clipped gradient
    float norm_squared = 0;
    for (auto value : clipped) {
        norm_squared += value * value;
    }
    float norm = sqrt(norm_squared);

    // Should be approximately 5
    EXPECT(norm <= 5.1f && norm >= 4.9f);
}

TEST_CASE(bloom_filter_stress_test)
{
    // Large-scale test with realistic parameters
    auto filter_result = BloomFilter::create(1000000, 7); // 1M bits, 7 hashes
    EXPECT(!filter_result.is_error());
    auto filter = filter_result.release_value();

    // Add 10,000 items
    size_t num_items = 10000;
    for (size_t i = 0; i < num_items; ++i) {
        StringBuilder builder;
        for (size_t j = 0; j < 32; ++j) {
            builder.appendff("{:02x}", get_random<u8>());
        }
        String hash = builder.to_string().release_value_but_fixme_should_propagate_errors();
        filter->add(hash);
    }

    size_t estimated = filter->estimated_item_count();
    // Should be within 20% of actual count
    EXPECT(estimated > num_items * 0.8 && estimated < num_items * 1.2);

    outln("Bloom filter stress test: added={}, estimated={}, bits_set={}",
        num_items, estimated, filter->bits_set());
}