#!/usr/bin/env python3
"""
Generate a signed JWT token for Metabase Static Embedding.

Usage:
    python generate-embed-token.py <secret> <dashboard_id> [ttl_days]

Arguments:
    secret        Value of METABASE_EMBEDDING_SECRET from .env
    dashboard_id  Numeric ID of the Metabase dashboard (visible in its URL)
    ttl_days      Token lifetime in days (default: 365)

Output:
    Signed JWT string — paste it into nginx/html/embed.html as the iframe src token.

Example:
    python generate-embed-token.py abc123secret 1 365
"""

import sys
import time

try:
    import jwt
except ImportError:
    print("Error: PyJWT not installed. Run: pip install PyJWT", file=sys.stderr)
    sys.exit(1)

if len(sys.argv) < 3:
    print(__doc__)
    sys.exit(1)

secret = sys.argv[1]
dashboard_id = int(sys.argv[2])
ttl_days = int(sys.argv[3]) if len(sys.argv) > 3 else 365

payload = {
    "resource": {"dashboard": dashboard_id},
    "params": {},
    "exp": int(time.time()) + ttl_days * 86400,
}

token = jwt.encode(payload, secret, algorithm="HS256")
print(token)
