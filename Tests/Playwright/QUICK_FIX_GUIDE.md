# Quick Fix Guide - Accessibility Tests

## TL;DR

**Problem**: All 20 accessibility tests failing with "404 Not Found"

**Cause**: Nginx Docker container not serving Playwright test fixtures

**Fix**: Restart Docker container (volume mount already added)

**Time**: 30 seconds

---

## The Fix (Windows)

### Option 1: Docker Desktop
1. Open Docker Desktop
2. Find `sentinel-test-server` container
3. Click **Restart**
4. Done!

### Option 2: PowerShell
```powershell
cd path\to\ladybird\Tools\test-download-server
docker-compose down
docker-compose up -d
```

---

## Verify It Worked

```bash
curl http://localhost:8080/accessibility/semantic-html.html | head -5
```

**Should see**:
```html
<!DOCTYPE html>
<html lang="en">
<head>
    <title>Semantic HTML Test - Proper Heading Hierarchy</title>
```

**NOT**:
```html
<html>
<head><title>404 Not Found</title></head>
```

---

## Run Tests

```bash
cd /home/rbsmith4/ladybird/Tests/Playwright

# All accessibility tests
npm test -- --grep "A11Y"

# Single test
npm test -- --grep "A11Y-001"

# Expected: 40 passed (20 tests × 2 browsers)
```

---

## What Was Changed

1. ✅ `docker-compose.yml` - Added volume mount for fixtures
2. ✅ `accessibility-test-utils.ts` - Fixed Chrome DevTools API usage
3. ✅ Test fixtures - Already exist, no changes needed

---

## Full Details

See `/home/rbsmith4/ladybird/Tests/Playwright/ACCESSIBILITY_TEST_REPORT.md`
