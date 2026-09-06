# Source attribution and limitations

## Observed course geometry

Manual observations from Esri World Imagery Wayback, capture2025-03-22,
release2025-12-18. Attribution: **Esri, Vantor, Earthstar Geographics, and the GIS
User Community**. Source product LG01,0.34m resolution,8.47m stated positional
accuracy. Manual picks add uncertainty; these are not surveyed/legal test bounds.

- [Imagery item and license information](https://www.arcgis.com/home/item.html?id=929ed51e5c11448c8ff82ef637bf42d6)
- [Source metadata item](https://www.arcgis.com/home/item.html?id=664b3448e36641628dc43c67b943e8c6)
- [Esri data collection/editing use cases](https://www.arcgis.com/home/item.html?id=8e90a00a0a6845a49262e0b756f57a10)
- [Official historical Dobong course guide](https://www.koroad.or.kr/main/board/9/87986/board_view.do?bcstIdx1=61&bcstIdx2=74)
- [2022 ramp-bottom works notice](https://www.koroad.or.kr/main/board/9/69273/board_view.do?bcstIdx1=61&bcstIdx2=74)
- [2023 post-acceleration turn works notice](https://www.koroad.or.kr/main/board/9/88325/board_view.do?bcstIdx1=61&bcstIdx2=74)

No imagery, tile raster, PDF artwork or official logo is distributed here. Image
filenames in metadata are provenance identifiers, not bundled assets. The
official schematic establishes historical topology only; its pixels were not
projected into the aerial geometry.2026 shadow prevents complete current check.

## Surroundings geometry

**OpenStreetMap contributors, Open Database License1.0 (ODbL).** Individual
feature source URLs/tags and attribution remain in the GeoJSON/header.
[Copyright and license](https://www.openstreetmap.org/copyright).
No claim is made that contributor heights, dates, or geometry are surveyed/current.

## Terrain heights

**Mapzen; SRTM and GMTED2010 terrain data courtesy of the U.S. Geological Survey.
Terrain Tiles accessed2026-09-06 from AWS Open Data Registry.**

- [AWS Terrain Tiles registry](https://registry.opendata.aws/terrain-tiles/)
- [Terrarium encoding](https://github.com/tilezen/joerd/blob/master/docs/formats.md)
- [Composition sources](https://github.com/tilezen/joerd/blob/master/docs/data-sources.md)
- [Provider attribution/license instructions](https://github.com/tilezen/joerd/blob/master/docs/attribution.md)

The saved `data/terrain/mapzen-attribution.md` preserves the provider's attribution
instructions. Raw source tile URLs, processing dates and SHA256 values are in
the manifest. Decode is R*256+G+B/256-32768; height resampling occurs only after
decode. Native ground sampling here is about30.26m; published mesh grid125m.
Peak sanity differences of roughly9–42m are recorded, not corrected. This is
background relief, not a land survey. No blanket CC0 claim is made.
