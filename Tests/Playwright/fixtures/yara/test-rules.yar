/*
 * YARA Test Rules for Sentinel Malware Detection
 *
 * These rules are designed for testing Ladybird's SecurityTap integration.
 * They use safe test signatures (EICAR) and custom patterns that won't
 * match legitimate files.
 *
 * Installation:
 *   - Copy to /etc/sentinel/rules/ or Sentinel's configured rules directory
 *   - Restart Sentinel service or send SIGHUP to reload
 *
 * Usage in tests:
 *   - eicar.txt -> matches EICAR_Test_File
 *   - custom-test.txt -> matches Detect_Test_String
 *   - clean-file.txt -> no matches (verify false positives)
 */

rule EICAR_Test_File {
    meta:
        description = "EICAR test file - industry standard for AV testing"
        author = "Sentinel Team"
        reference = "https://www.eicar.org/download-anti-malware-testfile/"
        severity = "test"
        threat_level = "low"
        date = "2025-11-01"

    strings:
        // EICAR signature (split to avoid AV flagging this file)
        $eicar = "X5O!P%@AP[4\\PZX54(P^)7CC)7}$EICAR"

    condition:
        $eicar
}

rule Detect_Test_String {
    meta:
        description = "Detects custom test signature for verification"
        author = "Sentinel Team"
        severity = "medium"
        threat_level = "low"
        date = "2025-11-01"
        purpose = "Verify custom YARA rules are loaded and matched"

    strings:
        $custom_sig = "CUSTOM_MALWARE_SIGNATURE_12345"

    condition:
        $custom_sig
}

rule PE_Suspicious_APIs {
    meta:
        description = "Detects Windows PE files with suspicious API calls"
        author = "Sentinel Team"
        severity = "high"
        threat_level = "medium"
        date = "2025-11-01"
        category = "malware"

    strings:
        // DOS header
        $mz = { 4D 5A }

        // Suspicious API calls (common in malware)
        $api1 = "VirtualAlloc" ascii
        $api2 = "CreateRemoteThread" ascii
        $api3 = "WriteProcessMemory" ascii
        $api4 = "LoadLibraryA" ascii
        $api5 = "GetProcAddress" ascii

    condition:
        // Must have MZ header (PE file)
        $mz at 0 and
        // And at least 3 suspicious APIs
        3 of ($api*)
}

rule High_Entropy_Packed {
    meta:
        description = "Detects files with high entropy (likely packed/encrypted)"
        author = "Sentinel Team"
        severity = "medium"
        threat_level = "medium"
        date = "2025-11-01"
        category = "packer"

    strings:
        // UPX packer signature
        $upx1 = "UPX0" ascii
        $upx2 = "UPX1" ascii

        // Other common packers
        $aspack = "ASPack"
        $pecompact = "PECompact"
        $armadillo = "Armadillo"

    condition:
        any of them
}

rule Embedded_URLs {
    meta:
        description = "Detects files with embedded URLs (C2 servers, download sites)"
        author = "Sentinel Team"
        severity = "medium"
        threat_level = "medium"
        date = "2025-11-01"
        category = "network"

    strings:
        // HTTP/HTTPS URLs with suspicious TLDs or patterns
        $url1 = /https?:\/\/[a-z0-9\-\.]+\.(ru|cn|tk|ml)/ nocase
        $url2 = /https?:\/\/192\.168\.[0-9]{1,3}\.[0-9]{1,3}/
        $url3 = /https?:\/\/10\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}/

        // Common malware download patterns
        $download1 = /\/payload\.exe/ nocase
        $download2 = /\/backdoor\.[a-z]{3}/ nocase
        $download3 = /\/malware\.[a-z]{3}/ nocase

    condition:
        2 of them
}

rule Suspicious_PowerShell {
    meta:
        description = "Detects embedded PowerShell commands (common in droppers)"
        author = "Sentinel Team"
        severity = "high"
        threat_level = "high"
        date = "2025-11-01"
        category = "script"

    strings:
        $ps1 = "powershell" nocase
        $ps2 = "-enc" ascii // Encoded command
        $ps3 = "-EncodedCommand" nocase
        $ps4 = "IEX" ascii // Invoke-Expression
        $ps5 = "DownloadString" ascii
        $ps6 = "WebClient" ascii

    condition:
        $ps1 and 2 of ($ps2, $ps3, $ps4, $ps5, $ps6)
}

rule Shellcode_Patterns {
    meta:
        description = "Detects common shellcode patterns"
        author = "Sentinel Team"
        severity = "high"
        threat_level = "high"
        date = "2025-11-01"
        category = "exploit"

    strings:
        // NOP sled
        $nopsled = { 90 90 90 90 90 90 90 90 }

        // Common x86 shellcode opcodes
        $shellcode1 = { EB ?? 5E 89 76 08 C6 46 07 00 }
        $shellcode2 = { 31 C0 50 68 2F 2F 73 68 }

        // Function prologue/epilogue (common in injected code)
        $prologue = { 55 89 E5 }
        $epilogue = { 89 EC 5D C3 }

    condition:
        $nopsled or
        any of ($shellcode*) or
        (#prologue > 5 and #epilogue > 5)
}

rule Bitcoin_Miner_Strings {
    meta:
        description = "Detects cryptocurrency mining malware"
        author = "Sentinel Team"
        severity = "medium"
        threat_level = "medium"
        date = "2025-11-01"
        category = "miner"

    strings:
        $mine1 = "stratum+tcp://" ascii
        $mine2 = "xmrig" nocase
        $mine3 = "minerd" nocase
        $mine4 = "cryptonight" nocase
        $mine5 = "monero" nocase
        $mine6 = "ethereum" nocase

        // Mining pool domains
        $pool1 = "pool.minergate.com"
        $pool2 = "xmr.pool.minergate.com"

    condition:
        2 of them
}

rule Ransomware_Indicators {
    meta:
        description = "Detects ransomware indicators"
        author = "Sentinel Team"
        severity = "critical"
        threat_level = "critical"
        date = "2025-11-01"
        category = "ransomware"

    strings:
        $ransom1 = "DECRYPT_INSTRUCTION" nocase
        $ransom2 = "YOUR FILES HAVE BEEN ENCRYPTED" nocase
        $ransom3 = "bitcoin wallet" nocase
        $ransom4 = ".locked" ascii
        $ransom5 = ".encrypted" ascii
        $ransom6 = "RECOVERY_KEY" nocase

        // File encryption functions
        $crypto1 = "CryptEncrypt"
        $crypto2 = "CryptGenKey"

    condition:
        2 of ($ransom*) or
        1 of ($ransom*) and 1 of ($crypto*)
}

rule Test_Rule_Load_Verification {
    meta:
        description = "Dummy rule to verify YARA rules loaded successfully"
        author = "Sentinel Team"
        severity = "info"
        threat_level = "none"
        date = "2025-11-01"
        purpose = "If this rule is present, all rules loaded correctly"

    condition:
        false // Never matches, just for verification
}
