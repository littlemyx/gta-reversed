#!/usr/bin/env bash
# CI step: check out the vendor/librw submodule. littlemyx/librw is a PRIVATE repo, so the job's GITHUB_TOKEN cannot read it: the workflow passes
#   LIBRW_DEPLOY_KEY  (secret: private half of a read-only deploy key of littlemyx/librw; set up by tools/portable/ci_librw_access.sh)   or
#   LIBRW_TOKEN       (secret: a fine-grained PAT with Contents:read on littlemyx/librw)
# With neither the step fails early with the instructions instead of a cryptic "repository not found".
set -euo pipefail
if [ -f vendor/librw/CMakeLists.txt ]; then echo "vendor/librw already present"; exit 0; fi
if [ -n "${LIBRW_DEPLOY_KEY:-}" ]; then
    mkdir -p ~/.ssh && chmod 700 ~/.ssh
    printf '%s\n' "$LIBRW_DEPLOY_KEY" > ~/.ssh/id_librw && chmod 600 ~/.ssh/id_librw
    ssh-keyscan -t ed25519,rsa github.com >> ~/.ssh/known_hosts 2>/dev/null
    export GIT_SSH_COMMAND="ssh -i $HOME/.ssh/id_librw -o IdentitiesOnly=yes -o StrictHostKeyChecking=yes"
    git -c url."git@github.com:".insteadOf="https://github.com/" submodule update --init --depth 1 vendor/librw
    rm -f ~/.ssh/id_librw
elif [ -n "${LIBRW_TOKEN:-}" ]; then
    git -c url."https://x-access-token:${LIBRW_TOKEN}@github.com/".insteadOf="https://github.com/" submodule update --init --depth 1 vendor/librw
else
    echo "::error::vendor/librw (littlemyx/librw, private) cannot be fetched: neither secret LIBRW_DEPLOY_KEY nor LIBRW_TOKEN is set on this repository. Run tools/portable/ci_librw_access.sh once (or make librw public)."
    exit 1
fi
test -f vendor/librw/CMakeLists.txt
