#!/bin/bash
# Quick start script for Sentinel download testing

set -e

echo "Sentinel Download Test Environment"
echo "==================================="
echo ""

# Check if Docker is running
if ! docker info > /dev/null 2>&1; then
    echo "[ERROR] Docker is not running"
    echo "        Please start Docker first"
    exit 1
fi

# Check if Sentinel is running
if ! [ -S /tmp/sentinel.sock ]; then
    echo "[WARNING] Sentinel daemon is not running"
    echo "          Starting Sentinel in background..."
    echo ""

    # Start Sentinel in background
    cd /home/rbsmith4/ladybird/Build/release
    ./bin/Sentinel > /tmp/sentinel.log 2>&1 &
    SENTINEL_PID=$!

    # Wait for socket to be created
    for i in {1..10}; do
        if [ -S /tmp/sentinel.sock ]; then
            echo "[OK] Sentinel started (PID: $SENTINEL_PID)"
            break
        fi
        sleep 0.5
    done

    if ! [ -S /tmp/sentinel.sock ]; then
        echo "[ERROR] Failed to start Sentinel"
        echo "        Check logs: tail /tmp/sentinel.log"
        exit 1
    fi
else
    echo "[OK] Sentinel daemon is running"
fi

echo ""

# Build and start Docker test server
cd /home/rbsmith4/ladybird/Tools/test-download-server

echo "Building Docker test server..."
docker-compose build --quiet

echo "Starting test server on http://localhost:8080"
docker-compose up -d

# Wait for server to be ready
sleep 2

if docker-compose ps | grep -q "Up"; then
    echo ""
    echo "[OK] Test environment ready!"
    echo ""
    echo "Next steps:"
    echo "   1. Open Ladybird browser:"
    echo "      /home/rbsmith4/ladybird/Build/release/bin/Ladybird"
    echo ""
    echo "   2. Navigate to test page:"
    echo "      http://localhost:8080"
    echo ""
    echo "   3. Try downloading the EICAR test file (should be quarantined)"
    echo ""
    echo "   4. Check results:"
    echo "      • Browser: about:security"
    echo "      • CLI: /home/rbsmith4/ladybird/Build/release/bin/sentinel-cli list-quarantine"
    echo ""
    echo "   5. Stop test server when done:"
    echo "      docker-compose down"
    echo ""
else
    echo "[ERROR] Failed to start test server"
    docker-compose logs
    exit 1
fi
