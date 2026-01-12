#!/usr/bin/env node

/**
 * HTTP Test Server for Ladybird Playwright Tests
 *
 * Provides local HTTP server for:
 * - Cross-origin testing (FormMonitor, PolicyGraph)
 * - Real HTTP headers and cookies
 * - Form submission endpoints
 * - OAuth flow simulation
 * - Scrolling support (not available with data: URLs)
 *
 * Usage:
 *   node test-server.js                    # Default port 8080
 *   PORT=8081 node test-server.js          # Custom port
 *   npm run test-server                    # Via package.json script
 *
 * Architecture:
 * - Primary server: http://localhost:8080  (Origin A)
 * - Secondary server: http://localhost:8081 (Origin B for cross-origin tests)
 * - Third origin: http://127.0.0.1:8080   (Different host)
 */

const express = require('express');
const path = require('path');
const cors = require('cors');
const fs = require('fs');
const https = require('https');
const http = require('http');

const app = express();
const PORT = process.env.PORT || 8080;
const USE_HTTPS = process.env.USE_HTTPS === 'true';

// HTTPS configuration
let httpsOptions = null;
if (USE_HTTPS) {
  const certPath = path.join(__dirname, 'certs', 'cert.pem');
  const keyPath = path.join(__dirname, 'certs', 'key.pem');

  if (fs.existsSync(certPath) && fs.existsSync(keyPath)) {
    httpsOptions = {
      key: fs.readFileSync(keyPath),
      cert: fs.readFileSync(certPath)
    };
    console.log('HTTPS enabled with self-signed certificates');
  } else {
    console.error('HTTPS requested but certificates not found!');
    console.error('Run: openssl req -x509 -newkey rsa:2048 -keyout certs/key.pem -out certs/cert.pem -days 365 -nodes');
    process.exit(1);
  }
}

// ============================================================================
// Middleware Configuration
// ============================================================================

// Serve static files from fixtures directory
app.use(express.static(path.join(__dirname, 'fixtures'), {
  // Enable CORS for cross-origin testing
  setHeaders: (res, filepath) => {
    // Add security headers
    res.setHeader('X-Content-Type-Options', 'nosniff');
    res.setHeader('X-Frame-Options', 'SAMEORIGIN');

    // Allow cross-origin for test scenarios
    res.setHeader('Access-Control-Allow-Origin', '*');
    res.setHeader('Access-Control-Allow-Methods', 'GET, POST, OPTIONS');
    res.setHeader('Access-Control-Allow-Headers', 'Content-Type');
  }
}));

// Serve static files from public directory (network-monitoring tests)
app.use(express.static(path.join(__dirname, 'public'), {
  setHeaders: (res, filepath) => {
    res.setHeader('X-Content-Type-Options', 'nosniff');
    res.setHeader('X-Frame-Options', 'SAMEORIGIN');
    res.setHeader('Access-Control-Allow-Origin', '*');
    res.setHeader('Access-Control-Allow-Methods', 'GET, POST, OPTIONS');
    res.setHeader('Access-Control-Allow-Headers', 'Content-Type');
  }
}));

// Enable CORS for API endpoints
app.use(cors({
  origin: '*',
  credentials: true
}));

// Parse form data (application/x-www-form-urlencoded)
app.use(express.urlencoded({ extended: true }));

// Parse JSON (application/json)
app.use(express.json());

// Logging middleware
app.use((req, res, next) => {
  const timestamp = new Date().toISOString();
  console.log(`[${timestamp}] ${req.method} ${req.url} - Origin: ${req.get('origin') || 'none'}`);
  next();
});

// ============================================================================
// Form Submission Endpoints (FormMonitor Testing)
// ============================================================================

/**
 * POST /submit
 * Generic form submission endpoint for testing FormMonitor
 * Returns submitted data and request metadata
 */
app.post('/submit', (req, res) => {
  const submissionData = {
    received: req.body,
    metadata: {
      origin: req.get('origin') || null,
      referer: req.get('referer') || null,
      userAgent: req.get('user-agent') || null,
      contentType: req.get('content-type') || null,
      timestamp: new Date().toISOString(),
      serverPort: PORT
    },
    // Check for password/email fields (FormMonitor detection)
    hasPasswordField: !!(req.body.password || req.body.old_password || req.body.new_password),
    hasEmailField: !!(req.body.email || req.body.username)
  };

  console.log('Form submission received:', {
    hasPassword: submissionData.hasPasswordField,
    hasEmail: submissionData.hasEmailField,
    origin: submissionData.metadata.origin,
    fieldCount: Object.keys(req.body).length
  });

  res.json(submissionData);
});

/**
 * POST /login
 * Simulates same-origin login endpoint
 */
app.post('/login', (req, res) => {
  const { email, password, username } = req.body;

  // Simulate authentication
  if ((email || username) && password) {
    res.json({
      success: true,
      message: 'Login successful',
      user: {
        email: email || username,
        token: 'mock-auth-token-' + Date.now()
      },
      timestamp: new Date().toISOString()
    });
  } else {
    res.status(400).json({
      success: false,
      message: 'Missing email/username or password',
      timestamp: new Date().toISOString()
    });
  }
});

/**
 * POST /harvest
 * Simulates malicious credential harvesting endpoint
 * (For testing cross-origin detection)
 */
app.post('/harvest', (req, res) => {
  const origin = req.get('origin');
  const referer = req.get('referer');

  console.warn('⚠️  SIMULATED PHISHING ENDPOINT HIT:', {
    origin,
    referer,
    body: req.body
  });

  // Return success to allow test to verify submission was detected
  res.json({
    harvested: true,
    origin: origin,
    data: req.body,
    warning: 'This is a test endpoint simulating credential harvesting',
    timestamp: new Date().toISOString()
  });
});

// ============================================================================
// OAuth Flow Endpoints (Trusted Relationship Testing)
// ============================================================================

/**
 * GET /oauth/authorize
 * Simulates OAuth authorization endpoint (e.g., Auth0, Google)
 */
app.get('/oauth/authorize', (req, res) => {
  const { client_id, redirect_uri, response_type, scope } = req.query;

  res.send(`
    <!DOCTYPE html>
    <html>
    <head>
      <title>OAuth Authorization</title>
      <style>
        body { font-family: Arial; max-width: 500px; margin: 50px auto; padding: 20px; }
        .oauth-box { border: 2px solid #4285f4; padding: 20px; border-radius: 5px; }
        button { padding: 10px 20px; background: #4285f4; color: white; border: none; border-radius: 3px; cursor: pointer; }
      </style>
    </head>
    <body>
      <div class="oauth-box">
        <h1>Authorize Application</h1>
        <p><strong>Client ID:</strong> ${client_id || 'unknown'}</p>
        <p><strong>Redirect URI:</strong> ${redirect_uri || 'unknown'}</p>
        <p><strong>Scope:</strong> ${scope || 'unknown'}</p>
        <button onclick="authorize()">Authorize</button>
      </div>
      <script>
        function authorize() {
          const redirectUri = '${redirect_uri}';
          if (redirectUri && redirectUri !== 'unknown') {
            window.location.href = redirectUri + '?code=mock-auth-code-' + Date.now();
          } else {
            alert('Authorization successful (no redirect URI)');
          }
        }
      </script>
    </body>
    </html>
  `);
});

/**
 * POST /oauth/authorize
 * Handles POST-based OAuth authorization
 */
app.post('/oauth/authorize', (req, res) => {
  const { client_id, redirect_uri, email, password } = req.body;

  console.log('OAuth POST authorization:', { client_id, redirect_uri, hasCredentials: !!(email && password) });

  res.json({
    authorized: true,
    client_id,
    redirect_uri,
    code: 'mock-oauth-code-' + Date.now(),
    timestamp: new Date().toISOString()
  });
});

/**
 * GET /oauth/callback
 * OAuth callback endpoint
 */
app.get('/oauth/callback', (req, res) => {
  const { code } = req.query;

  res.send(`
    <!DOCTYPE html>
    <html>
    <head><title>OAuth Callback</title></head>
    <body>
      <h1>Authorization Successful</h1>
      <p>Authorization code: <code>${code}</code></p>
      <p>You can close this window.</p>
      <script>
        window.__oauthTestData = {
          code: '${code}',
          timestamp: Date.now()
        };
      </script>
    </body>
    </html>
  `);
});

// ============================================================================
// Navigation Test Endpoints
// ============================================================================

/**
 * GET /redirect
 * HTTP redirect testing (301, 302, 307, 308)
 */
app.get('/redirect', (req, res) => {
  const { status, url } = req.query;
  const statusCode = parseInt(status) || 302;
  const redirectUrl = url || 'http://localhost:' + PORT + '/redirect-target';

  console.log(`Redirecting with ${statusCode} to ${redirectUrl}`);
  res.redirect(statusCode, redirectUrl);
});

/**
 * GET /redirect-target
 * Target page for redirect tests
 */
app.get('/redirect-target', (req, res) => {
  res.send(`
    <!DOCTYPE html>
    <html>
    <head><title>Redirect Target</title></head>
    <body>
      <h1>Redirect Successful</h1>
      <p>You have been redirected to this page.</p>
      <div id="redirect-info">Target reached</div>
    </body>
    </html>
  `);
});

/**
 * GET /slow-page
 * Slow-loading page for timeout testing
 */
app.get('/slow-page', (req, res) => {
  const delay = parseInt(req.query.delay) || 3000;

  setTimeout(() => {
    res.send(`
      <!DOCTYPE html>
      <html>
      <head><title>Slow Page</title></head>
      <body>
        <h1>Page Loaded After ${delay}ms</h1>
        <p>This page deliberately loaded slowly for testing.</p>
      </body>
      </html>
    `);
  }, delay);
});

// ============================================================================
// Cookie and Session Testing
// ============================================================================

/**
 * GET /set-cookie
 * Sets test cookies
 */
app.get('/set-cookie', (req, res) => {
  res.cookie('test-cookie', 'test-value', { httpOnly: false });
  res.cookie('secure-cookie', 'secure-value', { httpOnly: true });

  res.send(`
    <!DOCTYPE html>
    <html>
    <head><title>Cookie Test</title></head>
    <body>
      <h1>Cookies Set</h1>
      <p>Test cookies have been set.</p>
      <div id="cookie-status">Cookies: <span id="cookie-value"></span></div>
      <script>
        document.getElementById('cookie-value').textContent = document.cookie;
        window.__cookieTestData = {
          cookies: document.cookie,
          timestamp: Date.now()
        };
      </script>
    </body>
    </html>
  `);
});

/**
 * GET /read-cookie
 * Reads and displays cookies
 */
app.get('/read-cookie', (req, res) => {
  res.send(`
    <!DOCTYPE html>
    <html>
    <head><title>Cookie Read</title></head>
    <body>
      <h1>Cookie Values</h1>
      <p>Server cookies: ${JSON.stringify(req.cookies)}</p>
      <p>Client cookies: <span id="client-cookies"></span></p>
      <script>
        document.getElementById('client-cookies').textContent = document.cookie;
        window.__cookieTestData = {
          serverCookies: ${JSON.stringify(req.cookies)},
          clientCookies: document.cookie,
          timestamp: Date.now()
        };
      </script>
    </body>
    </html>
  `);
});

// ============================================================================
// Scrolling and Layout Testing
// ============================================================================

/**
 * GET /scroll-test
 * Page with scrollable content (not possible with data: URLs)
 */
app.get('/scroll-test', (req, res) => {
  res.send(`
    <!DOCTYPE html>
    <html>
    <head>
      <title>Scroll Test</title>
      <style>
        body { margin: 0; padding: 20px; }
        .section { height: 800px; border: 2px solid #ccc; margin: 20px 0; padding: 20px; }
        #target { background: #ffe0e0; }
      </style>
    </head>
    <body>
      <h1>Scroll Test Page</h1>
      <a href="#target">Jump to Target</a>
      <div class="section" id="section1">Section 1</div>
      <div class="section" id="section2">Section 2</div>
      <div class="section" id="target">Target Section (should scroll here)</div>
      <div class="section" id="section4">Section 4</div>
      <script>
        window.__scrollTestData = {
          initialScrollY: window.scrollY,
          pageHeight: document.body.scrollHeight,
          viewportHeight: window.innerHeight
        };
      </script>
    </body>
    </html>
  `);
});

// ============================================================================
// Malware Detection Testing (SecurityTap/Sentinel)
// ============================================================================

/**
 * GET /malware/:filename
 * Serves malware test files with appropriate Content-Disposition headers
 * Triggers download in browser -> SecurityTap scans -> Sentinel detects
 */
app.get('/malware/:filename', (req, res) => {
  const { filename } = req.params;
  const filepath = path.join(__dirname, 'fixtures', 'malware', filename);

  console.log(`Malware test download requested: ${filename}`);

  // Check if file exists
  if (!require('fs').existsSync(filepath)) {
    return res.status(404).send({
      error: 'Test file not found',
      filename,
      available: require('fs').readdirSync(path.join(__dirname, 'fixtures', 'malware'))
    });
  }

  // Set Content-Disposition to trigger download
  res.setHeader('Content-Disposition', `attachment; filename="${filename}"`);

  // Set appropriate Content-Type
  if (filename.endsWith('.txt')) {
    res.setHeader('Content-Type', 'text/plain');
  } else if (filename.endsWith('.bin') || filename.endsWith('.exe')) {
    res.setHeader('Content-Type', 'application/octet-stream');
  } else {
    res.setHeader('Content-Type', 'application/octet-stream');
  }

  // Log download metadata for test verification
  console.log(`  File: ${filename}`);
  console.log(`  Origin: ${req.get('origin') || 'none'}`);
  console.log(`  User-Agent: ${req.get('user-agent') || 'unknown'}`);

  // Serve the file
  res.sendFile(filepath);
});

/**
 * GET /malware-info
 * Returns information about available malware test files
 */
app.get('/malware-info', (req, res) => {
  const malwareDir = path.join(__dirname, 'fixtures', 'malware');
  const files = require('fs').readdirSync(malwareDir);

  const fileInfo = files.map(filename => {
    const filepath = path.join(malwareDir, filename);
    const stats = require('fs').statSync(filepath);

    return {
      filename,
      size: stats.size,
      url: `http://localhost:${PORT}/malware/${filename}`,
      expectedThreat: filename.includes('eicar') || filename.includes('suspicious')
    };
  });

  res.json({
    files: fileInfo,
    count: files.length,
    directory: malwareDir
  });
});

// ============================================================================
// Health Check and Info
// ============================================================================

/**
 * GET /health
 * Health check endpoint
 */
app.get('/health', (req, res) => {
  res.json({
    status: 'ok',
    port: PORT,
    uptime: process.uptime(),
    timestamp: new Date().toISOString()
  });
});

/**
 * GET /
 * Root endpoint with server info
 */
app.get('/', (req, res) => {
  res.send(`
    <!DOCTYPE html>
    <html>
    <head>
      <title>Ladybird Test Server</title>
      <style>
        body { font-family: Arial; max-width: 800px; margin: 50px auto; padding: 20px; }
        h1 { color: #333; }
        .endpoint { background: #f5f5f5; padding: 10px; margin: 10px 0; border-left: 3px solid #007bff; }
        code { background: #e0e0e0; padding: 2px 5px; border-radius: 3px; }
      </style>
    </head>
    <body>
      <h1>Ladybird Test Server</h1>
      <p>HTTP test server for Playwright E2E tests</p>
      <p><strong>Port:</strong> ${PORT}</p>
      <p><strong>Origin:</strong> http://localhost:${PORT}</p>

      <h2>Available Endpoints</h2>

      <h3>Form Testing</h3>
      <div class="endpoint"><code>POST /submit</code> - Generic form submission</div>
      <div class="endpoint"><code>POST /login</code> - Same-origin login</div>
      <div class="endpoint"><code>POST /harvest</code> - Simulated phishing endpoint</div>

      <h3>OAuth Flow</h3>
      <div class="endpoint"><code>GET /oauth/authorize</code> - OAuth authorization</div>
      <div class="endpoint"><code>POST /oauth/authorize</code> - OAuth POST flow</div>
      <div class="endpoint"><code>GET /oauth/callback</code> - OAuth callback</div>

      <h3>Navigation</h3>
      <div class="endpoint"><code>GET /redirect?status=302&url=...</code> - HTTP redirects</div>
      <div class="endpoint"><code>GET /slow-page?delay=3000</code> - Slow loading page</div>
      <div class="endpoint"><code>GET /scroll-test</code> - Scrollable content</div>

      <h3>Cookies</h3>
      <div class="endpoint"><code>GET /set-cookie</code> - Set test cookies</div>
      <div class="endpoint"><code>GET /read-cookie</code> - Read cookies</div>

      <h3>Static Fixtures</h3>
      <div class="endpoint"><code>GET /forms/legitimate-login.html</code> - Same-origin login form</div>
      <div class="endpoint"><code>GET /forms/oauth-flow.html</code> - OAuth flow test</div>
      <div class="endpoint"><code>GET /forms/phishing-attack.html</code> - Phishing simulation</div>
      <div class="endpoint"><code>GET /forms/data-harvesting.html</code> - Data harvesting detection</div>

      <h3>Malware Detection Testing</h3>
      <div class="endpoint"><code>GET /malware/eicar.txt</code> - EICAR test file (safe AV test)</div>
      <div class="endpoint"><code>GET /malware/clean-file.txt</code> - Clean file (no threat)</div>
      <div class="endpoint"><code>GET /malware/ml-suspicious.bin</code> - ML detection test</div>
      <div class="endpoint"><code>GET /malware/custom-test.txt</code> - Custom YARA rule test</div>
      <div class="endpoint"><code>GET /malware-info</code> - List all malware test files</div>

      <h3>Utilities</h3>
      <div class="endpoint"><code>GET /health</code> - Health check</div>

      <h2>Cross-Origin Testing</h2>
      <p>For cross-origin tests, run multiple instances:</p>
      <ul>
        <li>Origin A: <code>PORT=8080 node test-server.js</code> → http://localhost:8080</li>
        <li>Origin B: <code>PORT=8081 node test-server.js</code> → http://localhost:8081</li>
        <li>Origin C: http://127.0.0.1:8080 (different host, same port)</li>
      </ul>
    </body>
    </html>
  `);
});

// ============================================================================
// Error Handling
// ============================================================================

// 404 handler
app.use((req, res) => {
  res.status(404).send(`
    <!DOCTYPE html>
    <html>
    <head><title>404 Not Found</title></head>
    <body>
      <h1>404 - Not Found</h1>
      <p>The requested URL <code>${req.url}</code> was not found on this server.</p>
      <p><a href="/">Back to home</a></p>
    </body>
    </html>
  `);
});

// Error handler
app.use((err, req, res, next) => {
  console.error('Server error:', err);
  res.status(500).json({
    error: 'Internal server error',
    message: err.message,
    timestamp: new Date().toISOString()
  });
});

// ============================================================================
// Server Startup
// ============================================================================

// Create HTTP or HTTPS server based on configuration
const server = USE_HTTPS
  ? https.createServer(httpsOptions, app)
  : http.createServer(app);

server.listen(PORT, () => {
  const protocol = USE_HTTPS ? 'https' : 'http';
  console.log('='.repeat(70));
  console.log(`Ladybird Test Server ${USE_HTTPS ? '(HTTPS)' : '(HTTP)'}`);
  console.log('='.repeat(70));
  console.log(`Port:        ${PORT}`);
  console.log(`URL:         ${protocol}://localhost:${PORT}`);
  console.log(`Alternative: ${protocol}://127.0.0.1:${PORT}`);
  console.log(`Fixtures:    ${path.join(__dirname, 'fixtures')}`);
  console.log(`Public:      ${path.join(__dirname, 'public')}`);
  console.log(`Health:      ${protocol}://localhost:${PORT}/health`);
  if (USE_HTTPS) {
    console.log(`Note:        Self-signed certificate - browsers will show security warning`);
  }
  console.log('='.repeat(70));
  console.log('Ready for Playwright tests');
});

// Graceful shutdown
process.on('SIGTERM', () => {
  console.log('SIGTERM received, shutting down gracefully...');
  server.close(() => {
    console.log('Server closed');
    process.exit(0);
  });
});

process.on('SIGINT', () => {
  console.log('\nSIGINT received, shutting down gracefully...');
  server.close(() => {
    console.log('Server closed');
    process.exit(0);
  });
});
