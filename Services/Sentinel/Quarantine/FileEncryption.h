/*
 * Copyright (c) 2025, Ladybird contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/ByteBuffer.h>
#include <AK/Error.h>
#include <AK/String.h>

namespace Sentinel::Quarantine {

// File encryption utilities using AES-256-CBC
// Used to encrypt quarantined files to prevent accidental execution
// Uses LibCrypto's AESCBCCipher with automatic PKCS#7 padding
class FileEncryption {
public:
    // Generate a cryptographically secure 256-bit encryption key
    // Key should be stored securely (e.g., in system keyring or encrypted config)
    static ErrorOr<ByteBuffer> generate_encryption_key();

    // Encrypt a file using AES-256-CBC
    // Input: plaintext file path
    // Output: encrypted file path with IV prepended to ciphertext
    // Format: [16-byte IV][encrypted data with PKCS#7 padding]
    static ErrorOr<void> encrypt_file(
        String const& input_path,
        String const& output_path,
        ByteBuffer const& key);

    // Decrypt a file using AES-256-CBC
    // Input: encrypted file path (with IV prepended)
    // Output: plaintext file path
    static ErrorOr<void> decrypt_file(
        String const& input_path,
        String const& output_path,
        ByteBuffer const& key);

    // Encrypt data in memory (for smaller files)
    // Returns: [16-byte IV][encrypted data with PKCS#7 padding]
    static ErrorOr<ByteBuffer> encrypt_data(
        ReadonlyBytes data,
        ByteBuffer const& key);

    // Decrypt data in memory
    // Input: [16-byte IV][encrypted data with PKCS#7 padding]
    // Returns: plaintext data
    static ErrorOr<ByteBuffer> decrypt_data(
        ReadonlyBytes encrypted_data,
        ByteBuffer const& key);

private:
    // AES-256 requires 32-byte key, 16-byte IV
    static constexpr size_t KEY_SIZE = 32;
    static constexpr size_t IV_SIZE = 16;

    // Generate random IV for CBC mode
    static ErrorOr<ByteBuffer> generate_iv();
};

}
