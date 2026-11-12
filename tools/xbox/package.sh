#!/usr/bin/env bash
# Builds the Xbox Dev Mode package: one signed .msix (x64, for Xbox One and Xbox Series X|S) holding the UWP build of
# SSARecomp.exe and the player's disc image, stored uncompressed. Non-interactive (the launcher's "Export for Xbox").
#
#   tools/xbox/package.sh --exe <SSARecomp.exe> --iso <disc image> --out <package.msix>
#                         [--cert-dir <dir>] [--version <a.b.c.d>]
#   tools/xbox/package.sh --exe <SSARecomp.exe> --no-iso --out <package.msix>   (a small test package, no game)
#
# --exe       the UWP build (cmake/toolchains/uwp-x64-llvm-mingw.cmake), not the desktop one.
# --iso       copied into the package as game.iso (hard link or clone in the staging folder when possible).
# --cert-dir  where the signing key lives. The first run creates a self-signed code-signing certificate there whose
#             subject is the package Publisher, and exports it as SSARecomp.cer (to trust it on a Windows PC); later
#             runs reuse it, so new packages install over the old one. Default: ~/Library/Application Support/SSARecomp/xbox-signing
#             (macOS), ${XDG_CONFIG_HOME:-~/.config}/ssarecomp/xbox-signing elsewhere.
# --version   default 1.0.<days since 2020-01-01>.<minute of the day> (UTC): every build is newer than the last.
#
# Tools: $MAKEMSIX and $OSSLSIGNCODE, else private/xbox/bin (tools/xbox/setup_tools.sh); $OPENSSL, else openssl.
# Disk: the output volume needs about twice the disc image's size while the unsigned and the signed package both
# exist; the unsigned one is deleted at the end.
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../.." && pwd)"

PACKAGE_NAME="${SKY_PACKAGE_NAME:-${SSA_PACKAGE_NAME:-SSARecomp.SpyrosAdventure}}" # another name installs next to the game
DISPLAY_NAME="Spyro's Adventure (SSARecomp)"
SKY_DISPLAY_NAME="${SKY_DISPLAY_NAME:-${SSA_DISPLAY_NAME:-}}" # the older SSA_* names still work
[[ -z "$SKY_DISPLAY_NAME" ]] || DISPLAY_NAME="$SKY_DISPLAY_NAME"
PUBLISHER_DISPLAY_NAME="SSARecomp"
DESCRIPTION="Skylanders: Spyro's Adventure, recompiled from the player's own disc"
CERT_SUBJECT="CN=SSARecomp Local"

die() { echo "error: $*" >&2; exit 1; }
usage() { awk 'NR > 1 && /^#/ { sub(/^# ?/, ""); print; next } NR > 1 { exit }' "$0" >&2; exit 2; }

exe="" iso="" out="" cert_dir="" version=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        --exe) exe="${2:-}"; shift 2 ;;
        --iso) iso="${2:-}"; shift 2 ;;
        --no-iso) iso=none; shift ;;
        --out) out="${2:-}"; shift 2 ;;
        --cert-dir) cert_dir="${2:-}"; shift 2 ;;
        --version) version="${2:-}"; shift 2 ;;
        -h|--help) usage ;;
        *) echo "error: unknown option $1" >&2; usage ;;
    esac
done
[[ -n "$exe" && -n "$iso" && -n "$out" ]] || usage
if [[ -z "$cert_dir" ]]; then
    if [[ "$(uname -s)" == Darwin ]]; then
        cert_dir="$HOME/Library/Application Support/SSARecomp/xbox-signing"
    else
        cert_dir="${XDG_CONFIG_HOME:-$HOME/.config}/ssarecomp/xbox-signing"
    fi
fi
if [[ -z "$version" ]]; then
    now=$(date -u +%s)
    version="1.0.$(( (now - 1577836800) / 86400 )).$(( now % 86400 / 60 ))"
fi
[[ "$version" =~ ^[0-9]{1,5}\.[0-9]{1,5}\.[0-9]{1,5}\.[0-9]{1,5}$ ]] || die "version must be a.b.c.d (0-65535 each)"

makemsix="${MAKEMSIX:-$root/private/xbox/bin/makemsix}"
osslsigncode="${OSSLSIGNCODE:-$root/private/xbox/bin/osslsigncode}"
openssl="${OPENSSL:-openssl}"
[[ -x "$makemsix" ]] || die "makemsix not found at $makemsix (run tools/xbox/setup_tools.sh or set MAKEMSIX)"
[[ -x "$osslsigncode" ]] || die "osslsigncode not found at $osslsigncode (run tools/xbox/setup_tools.sh or set OSSLSIGNCODE)"
command -v "$openssl" >/dev/null || die "openssl not found (set OPENSSL)"

file_size() { stat -f %z "$1" 2>/dev/null || stat -c %s "$1"; }
read_bytes() { od -A n -t x1 -j "$2" -N "$3" "$1" 2>/dev/null | tr -d ' \n'; }

# --- Inputs ---------------------------------------------------------------------------------------------------------
[[ -f "$exe" ]] || die "$exe not found"
[[ "$(read_bytes "$exe" 0 2)" == 4d5a ]] || die "$exe is not a Windows executable"
if [[ "$iso" != none ]]; then
[[ -f "$iso" ]] || die "$iso not found"
# XDVDFS volume descriptor ("MICROSOFT*XBOX*MEDIA") at sector 32 of the game partition, as runtime/vfs.cpp reads it.
magic=4d4943524f534f46542a58424f582a4d45444941
found=0
for partition in 0 $((0x2080000)) $((0xFD90000)) $((0x18300000)); do
    if [[ "$(read_bytes "$iso" $((partition + 32 * 2048)) 20)" == "$magic" ]]; then
        found=1
        break
    fi
done
[[ $found == 1 ]] || die "$iso is not an Xbox 360 disc image"
fi

out_dir="$(cd "$(dirname "$out")" && pwd)" || die "output folder of $out does not exist"
out="$out_dir/$(basename "$out")"
iso_size=0
[[ "$iso" == none ]] || iso_size=$(file_size "$iso")
exe_size=$(file_size "$exe")
needed_kb=$(( (2 * (iso_size + exe_size) + (100 << 20)) / 1024 ))
available_kb=$(df -Pk "$out_dir" | awk 'NR == 2 { print $4 }')
(( available_kb >= needed_kb )) ||
    die "$out_dir has $((available_kb >> 20)) GB free, packaging needs $(( (needed_kb >> 20) + 1 )) GB (see tools/xbox/README.md)"

# --- Signing certificate ------------------------------------------------------------------------------------------
mkdir -p "$cert_dir"
key="$cert_dir/key.pem"
cert="$cert_dir/cert.pem"
if [[ ! -f "$key" || ! -f "$cert" ]]; then
    echo "Creating the signing certificate in $cert_dir"
    config="$(mktemp)"
    cat >"$config" <<EOF
[req]
distinguished_name = subject
prompt = no
x509_extensions = codesigning
[subject]
CN = ${CERT_SUBJECT#CN=}
[codesigning]
basicConstraints = critical, CA:FALSE
keyUsage = critical, digitalSignature
extendedKeyUsage = codeSigning
subjectKeyIdentifier = hash
EOF
    (umask 077 && "$openssl" req -x509 -newkey rsa:3072 -sha256 -days 3650 -nodes -config "$config" \
        -keyout "$key" -out "$cert" 2>/dev/null) || { rm -f "$config"; die "could not create the certificate"; }
    rm -f "$config"
fi
"$openssl" x509 -in "$cert" -outform DER -out "$cert_dir/SSARecomp.cer"
# The manifest's Publisher must be the certificate's subject, exactly.
publisher="$("$openssl" x509 -in "$cert" -noout -subject -nameopt RFC2253 | sed 's/^subject= *//')"

# Package family name = <name>_<publisher id>: the publisher id is the first 8 bytes of SHA-256(UTF-16LE publisher),
# plus a zero bit, in base 32 (Crockford-like alphabet). LocalState lives under that name on the console.
publisher_hash="$(printf '%s' "$publisher" | iconv -f UTF-8 -t UTF-16LE | "$openssl" dgst -sha256 -binary | od -A n -t x1 -N 8 | tr -d ' \n')"
bits=""
for ((i = 0; i < 16; i++)); do
    digit=$((16#${publisher_hash:i:1}))
    for b in 8 4 2 1; do bits+=$(( (digit & b) ? 1 : 0 )); done
done
bits+=0
alphabet=0123456789abcdefghjkmnpqrstvwxyz
publisher_id=""
for ((i = 0; i < 65; i += 5)); do publisher_id+=${alphabet:$((2#${bits:i:5})):1}; done

# --- Layout -------------------------------------------------------------------------------------------------------
stage="$(mktemp -d "$out_dir/.ssarecomp-package.XXXXXX")"
trap 'rm -rf "$stage"' EXIT
layout="$stage/layout"
mkdir -p "$layout/Assets"
cp "$here"/assets/*.png "$layout/Assets/"
cp "$exe" "$layout/SSARecomp.exe"
if [[ -f "$root/THIRD_PARTY_NOTICES.md" ]]; then
    cp "$root/THIRD_PARTY_NOTICES.md" "$layout/THIRD_PARTY_NOTICES.md"
elif [[ -f "$(dirname "$exe")/THIRD_PARTY_NOTICES.md" ]]; then
    cp "$(dirname "$exe")/THIRD_PARTY_NOTICES.md" "$layout/THIRD_PARTY_NOTICES.md"
fi
[[ "$iso" == none ]] || ln "$iso" "$layout/game.iso" 2>/dev/null || cp -c "$iso" "$layout/game.iso" 2>/dev/null || cp "$iso" "$layout/game.iso"

xml_escape() { printf '%s' "$1" | sed -e 's/&/\&amp;/g' -e 's/</\&lt;/g' -e 's/>/\&gt;/g' -e 's/"/\&quot;/g'; }
sed_value() { xml_escape "$1" | sed -e 's/[\/&|]/\\&/g'; }
sed -e "s|@NAME@|$(sed_value "$PACKAGE_NAME")|g" \
    -e "s|@PUBLISHER@|$(sed_value "$publisher")|g" \
    -e "s|@VERSION@|$version|g" \
    -e "s|@DISPLAY_NAME@|$(sed_value "$DISPLAY_NAME")|g" \
    -e "s|@PUBLISHER_DISPLAY_NAME@|$(sed_value "$PUBLISHER_DISPLAY_NAME")|g" \
    -e "s|@DESCRIPTION@|$(sed_value "$DESCRIPTION")|g" \
    -e "s|@EXECUTABLE@|SSARecomp.exe|g" \
    "$here/AppxManifest.xml.in" >"$layout/AppxManifest.xml"
! grep -v '<!--' "$layout/AppxManifest.xml" | grep -q '@[A-Z_]*@' || die "unfilled placeholder in AppxManifest.xml.in"

# --- Pack, sign, verify -------------------------------------------------------------------------------------------
echo "Packing $(basename "$out") (version $version)"
"$makemsix" pack -d "$layout" -p "$stage/unsigned.msix" >"$stage/makemsix.log" 2>&1 ||
    { cat "$stage/makemsix.log" >&2; die "makemsix failed"; }
rm -rf "$layout"

echo "Signing with $cert"
"$osslsigncode" sign -certs "$cert" -key "$key" -h sha256 -in "$stage/unsigned.msix" -out "$stage/signed.msix" \
    >"$stage/sign.log" 2>&1 || { cat "$stage/sign.log" >&2; die "signing failed"; }
rm -f "$stage/unsigned.msix"
"$osslsigncode" verify -CAfile "$cert" -in "$stage/signed.msix" >"$stage/verify.log" 2>&1 ||
    { cat "$stage/verify.log" >&2; die "the signed package does not verify"; }
mv "$stage/signed.msix" "$out"

echo "Package:        $out ($(( $(file_size "$out") >> 20 )) MB)"
echo "Identity:       $PACKAGE_NAME $version, publisher \"$publisher\""
echo "Family name:    ${PACKAGE_NAME}_$publisher_id"
echo "Certificate:    $cert_dir/SSARecomp.cer"
