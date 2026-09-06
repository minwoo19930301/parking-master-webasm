#!/usr/bin/env bash
set -euo pipefail
PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
DATA_DIR="$PROJECT_DIR/tools/dobong-data"
python3 "$DATA_DIR/generate_cpp_headers.py" --check
for header in dobong_observed_data.h dobong_context_data.h dobong_dem_data.h; do
  cmp "$DATA_DIR/generated/$header" "$PROJECT_DIR/src/dobong/$header"
done
printf 'PASS: runtime headers equal the independently regenerated source data.\n'
