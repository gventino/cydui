#!/usr/bin/env bash
# Publishes this library to the ESP Component Registry (components.espressif.com) as gventino/cydui.
#
#   tools/publish-registry.sh              checks, packs, asks for the token, validates (dry run), asks, uploads
#   tools/publish-registry.sh --check      only the checks and the pack: no token, nothing is sent
#
# Before the first time: sign in at https://components.espressif.com with GitHub, then make a token there
# (Profile > Tokens, scopes "user" and "write:components"). The script prints the link and reads the token with `read -s`:
# it is never shown, never put in a file by this script and never given on a command line. The login that the
# component manager keeps in ~/.espressif/idf_component_manager.yml is removed at the end (KEEP_LOGIN=1 keeps it).
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"
namespace="${CYDUI_REGISTRY_NAMESPACE:-gventino}"
name="cydui"
check_only=0
[ "${1:-}" = "--check" ] && check_only=1

say()  { printf '\n==> %s\n' "$1"; }
die()  { printf 'publish-registry: %s\n' "$1" >&2; exit 1; }

say "Loading ESP-IDF"
# shellcheck disable=SC1091
source tools/env.sh >/dev/null 2>&1 || die "could not load ESP-IDF (see tools/doctor.sh)"
command -v compote >/dev/null || die "compote (the IDF component manager) is not on the PATH after loading ESP-IDF"

say "Checking the release"
version="$(sed -n 's/^version: *"\(.*\)"/\1/p' idf_component.yml)"
header="$(sed -n 's/^#define CYDUI_VERSION_STRING "\(.*\)"/\1/p' include/cydui/cydui.h)"
[ -n "$version" ] || die "no version in idf_component.yml"
[ "$version" = "$header" ] || die "idf_component.yml says $version but cydui.h says $header"
echo "  version $version"
if [ -n "$(git status --porcelain --untracked-files=no)" ]; then die "there are uncommitted changes: commit them first (the archive is made from the files on disk)"; fi
git rev-parse -q --verify "refs/tags/v$version" >/dev/null || die "the tag v$version does not exist"
tag_commit="$(git rev-list -n1 "v$version")"
if [ "$tag_commit" != "$(git rev-parse HEAD)" ]; then
    echo "  note: HEAD is not the commit of the tag v$version (the registry gets the files as they are now)"
fi
case "$version" in *-*) echo "  note: $version is a pre-release: people get it only when they ask for that exact version" ;; esac

say "Packing"
dist="$(mktemp -d)"
trap 'rm -rf "$dist"' EXIT
compote component pack --name "$name" --dest-dir "$dist" 2>&1 | tail -1
archive="$(ls "$dist"/*.tgz)"
size="$(du -k "$archive" | cut -f1)"
files="$(tar tzf "$archive" | wc -l)"
echo "  $(basename "$archive"): ${size} KB, $files files"
[ "$size" -lt 5000 ] || die "the archive is over 5 MB: something that should be excluded got in (see files.exclude in idf_component.yml)"
if tar tzf "$archive" | grep -Eq '(/|^)(build|\.venv|slow-unit)'; then die "the archive has build output or a virtual environment in it"; fi

if [ "$check_only" = 1 ]; then
    say "Checks passed. Nothing was sent (--check)."
    exit 0
fi

say "Token"
echo "Open this link (signed in with GitHub), create a token with the scopes user and write:components, and copy it:"
echo "  https://components.espressif.com/settings/tokens?scope=user%20write%3Acomponents&description=cydui%20publish"
printf 'Paste the token (nothing is shown): '
IFS= read -rs token
printf '\n'
[ -n "$token" ] || die "no token"

cleanup() {
    unset token
    if [ "${KEEP_LOGIN:-0}" != 1 ]; then compote registry logout >/dev/null 2>&1 || true; fi
    rm -rf "$dist"
}
trap cleanup EXIT

say "Logging in (the token goes to the login command through a pipe, not an argument)"
printf '%s\n' "$token" | compote registry login --no-browser --default-namespace "$namespace" --description "cydui publish" >/dev/null 2>&1 \
    || die "the registry did not accept the token (it needs the scopes user and write:components)"
unset token
echo "  logged in; the default namespace is $namespace"

say "Validating on the registry (a dry run: no version is created)"
compote component upload --name "$name" --namespace "$namespace" --dry-run

printf '\nUpload %s/%s %s for real? This cannot be undone (a version can only be yanked). Type yes: ' "$namespace" "$name" "$version"
read -r answer
[ "$answer" = "yes" ] || { echo "Not uploaded."; exit 0; }

say "Uploading"
compote component upload --name "$name" --namespace "$namespace"

cat <<MSG

Done. The page (it may take a minute to appear): https://components.espressif.com/components/$namespace/$name
Anyone can now use it with:   idf.py add-dependency "$namespace/$name==$version"
MSG
