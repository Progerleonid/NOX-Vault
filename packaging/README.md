# NOX Vault packaging

The install graph starts in `client/CMakeLists.txt`. Linux uses CPack's DEB
generator; Windows uses WiX v5; macOS uses `pkgbuild` and `productbuild`.

Brand assets are derived from `assets/nox-vault-icon.png`. The committed ICO,
ICNS, WiX bitmaps and macOS background are final release inputs. The original
chroma-key generation is intentionally ignored.

Do not add account registration, server configuration, or per-user data removal
to installer actions. Installers own program files and PATH integration only.

Unsigned artifacts are intentional until signing credentials are provisioned.
Never work around platform security controls in installer scripts.
