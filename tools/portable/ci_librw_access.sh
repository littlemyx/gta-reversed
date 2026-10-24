#!/usr/bin/env bash
# ONE-TIME setup for the GitHub Actions workflows of the fork (portable-linux.yml, portable-macos.yml); run it yourself, it changes repository settings:
#   1. generates a throw-away ed25519 key pair,
#   2. registers the public half as a READ-ONLY deploy key of the private repo littlemyx/librw (submodule vendor/librw),
#   3. stores the private half as the Actions secret LIBRW_DEPLOY_KEY of littlemyx/gta-reversed,
#   4. deletes the local key files.
# Needs `gh` logged in with repo admin rights on both repositories. Undo: delete the deploy key (Settings -> Deploy keys of librw) and the secret.
set -euo pipefail
OWNER="${OWNER:-littlemyx}"; MAIN="${MAIN:-$OWNER/gta-reversed}"; LIBRW="${LIBRW:-$OWNER/librw}"
D="$(mktemp -d)"; trap 'rm -rf "$D"' EXIT
ssh-keygen -q -t ed25519 -N "" -C "gta-reversed-ci-librw-readonly" -f "$D/k"
gh api -X POST "repos/$LIBRW/keys" -f title="gta-reversed CI (read-only)" -f key="$(cat "$D/k.pub")" -F read_only=true >/dev/null
gh secret set LIBRW_DEPLOY_KEY -R "$MAIN" < "$D/k"
echo "done: deploy key added to $LIBRW, secret LIBRW_DEPLOY_KEY set on $MAIN"
