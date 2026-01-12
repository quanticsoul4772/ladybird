/*
 * Web-specific threat detection rules
 * Focus on JavaScript malware and web exploits
 */

rule Malicious_JavaScript_Redirector {
    meta:
        description = "JavaScript that performs suspicious redirects"
        severity = "high"
        category = "malicious"
    strings:
        $redirect1 = /window\.location\s*=\s*['"]https?:\/\//
        $redirect2 = /document\.location\s*=\s*['"]https?:\/\//
        $obfusc = /eval\s*\(/
        $suspicious_domain = /\.ru['"]/ nocase
    condition:
        ($redirect1 or $redirect2) and ($obfusc or $suspicious_domain)
}

rule Drive_By_Download {
    meta:
        description = "JavaScript attempting drive-by download"
        severity = "critical"
        category = "malicious"
    strings:
        $download1 = "createElement('iframe')"
        $download2 = "createElement('object')"
        $hidden = /display:\s*none/
        $url = /src\s*=\s*['"]https?:\/\//
    condition:
        ($download1 or $download2) and $hidden and $url
}

rule Malicious_WebAssembly {
    meta:
        description = "Suspicious WebAssembly binary"
        severity = "high"
        category = "suspicious"
    strings:
        $wasm_magic = { 00 61 73 6D }
        $crypto = "crypto" nocase
        $miner = "miner" nocase
    condition:
        $wasm_magic at 0 and ($crypto or $miner)
}

rule XSS_Payload {
    meta:
        description = "Potential XSS attack payload"
        severity = "high"
        category = "exploit"
    strings:
        $xss1 = /<script[^>]*>.*<\/script>/s
        $xss2 = /javascript:\s*alert/
        $xss3 = /on\w+\s*=\s*['"]/
        $xss4 = /<img[^>]+onerror/
    condition:
        any of them
}

rule Credential_Stealer_JS {
    meta:
        description = "JavaScript attempting to steal credentials"
        severity = "critical"
        category = "malicious"
    strings:
        $input1 = /type\s*=\s*['"]password['"]/
        $input2 = /getElementById.*password/
        $send1 = /XMLHttpRequest/
        $send2 = /fetch\(/
        $suspicious = /btoa\(/
    condition:
        ($input1 or $input2) and ($send1 or $send2) and $suspicious
}
