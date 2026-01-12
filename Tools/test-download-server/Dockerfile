FROM nginx:alpine

# Create directory for test files
RUN mkdir -p /usr/share/nginx/html/downloads

# Copy test files
COPY test-files/ /usr/share/nginx/html/downloads/

# Copy custom nginx config to force downloads
COPY nginx.conf /etc/nginx/conf.d/default.conf

# Create a styled index page
RUN cat > /usr/share/nginx/html/index.html <<'EOF'
<!DOCTYPE html>
<html>
<head>
    <title>Sentinel Download Test Server</title>
    <style>
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, Cantarell, sans-serif;
            max-width: 800px;
            margin: 50px auto;
            padding: 20px;
            background: #f5f5f5;
        }
        h1 {
            color: #2c3e50;
            border-bottom: 3px solid #3498db;
            padding-bottom: 10px;
        }
        .test-file {
            background: white;
            margin: 15px 0;
            padding: 20px;
            border-radius: 8px;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        }
        .test-file h3 {
            margin-top: 0;
            color: #34495e;
        }
        .expected {
            background: #ecf0f1;
            padding: 10px;
            border-radius: 4px;
            margin: 10px 0;
            font-size: 14px;
        }
        .malicious { border-left: 4px solid #e74c3c; }
        .suspicious { border-left: 4px solid #f39c12; }
        .safe { border-left: 4px solid #27ae60; }
        a.download-btn {
            display: inline-block;
            padding: 10px 20px;
            background: #3498db;
            color: white;
            text-decoration: none;
            border-radius: 4px;
            font-weight: bold;
        }
        a.download-btn:hover {
            background: #2980b9;
        }
        .warning {
            background: #fff3cd;
            border: 1px solid #ffc107;
            padding: 15px;
            border-radius: 4px;
            margin-bottom: 20px;
        }
    </style>
</head>
<body>
    <h1>Sentinel Download Test Server</h1>

    <div class="warning">
        <strong>Test Environment:</strong> This server hosts files designed to test Sentinel's security scanning.
        Make sure Sentinel is running before testing downloads.
    </div>

    <div class="test-file malicious">
        <h3>[MALICIOUS] EICAR Test File</h3>
        <p>Industry-standard antivirus test file (completely safe, but recognized as malware by all AV engines)</p>
        <div class="expected">
            <strong>Expected:</strong> Triggers EICAR_Test_File rule → Quarantined immediately
        </div>
        <a href="/downloads/eicar.txt" class="download-btn" download>Download EICAR.txt</a>
    </div>

    <div class="test-file suspicious">
        <h3>[SUSPICIOUS] Obfuscated JavaScript</h3>
        <p>JavaScript file with suspicious patterns (eval, atob, base64, unescape)</p>
        <div class="expected">
            <strong>Expected:</strong> Triggers Obfuscated_JavaScript rule → Warning dialog
        </div>
        <a href="/downloads/obfuscated.js" class="download-btn" download>Download obfuscated.js</a>
    </div>

    <div class="test-file safe">
        <h3>[SAFE] Safe Text File</h3>
        <p>Plain text file with no suspicious content</p>
        <div class="expected">
            <strong>Expected:</strong> Clean scan → Downloads normally
        </div>
        <a href="/downloads/safe-file.txt" class="download-btn" download>Download safe-file.txt</a>
    </div>

    <div class="test-file safe">
        <h3>[SAFE] Safe Image</h3>
        <p>Minimal PNG image (1x1 pixel)</p>
        <div class="expected">
            <strong>Expected:</strong> Clean scan → Downloads normally
        </div>
        <a href="/downloads/test-image.png" class="download-btn" download>Download test-image.png</a>
    </div>

    <hr>
    <p style="text-align: center; color: #7f8c8d;">
        Check scan results at <a href="about:security" target="_blank">about:security</a><br>
        View quarantine: <code>sentinel-cli list-quarantine</code>
    </p>
</body>
</html>
EOF

EXPOSE 80

# Run nginx
CMD ["nginx", "-g", "daemon off;"]
