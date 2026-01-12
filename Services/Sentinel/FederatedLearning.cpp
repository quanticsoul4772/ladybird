/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "FederatedLearning.h"
#include <AK/Math.h>
#include <AK/Random.h>
#include <AK/StringBuilder.h>

namespace Sentinel {

ErrorOr<NonnullOwnPtr<FederatedLearning>> FederatedLearning::create(PrivacyConfig const& config)
{
    if (config.epsilon <= 0) {
        return Error::from_string_literal("Privacy budget epsilon must be positive");
    }

    if (config.delta < 0 || config.delta >= 1) {
        return Error::from_string_literal("Privacy parameter delta must be in [0, 1)");
    }

    if (config.sensitivity <= 0) {
        return Error::from_string_literal("Sensitivity must be positive");
    }

    auto learning = adopt_own(*new FederatedLearning(config));

    dbgln("FederatedLearning: Created with ε={}, δ={}, sensitivity={}, min_participants={}",
        config.epsilon, config.delta, config.sensitivity, config.min_participants);

    return learning;
}

ErrorOr<NonnullOwnPtr<FederatedLearning>> FederatedLearning::create()
{
    return create(PrivacyConfig {});
}

FederatedLearning::FederatedLearning(PrivacyConfig const& config)
    : m_config(config)
{
}

FederatedLearning::LocalGradient FederatedLearning::privatize_gradient(
    Vector<float> const& raw_gradient,
    float epsilon)
{
    LocalGradient result;
    result.epsilon = epsilon;
    result.sample_count = 1;

    // Calculate Laplace noise scale
    // For L1 sensitivity, scale = sensitivity / epsilon
    float scale = 1.0f / epsilon;

    // Add Laplace noise to each weight
    result.weights.ensure_capacity(raw_gradient.size());
    for (auto weight : raw_gradient) {
        float noisy_weight = weight + laplace_noise(scale);
        result.weights.append(noisy_weight);
    }

    dbgln("FederatedLearning: Privatized gradient with {} dimensions, ε={}, noise_scale={}",
        raw_gradient.size(), epsilon, scale);

    return result;
}

FederatedLearning::LocalGradient FederatedLearning::privatize_gradient_gaussian(
    Vector<float> const& raw_gradient) const
{
    LocalGradient result;
    result.epsilon = m_config.epsilon;
    result.sample_count = 1;

    // Clip gradient if configured
    Vector<float> clipped_gradient = raw_gradient;
    if (m_config.clip_gradients) {
        clipped_gradient = clip_gradient(raw_gradient, m_config.clip_norm);
    }

    // Calculate Gaussian noise standard deviation
    float std_dev = calculate_gaussian_std(
        m_config.epsilon,
        m_config.delta,
        m_config.sensitivity);

    // Add Gaussian noise to each weight
    result.weights.ensure_capacity(clipped_gradient.size());
    float total_noise = 0;

    for (auto weight : clipped_gradient) {
        float noise = gaussian_noise(std_dev);
        float noisy_weight = weight + noise;
        result.weights.append(noisy_weight);
        total_noise += AK::abs(noise);
    }

    // Update statistics
    m_stats.total_gradients_processed++;
    if (!m_stats.average_noise_added.has_value()) {
        m_stats.average_noise_added = total_noise / clipped_gradient.size();
    } else {
        // Running average
        float prev_avg = m_stats.average_noise_added.value();
        m_stats.average_noise_added = (prev_avg + total_noise / clipped_gradient.size()) / 2;
    }

    return result;
}

ErrorOr<FederatedLearning::AggregatedModel> FederatedLearning::aggregate_gradients(
    Vector<LocalGradient> const& peer_gradients) const
{
    if (peer_gradients.is_empty()) {
        return Error::from_string_literal("No gradients to aggregate");
    }

    // Check privacy requirements
    if (!meets_privacy_requirements(peer_gradients)) {
        m_stats.rejected_for_privacy++;
        return Error::from_string_literal("Aggregation does not meet privacy requirements");
    }

    // Validate dimensions
    if (!validate_gradient_dimensions(peer_gradients)) {
        return Error::from_string_literal("Gradient dimensions do not match");
    }

    AggregatedModel result;
    result.num_participants = peer_gradients.size();

    // Calculate total samples and effective epsilon
    for (auto const& gradient : peer_gradients) {
        result.total_samples += gradient.sample_count;
        result.effective_epsilon = AK::max(result.effective_epsilon, gradient.epsilon);
    }

    // Weighted average of gradients
    size_t dimension = peer_gradients[0].weights.size();
    result.weights.resize(dimension);

    for (size_t i = 0; i < dimension; ++i) {
        double weighted_sum = 0;
        for (auto const& gradient : peer_gradients) {
            double weight = static_cast<double>(gradient.sample_count) / result.total_samples;
            weighted_sum += gradient.weights[i] * weight;
        }
        result.weights[i] = static_cast<float>(weighted_sum);
    }

    // Update statistics
    m_stats.total_aggregations++;
    m_stats.accumulated_privacy_loss += result.effective_epsilon;

    dbgln("FederatedLearning: Aggregated {} gradients, {} total samples, effective ε={}",
        peer_gradients.size(), result.total_samples, result.effective_epsilon);

    return result;
}

ErrorOr<Vector<float>> FederatedLearning::federated_average(
    Vector<LocalGradient> const& peer_gradients) const
{
    auto aggregated = TRY(aggregate_gradients(peer_gradients));
    return aggregated.weights;
}

bool FederatedLearning::meets_privacy_requirements(
    Vector<LocalGradient> const& peer_gradients) const
{
    // Check k-anonymity (minimum participants)
    if (peer_gradients.size() < m_config.min_participants) {
        dbgln("FederatedLearning: Only {} participants, need {} for k-anonymity",
            peer_gradients.size(), m_config.min_participants);
        return false;
    }

    // Check privacy budget
    float max_epsilon = 0;
    for (auto const& gradient : peer_gradients) {
        max_epsilon = AK::max(max_epsilon, gradient.epsilon);
    }

    if (max_epsilon > m_config.epsilon * 10) { // Allow some flexibility
        dbgln("FederatedLearning: Privacy budget exceeded: {} > {}",
            max_epsilon, m_config.epsilon * 10);
        return false;
    }

    return true;
}

float FederatedLearning::calculate_privacy_loss(
    size_t num_participants,
    size_t num_rounds) const
{
    // Privacy loss accumulates over rounds
    // Using advanced composition theorem: ε_total ≈ ε_round * sqrt(2 * num_rounds * ln(1/δ))

    float single_round_epsilon = m_config.epsilon;
    float composition_factor = AK::sqrt(2.0f * num_rounds * AK::log(1.0f / m_config.delta));

    // Additional privacy amplification from subsampling
    float sampling_rate = AK::min(1.0f, 100.0f / num_participants);
    float amplified_epsilon = single_round_epsilon * sampling_rate;

    float total_privacy_loss = amplified_epsilon * composition_factor;

    return total_privacy_loss;
}

Vector<float> FederatedLearning::clip_gradient(
    Vector<float> const& gradient,
    float max_norm)
{
    // Calculate L2 norm
    float norm_squared = 0;
    for (auto value : gradient) {
        norm_squared += value * value;
    }
    float norm = AK::sqrt(norm_squared);

    // If norm is within bounds, return as-is
    if (norm <= max_norm) {
        return gradient;
    }

    // Scale down to max_norm
    float scale = max_norm / norm;
    Vector<float> clipped;
    clipped.ensure_capacity(gradient.size());

    for (auto value : gradient) {
        clipped.append(value * scale);
    }

    return clipped;
}

Vector<float> FederatedLearning::generate_synthetic_gradient(size_t dimension)
{
    Vector<float> gradient;
    gradient.ensure_capacity(dimension);

    // Generate random values with decreasing magnitude
    for (size_t i = 0; i < dimension; ++i) {
        // Random value between -1 and 1, with decay
        float decay = 1.0f / (1.0f + i * 0.1f);
        float value = (static_cast<float>(get_random<i32>()) / INT32_MAX) * decay;
        gradient.append(value);
    }

    return gradient;
}

float FederatedLearning::laplace_noise(float scale)
{
    // Generate Laplace noise using the inverse CDF method
    // Laplace CDF inverse: F^(-1)(p) = -scale * sign(p - 0.5) * ln(1 - 2*|p - 0.5|)

    float u = static_cast<float>(get_random<u32>()) / UINT32_MAX; // Uniform [0, 1]

    // Avoid exact 0 or 1 to prevent ln(0)
    u = AK::max(0.0001f, AK::min(0.9999f, u));

    float sign = (u < 0.5f) ? -1.0f : 1.0f;
    float magnitude = -AK::log(1.0f - 2.0f * AK::abs(u - 0.5f));

    return scale * sign * magnitude;
}

float FederatedLearning::gaussian_noise(float std_dev)
{
    // Box-Muller transform for Gaussian noise
    static bool has_spare = false;
    static float spare;

    if (has_spare) {
        has_spare = false;
        return spare * std_dev;
    }

    has_spare = true;

    // Generate two uniform random numbers
    float u1 = static_cast<float>(get_random<u32>() + 1) / (UINT32_MAX + 1.0f); // Avoid 0
    float u2 = static_cast<float>(get_random<u32>()) / UINT32_MAX;

    float magnitude = AK::sqrt(-2.0f * AK::log(u1));
    float angle = 2.0f * AK::Pi<float> * u2;

    spare = magnitude * AK::cos(angle);
    return magnitude * AK::sin(angle) * std_dev;
}

float FederatedLearning::calculate_noise_scale(float epsilon, float sensitivity) const
{
    // Laplace mechanism: scale = sensitivity / epsilon
    return sensitivity / epsilon;
}

float FederatedLearning::calculate_gaussian_std(
    float epsilon, float delta, float sensitivity) const
{
    // Gaussian mechanism: σ = sensitivity * sqrt(2 * ln(1.25/δ)) / ε
    if (delta <= 0) {
        // Use Laplace noise as fallback
        return calculate_noise_scale(epsilon, sensitivity);
    }

    float c = AK::sqrt(2.0f * AK::log(1.25f / delta));
    return sensitivity * c / epsilon;
}

bool FederatedLearning::validate_gradient_dimensions(Vector<LocalGradient> const& gradients)
{
    if (gradients.is_empty()) {
        return true;
    }

    size_t expected_dimension = gradients[0].weights.size();

    for (size_t i = 1; i < gradients.size(); ++i) {
        if (gradients[i].weights.size() != expected_dimension) {
            dbgln("FederatedLearning: Gradient dimension mismatch: {} != {}",
                gradients[i].weights.size(), expected_dimension);
            return false;
        }
    }

    return true;
}

}