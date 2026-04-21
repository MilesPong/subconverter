#!/bin/sh
set -eu

workflow_file=".github/workflows/docker.yml"
dockerfile="scripts/Dockerfile"

grep -F "context: ." "$workflow_file" >/dev/null || {
  echo "expected GHCR workflow to build from repository root context"
  exit 1
}

grep -F "file: scripts/Dockerfile" "$workflow_file" >/dev/null || {
  echo "expected GHCR workflow to use scripts/Dockerfile explicitly"
  exit 1
}

if grep -F "git clone https://github.com/metacubex/subconverter" "$dockerfile" >/dev/null; then
  echo "Dockerfile still clones upstream subconverter source"
  exit 1
fi

grep -F "COPY . /subconverter" "$dockerfile" >/dev/null || {
  echo "expected Dockerfile to copy the current repository into /subconverter"
  exit 1
}

if grep -F 'MAIN_IMAGE_NAME="metacubex/subconverter"' "scripts/merge_manifest.py" >/dev/null; then
  echo "merge_manifest.py still hardcodes the upstream image name"
  exit 1
fi

echo "GHCR build uses local repository source"
