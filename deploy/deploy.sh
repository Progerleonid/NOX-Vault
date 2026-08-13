#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

if [[ $(id -u) -ne 0 ]]; then
  echo "Run this script as root on the dedicated Ubuntu VPS." >&2
  exit 1
fi

export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y ca-certificates curl openssl ufw

if ! command -v docker >/dev/null 2>&1; then
  install -m 0755 -d /etc/apt/keyrings
  curl -fsSL https://download.docker.com/linux/ubuntu/gpg -o /etc/apt/keyrings/docker.asc
  chmod a+r /etc/apt/keyrings/docker.asc
  . /etc/os-release
  echo "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.asc] https://download.docker.com/linux/ubuntu ${VERSION_CODENAME} stable" \
    > /etc/apt/sources.list.d/docker.list
  apt-get update
  apt-get install -y docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin
fi

if [[ ! -f deploy/.env.production ]]; then
  umask 077
  cat > deploy/.env.production <<EOF
POSTGRES_PASSWORD=$(openssl rand -hex 32)
JWT_SECRET=$(openssl rand -hex 64)
ACME_EMAIL=admin@noxvault.tech
ACCESS_TOKEN_EXPIRE_MINUTES=15
MAX_REQUEST_SIZE=1048576
LOGIN_RATE_LIMIT_ATTEMPTS=5
LOGIN_RATE_LIMIT_WINDOW_SECONDS=60
EOF
fi
chmod 600 deploy/.env.production

ufw allow OpenSSH
ufw allow 80/tcp
ufw allow 443/tcp
ufw allow 443/udp
ufw --force enable

docker compose --env-file deploy/.env.production -f docker-compose.prod.yml config >/dev/null
docker compose --env-file deploy/.env.production -f docker-compose.prod.yml up -d --build
docker compose --env-file deploy/.env.production -f docker-compose.prod.yml ps

for _ in $(seq 1 30); do
  if curl --fail --silent --show-error https://api.noxvault.tech/api/v1/health; then
    echo
    echo "NOX VAULT production health check passed."
    exit 0
  fi
  sleep 5
done

echo "HTTPS health check did not pass; inspect Caddy/API logs." >&2
docker compose --env-file deploy/.env.production -f docker-compose.prod.yml logs --tail=100 caddy api >&2
exit 1
