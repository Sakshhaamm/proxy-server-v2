#!/bin/bash
echo "--- Starting Proxy Tests ---"

echo "1. Testing Allowed Site (Example.com)..."
curl -x localhost:8888 http://example.com > output.html
if [ -s output.html ]; then
    echo "[PASS] Successfully downloaded example.com"
else
    echo "[FAIL] Could not download example.com"
fi

echo "2. Testing Blocked Site (Google)..."
# We expect a 403 Forbidden or empty response
response=$(curl -x localhost:8888 -s -o /dev/null -w "%{http_code}" http://google.com)
if [ "$response" == "200" ]; then
    echo "[FAIL] Google was NOT blocked!"
else
    echo "[PASS] Google blocked (Response code: $response)"
fi

echo "--- Tests Complete ---"