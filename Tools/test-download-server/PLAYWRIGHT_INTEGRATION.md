# Playwright Test Integration with Nginx Test Server

## Problem
The nginx Docker container was blocking port 8080, preventing Playwright's Node.js test server from starting. This caused all accessibility tests to fail with "404 Not Found" errors because the test fixtures weren't being served.

## Solution
The `docker-compose.yml` has been updated to mount the Playwright test fixtures directory into the nginx container. This allows the same nginx server to serve both:
1. Malware/download test files (for Sentinel testing)
2. Playwright E2E test fixtures (for accessibility, forms, etc.)

## How to Restart the Container (Windows)

Since the Docker container runs on Windows and WSL can't access Docker commands, you need to restart the container from Windows:

### Option 1: Docker Desktop GUI
1. Open Docker Desktop on Windows
2. Go to Containers tab
3. Find `sentinel-test-server` container
4. Click Stop, then Start
5. Or click Restart

### Option 2: Windows PowerShell/CMD
```powershell
# Navigate to the docker-compose directory
cd C:\Users\[YourUser]\path\to\ladybird\Tools\test-download-server

# Restart the container with the new volume mount
docker-compose down
docker-compose up -d
```

### Option 3: From WSL (if Docker Desktop WSL integration is enabled)
```bash
cd /home/rbsmith4/ladybird/Tools/test-download-server
docker-compose down
docker-compose up -d
```

## Verification

After restarting, verify the fixtures are accessible:

```bash
# Test accessibility fixture
curl http://localhost:8080/accessibility/semantic-html.html | head -20

# Test forms fixture
curl http://localhost:8080/forms/legitimate-login.html | head -20

# Test malware download (should still work)
curl http://localhost:8080/downloads/eicar.txt
```

You should see HTML content (not 404 errors).

## Alternative: Use Playwright's Built-in Server

If you prefer to use Playwright's Node.js test server instead:

1. Stop the nginx container:
   ```bash
   docker-compose down
   ```

2. The Playwright config will automatically start the Node.js test server on ports 8080/8081

3. Restart the nginx container when you need Sentinel malware testing

## File Paths

- **Docker Compose Config**: `/home/rbsmith4/ladybird/Tools/test-download-server/docker-compose.yml`
- **Nginx Config**: `/home/rbsmith4/ladybird/Tools/test-download-server/nginx.conf`
- **Playwright Fixtures**: `/home/rbsmith4/ladybird/Tests/Playwright/fixtures/`
- **Volume Mount**: Maps `Tests/Playwright/fixtures` → `/usr/share/nginx/html` (inside container)

## What Changed

In `docker-compose.yml`:
```yaml
volumes:
  # Mount Playwright test fixtures for accessibility tests
  - ../../Tests/Playwright/fixtures:/usr/share/nginx/html:ro
```

The `:ro` flag makes it read-only for security.
