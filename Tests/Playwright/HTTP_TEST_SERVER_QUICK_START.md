# HTTP Test Server - Quick Start Guide

**TL;DR:** You now have a complete HTTP test server infrastructure for cross-origin testing. Servers auto-start/stop with Playwright tests.

## What Was Built

```
Tests/Playwright/
├── test-server.js                    🆕 Express HTTP server (auto-starts)
├── playwright.config.ts              ✏️  Updated: webServer config
├── package.json                      ✏️  Updated: dependencies + scripts
├── fixtures/
│   ├── forms/
│   │   ├── cross-origin-test.html   🆕 Cross-origin form (8080 → 8081)
│   │   ├── legitimate-login.html     ✅ Same-origin login
│   │   ├── oauth-flow.html          ✅ OAuth/SSO flow
│   │   ├── phishing-attack.html     ✅ Phishing simulation
│   │   └── data-harvesting.html     ✅ Hidden fields
│   ├── scroll-page.html             🆕 Scrolling & anchors
│   └── navigation-test.html         🆕 Navigation scenarios
├── HTTP_TEST_SERVER.md              🆕 Complete documentation
├── HTTP_TEST_SERVER_IMPLEMENTATION.md 🆕 Implementation summary
└── HTTP_TEST_SERVER_QUICK_START.md  🆕 This file
```

## Zero-Config Usage

```bash
# Just run tests - servers auto-start!
npm test

# That's it. Servers start on ports 8080 and 8081, run tests, then stop.
```

## Manual Testing

```bash
# Start server manually
npm run test-server

# Test it works
curl http://localhost:8080/health

# Stop: Ctrl+C
```

## Use in Tests

### Old Way (data: URLs)
```typescript
// Limited: No real origin, no scrolling, no cross-origin
await page.goto('data:text/html,<form>...</form>');
```

### New Way (HTTP URLs)
```typescript
// Full features: Real origin, scrolling, cross-origin testing
await page.goto('http://localhost:8080/forms/legitimate-login.html');
```

## Cross-Origin Testing

```typescript
// Form on Origin A (port 8080)
await page.goto('http://localhost:8080/forms/cross-origin-test.html');

// Form submits to Origin B (port 8081) - different origin!
await page.click('#submit-btn');

// FormMonitor detects: localhost:8080 → localhost:8081 (cross-origin!)
```

## Available Fixtures

| File | Purpose | Origin | Action |
|------|---------|--------|--------|
| `forms/legitimate-login.html` | Same-origin login (no alert) | localhost:8080 | /login (same) |
| `forms/cross-origin-test.html` | Cross-origin test | localhost:8080 | localhost:8081 (different!) |
| `forms/oauth-flow.html` | OAuth/SSO trusted flow | localhost:8080 | auth0.com |
| `scroll-page.html` | Scrolling & anchors | localhost:8080 | N/A |
| `navigation-test.html` | Navigation scenarios | localhost:8080 | Various |

## Available Endpoints

### Form Testing
- `POST /submit` - Generic form submission
- `POST /login` - Same-origin login
- `POST /harvest` - Simulated phishing

### Navigation
- `GET /redirect?status=302&url=...` - HTTP redirects
- `GET /scroll-test` - Scrollable content

### OAuth
- `GET/POST /oauth/authorize` - OAuth flow
- `GET /oauth/callback` - OAuth callback

### Utilities
- `GET /health` - Server health check
- `GET /` - Server info page

## Three Origins for Testing

1. **Origin A:** `http://localhost:8080` (Primary)
2. **Origin B:** `http://localhost:8081` (Different port = different origin)
3. **Origin C:** `http://127.0.0.1:8080` (Different host = different origin)

## Examples

### FormMonitor Cross-Origin Test
```typescript
test('cross-origin credential submission', async ({ page }) => {
  // Load form from Origin A
  await page.goto('http://localhost:8080/forms/cross-origin-test.html');

  // Form submits to Origin B (cross-origin!)
  const [response] = await Promise.all([
    page.waitForResponse(resp => resp.url().includes('8081/submit')),
    page.click('#submit-btn')
  ]);

  // Verify cross-origin submission was detected
  const data = await response.json();
  expect(data.hasPasswordField).toBe(true);
  expect(data.metadata.serverPort).toBe('8081');
});
```

### Navigation Scrolling Test
```typescript
test('anchor navigation with scrolling', async ({ page }) => {
  await page.goto('http://localhost:8080/scroll-page.html');

  // Initial scroll position
  let scrollY = await page.evaluate(() => window.scrollY);
  expect(scrollY).toBe(0);

  // Click anchor link
  await page.click('a[href="#target"]');
  await page.waitForTimeout(500);

  // Verify scrolling occurred
  scrollY = await page.evaluate(() => window.scrollY);
  expect(scrollY).toBeGreaterThan(1500);
});
```

### OAuth Flow Test
```typescript
test('oauth trusted relationship', async ({ page }) => {
  await page.goto('http://localhost:8080/forms/oauth-flow.html');

  // Verify cross-origin OAuth action
  const testData = await page.evaluate(() =>
    (window as any).__formMonitorTestData
  );

  expect(testData.formOrigin).toBe('http://localhost:8080');
  expect(testData.actionOrigin).toBe('https://auth0.com');
  expect(testData.isCrossOrigin).toBe(true);
  expect(testData.isOAuthFlow).toBe(true);
});
```

## When to Use HTTP vs data: URLs

### Use HTTP URLs when you need:
- ✅ Cross-origin testing (FormMonitor, PolicyGraph)
- ✅ Scrolling behavior (anchor links)
- ✅ HTTP headers (Origin, Referer, Cookies)
- ✅ Form submissions with server responses
- ✅ Multi-page flows (redirects, OAuth)

### Keep data: URLs for:
- ✅ Simple DOM rendering tests
- ✅ Quick inline HTML snippets
- ✅ Single-page scenarios
- ✅ Tests that don't need real origins

## Troubleshooting

### Port Already in Use
```bash
# Kill existing process
lsof -i :8080
kill -9 <PID>

# Or use different port
PORT=9080 npm run test-server
```

### Server Won't Start
```bash
# Check if dependencies installed
npm install

# Test server manually
node test-server.js

# Check logs
cat /tmp/test-server.log
```

### Tests Can't Reach Server
```bash
# Verify server is running
curl http://localhost:8080/health

# Should return:
# {"status":"ok","port":"8080","uptime":...}
```

## Next Steps

1. **Try it out:**
   ```bash
   npm test
   ```

2. **Update existing tests** to use HTTP URLs where beneficial

3. **Add new fixtures** in `fixtures/` directory as needed

4. **Read full docs:**
   - `HTTP_TEST_SERVER.md` - Complete documentation
   - `HTTP_TEST_SERVER_IMPLEMENTATION.md` - Implementation details

## Key Benefits

✅ **Real cross-origin testing** - FormMonitor can detect actual cross-origin submissions
✅ **Automatic lifecycle** - Servers start/stop with tests
✅ **Zero configuration** - Just run `npm test`
✅ **Real scrolling** - Anchor navigation works properly
✅ **HTTP features** - Headers, cookies, redirects all work
✅ **Extensible** - Easy to add new endpoints or fixtures

## Files to Reference

1. **Complete Guide:** `HTTP_TEST_SERVER.md` (620 lines)
2. **Implementation:** `HTTP_TEST_SERVER_IMPLEMENTATION.md` (500+ lines)
3. **Quick Start:** `HTTP_TEST_SERVER_QUICK_START.md` (this file)
4. **Server Code:** `test-server.js` (513 lines)
5. **Config:** `playwright.config.ts` (webServer section)

---

**Ready to use!** Just run `npm test` and the servers will automatically start. 🚀
