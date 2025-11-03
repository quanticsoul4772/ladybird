/*
 * Copyright (c) 2025, Ladybird contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "FileEncryption.h"
#include <LibCore/File.h>
#include <LibCrypto/Cipher/AES.h>
#include <LibCrypto/SecureRandom.h>

namespace Sentinel::Quarantine {

ErrorOr<ByteBuffer> FileEncryption::generate_encryption_key()
{
    // Generate 256-bit (32-byte) key using cryptographically secure random
    auto key = TRY(ByteBuffer::create_uninitialized(KEY_SIZE));
    Crypto::fill_with_secure_random(key.bytes());
    return key;
}

ErrorOr<ByteBuffer> FileEncryption::generate_iv()
{
    // Generate 128-bit (16-byte) initialization vector using secure random
    auto iv = TRY(ByteBuffer::create_uninitialized(IV_SIZE));
    Crypto::fill_with_secure_random(iv.bytes());
    return iv;
}

ErrorOr<ByteBuffer> FileEncryption::encrypt_data(ReadonlyBytes data, ByteBuffer const& key)
{
    if (key.size() != KEY_SIZE)
        return Error::from_string_literal("Invalid key size (expected 32 bytes for AES-256)");

    // Generate random IV
    auto iv = TRY(generate_iv());

    // Create AES-256-CBC cipher with automatic PKCS#7 padding
    // AESCBCCipher automatically handles padding when no_padding=false (default)
    Crypto::Cipher::AESCBCCipher cipher(key.bytes());

    // Encrypt data (padding is handled automatically)
    auto ciphertext = TRY(cipher.encrypt(data, iv.bytes()));

    // Prepend IV to ciphertext: [IV][ciphertext]
    auto result = TRY(ByteBuffer::create_uninitialized(IV_SIZE + ciphertext.size()));
    memcpy(result.data(), iv.data(), IV_SIZE);
    memcpy(result.data() + IV_SIZE, ciphertext.data(), ciphertext.size());

    return result;
}

ErrorOr<ByteBuffer> FileEncryption::decrypt_data(ReadonlyBytes encrypted_data, ByteBuffer const& key)
{
    if (key.size() != KEY_SIZE)
        return Error::from_string_literal("Invalid key size (expected 32 bytes for AES-256)");

    if (encrypted_data.size() < IV_SIZE)
        return Error::from_string_literal("Encrypted data too short (missing IV)");

    // Extract IV from first 16 bytes
    ReadonlyBytes iv { encrypted_data.data(), IV_SIZE };

    // Extract ciphertext (everything after IV)
    ReadonlyBytes ciphertext { encrypted_data.data() + IV_SIZE, encrypted_data.size() - IV_SIZE };

    if (ciphertext.is_empty())
        return Error::from_string_literal("No ciphertext data after IV");

    // Create AES-256-CBC cipher with automatic PKCS#7 padding
    // AESCBCCipher automatically handles padding removal when no_padding=false (default)
    Crypto::Cipher::AESCBCCipher cipher(key.bytes());

    // Decrypt data (padding is handled automatically)
    auto plaintext = TRY(cipher.decrypt(ciphertext, iv));

    return plaintext;
}

ErrorOr<void> FileEncryption::encrypt_file(String const& input_path, String const& output_path, ByteBuffer const& key)
{
    // Read input file
    auto input_file = TRY(Core::File::open(input_path, Core::File::OpenMode::Read));
    auto file_data = TRY(input_file->read_until_eof());

    // Encrypt data
    auto encrypted_data = TRY(encrypt_data(file_data, key));

    // Write encrypted data to output file
    auto output_file = TRY(Core::File::open(output_path, Core::File::OpenMode::Write));
    TRY(output_file->write_until_depleted(encrypted_data));

    return {};
}

ErrorOr<void> FileEncryption::decrypt_file(String const& input_path, String const& output_path, ByteBuffer const& key)
{
    // Read encrypted file
    auto input_file = TRY(Core::File::open(input_path, Core::File::OpenMode::Read));
    auto encrypted_data = TRY(input_file->read_until_eof());

    // Decrypt data
    auto plaintext = TRY(decrypt_data(encrypted_data, key));

    // Write plaintext to output file
    auto output_file = TRY(Core::File::open(output_path, Core::File::OpenMode::Write));
    TRY(output_file->write_until_depleted(plaintext));

    return {};
}

}
