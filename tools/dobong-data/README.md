# Dobong source-world reproducibility bundle

This folder regenerates three C++17 data headers from saved source geometry and
terrain values. It is self-contained, offline, and uses only **Python 3.10+
standard library**. No API key, absolute user path, browser, aerial raster, PDF,
Pillow, NumPy or network access is required.

```sh
python3 generate_cpp_headers.py --check
```

The command works from any current directory when the script path is supplied.
It writes `generated/dobong_observed_data.h`, `dobong_context_data.h`,
`dobong_dem_data.h` and `generated-manifest.json`. `--output DIRECTORY` chooses a
different output folder. `--check` compares every input hash and all three output
hashes with `expected-manifest.json` and exits nonzero on a mismatch. Inputs are
read from `data/` relative to the script, never relative to the shell directory.

## Copy contract

Copy this whole folder into the repository's chosen tools/data location. Preserve
`data/`, this script, `expected-manifest.json`, and attribution files together.
Generated headers depend on the game's existing `dobong_source_world.h` type
definitions; this bundle regenerates data, not that handwritten runtime helper.
Do not silently replace the baseline hash file to make a changed dataset pass.

Inputs are:

- `data/course-observed.geojson`: manual observation of the actual 2025-03-22
  aerial after documented 2022/2023 works; 24 features, 168 vertices, four
  alternating T access/stall footprints. Source accuracy8.47m plus about2m manual
  pick uncertainty. No current legal detection-line or ramp-height certification.
- `data/surroundings-source.geojson`: OpenStreetMap geometry,54 features and413
  vertices, including the north main hall. Heights absent from tags remain unknown.
- `data/terrain/game-dem-129x129.json`:129x129 source-derived heights,125m step,
 16km square. The grid is background relief, not wheel-contact/ramp geometry.
- `data/terrain/manifest.json` and `validation.json`: original terrain provenance,
  tile hashes, decode and peak sanity evidence. They reference raw/native/257grid
  artifacts deliberately not included here. Those files are not needed to
  regenerate the three headers; their metadata is retained and hash-checked.

World convention: WGS84 origin127.0585674/37.6564809, game x=E+52 and z=-N-60,
terrain y=source elevation minus27.953465495653163m. No invented mountain noise,
peak height correction, or schematic-to-aerial coordinate projection is applied.

## Sources, licenses and publication boundary

See `ATTRIBUTION.md` and the saved provider attribution document. Attribution
strings and individual OSM source URLs are preserved in generated data. This
folder contains **geometry/numeric data only**: no original imagery, map tiles,
official PDF artwork, screenshots, personal paths, tokens, or account data.

Absence of copyrighted raster does not itself grant a blanket license to every
derived dataset. Esri's published item terms allow data collection/editing for
listed uses, while imagery remains governed by its license. Preserve provenance
and have the repository owner apply the relevant source-use terms. OSM-derived
database content must retain ODbL obligations. Terrain is a composition of source
datasets; it is not declared wholesale CC0 or covered by a blanket code license.

## Verification

The prepared bundle reproduced all3 original headers byte-for-byte. A separate
test ran the script from the parent directory with a different output folder.
The baseline manifest contains the exact SHA256 values and is free of absolute
user paths. Header generation is deterministic UTF-8 with LF newlines.
