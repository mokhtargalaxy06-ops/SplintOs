#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
temporary=$(mktemp)
trap 'rm -f "$temporary"' EXIT HUP INT TERM

find "$root" -path "$root/build" -prune -o -path "$root/.git" -prune -o \
    -name '*.md' -type f -print | LC_ALL=C sort > "$temporary"

while IFS= read -r document; do
    directory=$(dirname -- "$document")
    sed -n 's/.*](\([^)]*\)).*/\1/p' "$document" | while IFS= read -r target; do
        case "$target" in
            ''|'#'*|http://*|https://*|mailto:*) continue ;;
        esac
        path=${target%%#*}
        [ -e "$directory/$path" ] || {
            echo "broken local documentation link: ${document#$root/} -> $target" >&2
            exit 1
        }
    done
done < "$temporary"

echo "Local documentation links verified"
