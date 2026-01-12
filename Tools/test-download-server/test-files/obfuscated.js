// This file contains patterns that trigger YARA detection rules
// It's a safe test file but has suspicious patterns

var data = "aGVsbG8gd29ybGQgdGhpcyBpcyBhIHRlc3Qgc3RyaW5nIHdpdGggYmFzZTY0IGVuY29kaW5nIHRvIG1ha2UgaXQgbG9vayBzdXNwaWNpb3VzIGFuZCB0cmlnZ2VyIHRoZSBvYmZ1c2NhdGlvbiBkZXRlY3Rpb24gcnVsZXMgaW4gU2VudGluZWwgWUFSQQ==";
var decoded = atob(data);
eval("console.log('This is a test');");
document.write(unescape("%48%65%6c%6c%6f"));
