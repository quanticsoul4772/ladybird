/*
 * Archive-specific threat detection
 * Detects suspicious patterns in compressed files
 */

rule Suspicious_ZIP_with_Executable {
    meta:
        description = "ZIP archive containing Windows executable"
        severity = "medium"
        category = "suspicious"
    strings:
        $zip_sig = { 50 4B 03 04 }
        $exe_name = /.exe/ nocase
        $scr_name = /.scr/ nocase
        $bat_name = /.bat/ nocase
    condition:
        $zip_sig at 0 and any of ($exe_name, $scr_name, $bat_name)
}

rule Archive_Bomb {
    meta:
        description = "Potential ZIP bomb (high compression ratio)"
        severity = "high"
        category = "malicious"
    strings:
        $zip_sig = { 50 4B 03 04 }
    condition:
        $zip_sig at 0 and filesize < 100KB
}

rule Nested_Archives {
    meta:
        description = "Archive containing another archive (evasion technique)"
        severity = "medium"
        category = "suspicious"
    strings:
        $zip_sig = { 50 4B 03 04 }
        $rar_sig = { 52 61 72 21 }
        $gz_sig = { 1F 8B }
        $tar_gz = /.tar.gz/ nocase
        $nested = /.zip/ nocase
    condition:
        ($zip_sig or $rar_sig or $gz_sig) and ($tar_gz or $nested)
}

rule Password_Protected_Archive {
    meta:
        description = "Password-protected archive (may hide malware)"
        severity = "low"
        category = "suspicious"
    strings:
        $zip_encrypted = { 50 4B 03 04 ?? ?? 09 00 }
    condition:
        $zip_encrypted at 0
}

rule Double_Extension_Archive {
    meta:
        description = "File with double extension (e.g., invoice.pdf.exe)"
        severity = "high"
        category = "malicious"
    strings:
        $double1 = /.pdf.exe/ nocase
        $double2 = /.doc.exe/ nocase
        $double3 = /.jpg.exe/ nocase
        $double4 = /.txt.scr/ nocase
    condition:
        any of them
}
