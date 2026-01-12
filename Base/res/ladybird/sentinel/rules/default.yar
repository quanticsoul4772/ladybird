/*
 * Default YARA rules for Sentinel - Basic malware detection
 * These rules provide baseline protection for common threats
 */

rule EICAR_Test_File {
    meta:
        description = "EICAR anti-virus test file"
        severity = "low"
        category = "test"
    strings:
        $eicar = "X5O!P%@AP[4\\PZX54(P^)7CC)7}$EICAR-STANDARD-ANTIVIRUS-TEST-FILE!$H+H*"
    condition:
        $eicar
}

rule Suspicious_EXE_in_Archive {
    meta:
        description = "Executable file within a compressed archive"
        severity = "medium"
        category = "suspicious"
    strings:
        $zip_magic = { 50 4B 03 04 }
        $exe_mz = "MZ"
        $exe_pe = "PE"
    condition:
        $zip_magic at 0 and ($exe_mz or $exe_pe)
}

rule Obfuscated_JavaScript {
    meta:
        description = "JavaScript with suspicious obfuscation patterns"
        severity = "medium"
        category = "suspicious"
    strings:
        $js1 = /eval\s*\(\s*['"]/
        $js2 = /document\.write\s*\(\s*unescape/
        $js3 = /String\.fromCharCode/
        $js4 = /atob\s*\(/
    condition:
        2 of them
}

rule Embedded_PE_File {
    meta:
        description = "PE executable embedded in non-executable file"
        severity = "high"
        category = "malicious"
    strings:
        $mz = "MZ"
        $pe = "PE"
    condition:
        $mz at 0 and $pe in (0..1024)
}
