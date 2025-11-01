/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>
#include <AK/NonnullOwnPtr.h>
#include <AK/Optional.h>
#include <AK/String.h>
#include <AK/Vector.h>

namespace Sentinel {

class FederatedLearning {
public:
    // Gradient structure for federated learning
    struct LocalGradient {
        Vector<float> weights;
        float epsilon { 0.1f };  // Privacy budget (smaller = more private)
        u64 sample_count { 0 };
        Optional<String> model_version;
        Optional<u64> round_number;
    };

    // Aggregation result
    struct AggregatedModel {
        Vector<float> weights;
        u64 total_samples { 0 };
        size_t num_participants { 0 };
        float effective_epsilon { 0.0f };
    };

    // Privacy configuration
    struct PrivacyConfig {
        float epsilon { 0.1f };           // Overall privacy budget
        float delta { 1e-5f };             // Probability of privacy breach
        float sensitivity { 1.0f };        // L2 sensitivity of gradients
        size_t min_participants { 100 };   // K-anonymity threshold
        bool clip_gradients { true };      // Gradient clipping for bounded sensitivity
        float clip_norm { 1.0f };          // Maximum L2 norm for gradients
    };

    static ErrorOr<NonnullOwnPtr<FederatedLearning>> create(PrivacyConfig const& config);
    static ErrorOr<NonnullOwnPtr<FederatedLearning>> create();

    // Add Laplace noise for differential privacy
    static LocalGradient privatize_gradient(
        Vector<float> const& raw_gradient,
        float epsilon = 0.1f);

    // Add Gaussian noise for (ε,δ)-differential privacy
    LocalGradient privatize_gradient_gaussian(
        Vector<float> const& raw_gradient) const;

    // Aggregate gradients from peers using secure aggregation
    ErrorOr<AggregatedModel> aggregate_gradients(
        Vector<LocalGradient> const& peer_gradients) const;

    // Federated averaging algorithm
    ErrorOr<Vector<float>> federated_average(
        Vector<LocalGradient> const& peer_gradients) const;

    // Check if aggregation meets privacy requirements
    bool meets_privacy_requirements(
        Vector<LocalGradient> const& peer_gradients) const;

    // Calculate privacy loss for a given operation
    float calculate_privacy_loss(
        size_t num_participants,
        size_t num_rounds) const;

    // Clip gradient to bounded L2 norm
    static Vector<float> clip_gradient(
        Vector<float> const& gradient,
        float max_norm);

    // Generate synthetic data for testing
    static Vector<float> generate_synthetic_gradient(size_t dimension);

    // Configuration getters
    PrivacyConfig const& config() const { return m_config; }
    void set_config(PrivacyConfig const& config) { m_config = config; }

    // Statistics
    struct Statistics {
        u64 total_aggregations { 0 };
        u64 total_gradients_processed { 0 };
        float accumulated_privacy_loss { 0.0f };
        size_t rejected_for_privacy { 0 };
        Optional<float> average_noise_added;
    };

    Statistics get_statistics() const { return m_stats; }

private:
    FederatedLearning(PrivacyConfig const& config);

    // Generate Laplace noise with given scale
    static float laplace_noise(float scale);

    // Generate Gaussian noise with given standard deviation
    static float gaussian_noise(float std_dev);

    // Calculate noise scale for desired privacy level
    float calculate_noise_scale(float epsilon, float sensitivity) const;

    // Calculate Gaussian noise standard deviation
    float calculate_gaussian_std(float epsilon, float delta, float sensitivity) const;

    // Validate gradient dimensions
    static bool validate_gradient_dimensions(Vector<LocalGradient> const& gradients);

    PrivacyConfig m_config;
    mutable Statistics m_stats;
};

}