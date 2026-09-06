# Dobong source-world reconstruction — evidence and remaining gaps

This is a work-in-progress reconstruction, not an identical current field replica, surveyed boundary, or official training/assessment service.

## Source coordinates

- Origin WGS84 longitude 127.0585674, latitude 37.6564809; local game x = east + 52 metres, z = −north − 60 metres. Each course/context feature keeps its source identity and quality.
- Ordinary-car course is in the **southwest campus**, not the former generic east-side layout. A north-up aerial trace records outer pavement, two northern islands, a parking-complex envelope, **four alternating T access/stall outlines**, a southeast parked-vehicle apron and two excluded hatched corners. The apron, islands, parking lawns and hatched exclusions are not freely drivable. Access outlines are not certified T-parking detection lines.
- Canonical Esri Wayback release **2025-12-18** contains z19 imagery actually captured **2025-03-22**, resolution 0.34m, reported positional accuracy **8.47m**. This post-construction source replaces the old 2021 trace and its three inferred gaps. Broad exposed outer parts were compared with imagery captured **2026-01-21**, resolution 0.5m, reported accuracy 8.47m; shadow obscures much of the current interior. Estimated manual picking uncertainty is about 2m, in addition to source accuracy. Pixel spacing is not measurement accuracy. Source imagery is not redistributed in this repository.
- [Official 2023 Dobong course guide](https://www.koroad.or.kr/main/board/9/87986/board_view.do?bcstIdx1=61&bcstIdx2=74), published 2023-04-10, establishes course topology and procedure. The drawing is not north-up or a georeferenced survey. Do not treat the drawn T-bays as measured coordinates.
- Official notices document June 2022 ramp-end widening and June 2023 widening after the acceleration section. The canonical 2025 trace is after both works; nevertheless it cannot establish an unchanged 2026 field. Exact current boundaries, ramp height, operational start/finish lines and facade details remain under verification.
- The 24 source features / 168 vertices include observed crosswalk references, an east-side ramp plan marking and longitudinal divider. Runtime stripe widths, spacing and surface colours are illustrative. The ramp outline is flat: its height/profile is not invented. The west acceleration axis is retained as a reference, not drawn as a physical line or used as a scored speed trigger.

## Context and terrain

[Exterior observations](EXTERIOR.md) distinguish five observed roof colour families and the separate, unresolved southeast roof. Only colours of five existing footprints are updated; their estimated heights are not treated as measured. Entrance arches are not transplanted onto the southeast roof. The unresolved roof outline is preserved as evidence but not rendered as a guessed building.

OSM building footprints, roads, elevated Line 4 alignments and power infrastructure come from the per-feature URLs in `surroundings-source.geojson`. OSM height tags are contributor data, not measured-by-us values. Missing heights use **illustrative** 6m low-rise shells or 3m per reported storey; glazing is schematic. Rail layer is ordering, not height: the 8m deck, 28m towers and 5m wire sag are explicitly estimated. No trains or current operation are inferred from OSM disused/end-date tags.

The 36 building shells include [Sanggye Substation, way/472066007](https://www.openstreetmap.org/way/472066007): its saved tags explicitly say `building=yes` and `location=indoor`. Its power/substation classification previously caused the renderer to omit the building. The source footprint is now included, with the same explicitly illustrative 6m height because height/storeys are absent. This does not assume every substation is an enclosed building or reproduce its actual facade.

Real Mapzen/AWS Terrarium tiles (z12, x3492–3494, y1583–1585) identify USGS SRTM N37E127 and nearby SRTM/GMTED sources. Decoded source spacing is about 30.26m; runtime uses a 129² / 125m grid over 16×16km. Heights are relative to an estimated DEM course-centre elevation of 27.953465m, not a survey benchmark. The local 500m region is flattened for vehicle contact and blends to source relief by 900m; it does not supply a ramp model. Published peak elevations exceed sampled DEM peaks by about 9–42m. Source pixel precision and PNG modification dates do not prove capture date or vertical accuracy.

Attribution: **OpenStreetMap contributors**, [ODbL 1.0](https://www.openstreetmap.org/copyright). OSM-derived geometry retains that attribution and database license; it is not relicensed by the code license. **Mapzen; SRTM and GMTED2010 terrain data courtesy of the U.S. Geological Survey**; [AWS source](https://registry.opendata.aws/terrain-tiles/), [Tilezen attribution](https://github.com/tilezen/joerd/blob/master/docs/attribution.md). Composite source licenses apply; no blanket CC0 claim is made.

## Implementation boundary

The self-contained [reproduction bundle](../../tools/dobong-data/README.md) regenerates all three data headers offline using Python's standard library. `npm run test:sources` validates five input hashes and three output hashes, then compares each result with the runtime header. Original aerials/PDFs are not included.

Source-world driving and mirrors share world-space DEM/buildings. The old procedural course is retained only as explicitly labelled illustrative rules practice; its ramp, route and scoring triggers are not mapped onto the observed ground. Keyboard pedals and a full visible wheel apply to both modes. Source-code tests/builds and CPU geometry checks do not replace runtime visual, interaction, performance or on-site verification.
