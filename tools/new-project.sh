#!/usr/bin/env bash
# Makes a new ESP-IDF project for the CYD that uses this copy of the library.
#   tools/new-project.sh ~/projects/my-gadget [name]
# Then:  cd ~/projects/my-gadget && source <cydui>/tools/env.sh && idf.py build flash monitor
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
dest="${1:-}"
[ -n "$dest" ] || { echo "usage: $0 <directory> [project name]" >&2; exit 2; }
name="${2:-$(basename "$dest")}"
case "$name" in
    *[!A-Za-z0-9_-]*|"") echo "new-project: the name may have only letters, digits, - and _ (got '$name')" >&2; exit 2 ;;
esac
if [ -e "$dest" ] && [ -n "$(ls -A "$dest" 2>/dev/null)" ]; then
    echo "new-project: $dest exists and is not empty" >&2; exit 1
fi

mkdir -p "$dest"
cp -r "$here/templates/cyd-starter/." "$dest/"
# the path of the library and the name go in the files (| as the separator: paths have /)
find "$dest" -type f | while read -r f; do
    sed -i -e "s|@CYDUI_PATH@|$here|g" -e "s|@NAME@|$name|g" "$f"
done

cat <<MSG
Created $dest

  cd $dest
  source $here/tools/env.sh        # loads ESP-IDF (or use your own export.sh)
  idf.py build flash monitor       # -p /dev/ttyUSB0 if the port is not found

Not sure the computer is ready?  $here/tools/doctor.sh
MSG
