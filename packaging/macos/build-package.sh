#!/bin/bash
set -euo pipefail

if [[ $# -ne 4 ]]; then
  echo "usage: $0 VERSION INSTALL_ROOT ASSETS_DIR OUTPUT_PKG" >&2
  exit 2
fi

version="$1"
install_root="$(cd "$2" && pwd)"
assets_dir="$(cd "$3" && pwd)"
output_pkg="$4"
script_dir="$(cd "$(dirname "$0")" && pwd)"
work_dir="$(mktemp -d)"
trap 'rm -rf "$work_dir"' EXIT

[[ "$(uname -m)" == "arm64" ]] || { echo "macOS package must be built natively on arm64" >&2; exit 1; }
file "$install_root/bin/nox" | grep -q "arm64" || { echo "nox is not an arm64 binary" >&2; exit 1; }

mkdir -p "$work_dir/root/usr/local/bin" "$work_dir/root/usr/local/share/nox-vault" "$work_dir/resources"
install -m 0755 "$install_root/bin/nox" "$work_dir/root/usr/local/bin/nox"
install -m 0755 "$install_root/share/nox-vault/uninstall-nox-vault" "$work_dir/root/usr/local/share/nox-vault/uninstall-nox-vault"
install -m 0644 "$install_root/share/nox-vault/LICENSE" "$work_dir/root/usr/local/share/nox-vault/LICENSE"
install -m 0644 "$install_root/share/nox-vault/README.md" "$work_dir/root/usr/local/share/nox-vault/README.md"
cp "$script_dir/welcome.html" "$script_dir/conclusion.html" "$assets_dir/macos-background.png" "$work_dir/resources/"

pkgbuild --root "$work_dir/root" --identifier tech.noxvault.cli --version "$version" \
  --install-location / "$work_dir/nox-vault-component.pkg"

cat > "$work_dir/Distribution.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
  <title>NOX Vault</title>
  <organization>tech.noxvault</organization>
  <domains enable_localSystem="true" enable_currentUserHome="false" enable_anywhere="false"/>
  <options customize="never" require-scripts="false" hostArchitectures="arm64"/>
  <background file="macos-background.png" mime-type="image/png" alignment="center" scaling="proportional"/>
  <welcome file="welcome.html" mime-type="text/html"/>
  <license file="LICENSE" mime-type="text/plain"/>
  <conclusion file="conclusion.html" mime-type="text/html"/>
  <choices-outline><line choice="default"><line choice="cli"/></line></choices-outline>
  <choice id="default"/>
  <choice id="cli" visible="false"><pkg-ref id="tech.noxvault.cli"/></choice>
  <pkg-ref id="tech.noxvault.cli" version="$version" onConclusion="none">nox-vault-component.pkg</pkg-ref>
</installer-gui-script>
EOF
cp "$install_root/share/nox-vault/LICENSE" "$work_dir/resources/LICENSE"
productbuild --distribution "$work_dir/Distribution.xml" --resources "$work_dir/resources" \
  --package-path "$work_dir" "$output_pkg"
