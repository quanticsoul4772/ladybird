/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Error.h>
#include <LibCore/RetryPolicy.h>
#include <LibTest/TestCase.h>
#include <errno.h>
#include <netdb.h>

using AK::Duration;

// Test helper: Counter for tracking function calls
static size_t s_call_count;
static void reset_call_count()
{
    s_call_count = 0;
}

// Test helper: Function that succeeds on first attempt
static ErrorOr<int> succeed_immediately()
{
    s_call_count++;
    return 42;
}

// Test helper: Function that fails N times then succeeds
static ErrorOr<int> succeed_after_n_attempts(size_t n)
{
    s_call_count++;
    if (s_call_count <= n)
        return Error::from_errno(EAGAIN);
    return 42;
}

// Test helper: Function that always fails
static ErrorOr<int> always_fail()
{
    s_call_count++;
    return Error::from_errno(EAGAIN);
}

// Test helper: Function that fails with non-retryable error
static ErrorOr<int> fail_non_retryable()
{
    s_call_count++;
    return Error::from_errno(ENOENT);  // File not found - permanent error
}

TEST_CASE(success_on_first_attempt)
{
    reset_call_count();

    Core::RetryPolicy policy(3, Duration::from_milliseconds(10));
    auto result = policy.execute(succeed_immediately);

    EXPECT(!result.is_error());
    if (!result.is_error()) {
        auto& inner = result.value();
        EXPECT(!inner.is_error());
        if (!inner.is_error())
            EXPECT_EQ(inner.value(), 42);
    }
    EXPECT_EQ(s_call_count, 1uz);  // Should only call once
    EXPECT_EQ(policy.metrics().total_executions, 1uz);
    EXPECT_EQ(policy.metrics().total_attempts, 1uz);
    EXPECT_EQ(policy.metrics().successful_executions, 1uz);
    EXPECT_EQ(policy.metrics().failed_executions, 0uz);
    EXPECT_EQ(policy.metrics().retried_executions, 0uz);
}

TEST_CASE(success_on_second_attempt)
{
    reset_call_count();

    Core::RetryPolicy policy(3, Duration::from_milliseconds(10));
    auto result = policy.execute([&] { return succeed_after_n_attempts(1); });

    EXPECT(!result.is_error());
    if (!result.is_error()) {
        auto& inner = result.value();
        EXPECT(!inner.is_error());
        if (!inner.is_error())
            EXPECT_EQ(inner.value(), 42);
    }
    EXPECT_EQ(s_call_count, 2uz);  // Should call twice (fail once, succeed once)
    EXPECT_EQ(policy.metrics().total_executions, 1uz);
    EXPECT_EQ(policy.metrics().total_attempts, 2uz);
    EXPECT_EQ(policy.metrics().successful_executions, 1uz);
    EXPECT_EQ(policy.metrics().failed_executions, 0uz);
    EXPECT_EQ(policy.metrics().retried_executions, 1uz);
}

TEST_CASE(success_on_third_attempt)
{
    reset_call_count();

    Core::RetryPolicy policy(3, Duration::from_milliseconds(10));
    auto result = policy.execute([&] { return succeed_after_n_attempts(2); });

    EXPECT(!result.is_error());
    if (!result.is_error()) {
        auto& inner = result.value();
        EXPECT(!inner.is_error());
        if (!inner.is_error())
            EXPECT_EQ(inner.value(), 42);
    }
    EXPECT_EQ(s_call_count, 3uz);  // Should call 3 times (fail twice, succeed once)
    EXPECT_EQ(policy.metrics().total_executions, 1uz);
    EXPECT_EQ(policy.metrics().total_attempts, 3uz);
    EXPECT_EQ(policy.metrics().successful_executions, 1uz);
    EXPECT_EQ(policy.metrics().retried_executions, 1uz);
}

TEST_CASE(failure_after_max_attempts)
{
    reset_call_count();

    Core::RetryPolicy policy(3, Duration::from_milliseconds(10));
    auto result = policy.execute(always_fail);

    EXPECT(result.is_error());
    EXPECT_EQ(s_call_count, 3uz);  // Should call 3 times (all fail)
    EXPECT_EQ(policy.metrics().total_executions, 1uz);
    EXPECT_EQ(policy.metrics().total_attempts, 3uz);
    EXPECT_EQ(policy.metrics().successful_executions, 0uz);
    EXPECT_EQ(policy.metrics().failed_executions, 1uz);
    EXPECT_EQ(policy.metrics().retried_executions, 0uz);
}

TEST_CASE(exponential_backoff_delays)
{
    Core::RetryPolicy policy(
        5,                                      // max_attempts
        Duration::from_milliseconds(100),       // initial_delay
        Duration::from_seconds(10),             // max_delay
        2.0,                                    // backoff_multiplier
        0.0                                     // no jitter for predictable testing
    );

    // Attempt 0 (first retry): 100ms * 2^0 = 100ms
    auto delay0 = policy.calculate_next_delay(0);
    EXPECT_EQ(delay0.to_milliseconds(), 100);

    // Attempt 1 (second retry): 100ms * 2^1 = 200ms
    auto delay1 = policy.calculate_next_delay(1);
    EXPECT_EQ(delay1.to_milliseconds(), 200);

    // Attempt 2 (third retry): 100ms * 2^2 = 400ms
    auto delay2 = policy.calculate_next_delay(2);
    EXPECT_EQ(delay2.to_milliseconds(), 400);

    // Attempt 3 (fourth retry): 100ms * 2^3 = 800ms
    auto delay3 = policy.calculate_next_delay(3);
    EXPECT_EQ(delay3.to_milliseconds(), 800);

    // Attempt 4 (fifth retry): 100ms * 2^4 = 1600ms
    auto delay4 = policy.calculate_next_delay(4);
    EXPECT_EQ(delay4.to_milliseconds(), 1600);
}

TEST_CASE(jitter_adds_randomness)
{
    Core::RetryPolicy policy(
        3,
        Duration::from_milliseconds(100),
        Duration::from_seconds(10),
        2.0,
        0.1  // 10% jitter
    );

    // Calculate delay 100 times and check they're in the expected range
    // With 10% jitter, delay should be in range [90ms, 110ms]
    for (size_t i = 0; i < 100; ++i) {
        auto delay = policy.calculate_next_delay(0);
        auto ms = delay.to_milliseconds();
        EXPECT(ms >= 90 && ms <= 110);
    }
}

TEST_CASE(max_delay_enforced)
{
    Core::RetryPolicy policy(
        10,                                     // max_attempts
        Duration::from_milliseconds(1000),      // initial_delay
        Duration::from_seconds(2),              // max_delay (2000ms)
        2.0,                                    // backoff_multiplier
        0.0                                     // no jitter
    );

    // Attempt 0: 1000ms * 2^0 = 1000ms (within max)
    auto delay0 = policy.calculate_next_delay(0);
    EXPECT_EQ(delay0.to_milliseconds(), 1000);

    // Attempt 1: 1000ms * 2^1 = 2000ms (at max)
    auto delay1 = policy.calculate_next_delay(1);
    EXPECT_EQ(delay1.to_milliseconds(), 2000);

    // Attempt 2: 1000ms * 2^2 = 4000ms (exceeds max, capped at 2000ms)
    auto delay2 = policy.calculate_next_delay(2);
    EXPECT_EQ(delay2.to_milliseconds(), 2000);

    // Attempt 5: 1000ms * 2^5 = 32000ms (exceeds max, capped at 2000ms)
    auto delay5 = policy.calculate_next_delay(5);
    EXPECT_EQ(delay5.to_milliseconds(), 2000);
}

TEST_CASE(custom_retry_predicate_retryable)
{
    reset_call_count();

    Core::RetryPolicy policy(3, Duration::from_milliseconds(10));

    // Set predicate that only retries EAGAIN
    policy.set_retry_predicate([](Error const& error) {
        return error.is_errno() && error.code() == EAGAIN;
    });

    // Test with EAGAIN (retryable)
    auto result = policy.execute([&] { return succeed_after_n_attempts(1); });

    EXPECT(!result.is_error());
    if (!result.is_error()) {
        auto& inner = result.value();
        EXPECT(!inner.is_error());
        if (!inner.is_error())
            EXPECT_EQ(inner.value(), 42);
    }
    EXPECT_EQ(s_call_count, 2uz);  // Should retry and succeed
}

TEST_CASE(custom_retry_predicate_non_retryable)
{
    reset_call_count();

    Core::RetryPolicy policy(3, Duration::from_milliseconds(10));

    // Set predicate that only retries EAGAIN
    policy.set_retry_predicate([](Error const& error) {
        return error.is_errno() && error.code() == EAGAIN;
    });

    // Test with ENOENT (non-retryable)
    auto result = policy.execute(fail_non_retryable);

    EXPECT(result.is_error());
    EXPECT_EQ(s_call_count, 1uz);  // Should NOT retry
    EXPECT_EQ(policy.metrics().failed_executions, 1uz);
}

TEST_CASE(database_retry_predicate)
{
    auto predicate = Core::RetryPolicy::database_retry_predicate();

    // Transient errors should be retryable
    EXPECT(predicate(Error::from_errno(EAGAIN)));
    EXPECT(predicate(Error::from_errno(ECONNREFUSED)));
    EXPECT(predicate(Error::from_errno(ETIMEDOUT)));
    EXPECT(predicate(Error::from_errno(EBUSY)));

    // Permanent errors should NOT be retryable
    EXPECT(!predicate(Error::from_errno(ENOENT)));
    EXPECT(!predicate(Error::from_errno(EACCES)));
    EXPECT(!predicate(Error::from_errno(EINVAL)));
    EXPECT(!predicate(Error::from_errno(ENOSPC)));

    // Non-errno errors should NOT be retryable
    EXPECT(!predicate(Error::from_string_literal("Some error")));
}

TEST_CASE(file_io_retry_predicate)
{
    auto predicate = Core::RetryPolicy::file_io_retry_predicate();

    // Transient errors should be retryable
    EXPECT(predicate(Error::from_errno(EAGAIN)));
    EXPECT(predicate(Error::from_errno(EBUSY)));
    EXPECT(predicate(Error::from_errno(EINTR)));
    EXPECT(predicate(Error::from_errno(ETXTBSY)));

    // Permanent errors should NOT be retryable
    EXPECT(!predicate(Error::from_errno(ENOENT)));
    EXPECT(!predicate(Error::from_errno(EACCES)));
    EXPECT(!predicate(Error::from_errno(ENOSPC)));
    EXPECT(!predicate(Error::from_errno(EROFS)));
}

TEST_CASE(ipc_retry_predicate)
{
    auto predicate = Core::RetryPolicy::ipc_retry_predicate();

    // Transient errors should be retryable
    EXPECT(predicate(Error::from_errno(EAGAIN)));
    EXPECT(predicate(Error::from_errno(ECONNREFUSED)));
    EXPECT(predicate(Error::from_errno(ECONNRESET)));
    EXPECT(predicate(Error::from_errno(ETIMEDOUT)));
    EXPECT(predicate(Error::from_errno(EPIPE)));

    // Permanent errors should NOT be retryable
    EXPECT(!predicate(Error::from_errno(EINVAL)));
    EXPECT(!predicate(Error::from_errno(EPROTO)));
}

TEST_CASE(network_retry_predicate)
{
    auto predicate = Core::RetryPolicy::network_retry_predicate();

    // Transient errors should be retryable
    EXPECT(predicate(Error::from_errno(EAGAIN)));
    EXPECT(predicate(Error::from_errno(ECONNREFUSED)));
    EXPECT(predicate(Error::from_errno(ENETDOWN)));
    EXPECT(predicate(Error::from_errno(ETIMEDOUT)));
    EXPECT(predicate(Error::from_errno(EAI_AGAIN)));

    // Permanent errors should NOT be retryable
    EXPECT(!predicate(Error::from_errno(EPROTO)));
}

TEST_CASE(reset_metrics)
{
    reset_call_count();

    Core::RetryPolicy policy(3, Duration::from_milliseconds(10));

    // Execute some operations
    (void)policy.execute(succeed_immediately);
    (void)policy.execute(always_fail);

    EXPECT_EQ(policy.metrics().total_executions, 2uz);
    EXPECT_EQ(policy.metrics().total_attempts, 4uz);  // 1 + 3

    // Reset metrics
    policy.reset_metrics();

    EXPECT_EQ(policy.metrics().total_executions, 0uz);
    EXPECT_EQ(policy.metrics().total_attempts, 0uz);
    EXPECT_EQ(policy.metrics().successful_executions, 0uz);
    EXPECT_EQ(policy.metrics().failed_executions, 0uz);
}

TEST_CASE(concurrent_retries_independent)
{
    reset_call_count();

    Core::RetryPolicy policy(3, Duration::from_milliseconds(10));

    // Execute multiple operations - they should not interfere
    auto result1 = policy.execute(succeed_immediately);
    auto result2 = policy.execute(succeed_immediately);
    auto result3 = policy.execute([&] { return succeed_after_n_attempts(1); });

    EXPECT(!result1.is_error());
    EXPECT(!result2.is_error());
    EXPECT(!result3.is_error());

    // Metrics should reflect all executions
    EXPECT_EQ(policy.metrics().total_executions, 3uz);
    EXPECT_EQ(policy.metrics().total_attempts, 4uz);  // 1 + 1 + 2
    EXPECT_EQ(policy.metrics().successful_executions, 3uz);
}

TEST_CASE(minimum_one_attempt)
{
    // Even with max_attempts=0, should still make at least 1 attempt
    Core::RetryPolicy policy(0, Duration::from_milliseconds(10));

    EXPECT_EQ(policy.max_attempts(), 1uz);  // Should be clamped to 1
}

TEST_CASE(retry_metrics_tracking)
{
    reset_call_count();

    Core::RetryPolicy policy(3, Duration::from_milliseconds(10));

    // Track multiple operations
    (void)policy.execute(succeed_immediately);           // Success on 1st attempt
    (void)policy.execute([&] { return succeed_after_n_attempts(1); });  // Success on 2nd attempt
    (void)policy.execute([&] { return succeed_after_n_attempts(2); });  // Success on 3rd attempt
    (void)policy.execute(always_fail);                   // Fail after 3 attempts

    auto& metrics = policy.metrics();
    EXPECT_EQ(metrics.total_executions, 4uz);
    EXPECT_EQ(metrics.total_attempts, 9uz);  // 1 + 2 + 3 + 3
    EXPECT_EQ(metrics.successful_executions, 3uz);
    EXPECT_EQ(metrics.failed_executions, 1uz);
    EXPECT_EQ(metrics.retried_executions, 2uz);  // 2nd and 3rd succeeded after retry
}
