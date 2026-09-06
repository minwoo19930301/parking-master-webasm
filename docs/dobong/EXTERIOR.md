# Exterior roof observations

The five existing OSM footprints below now have source-observed roof colour families instead of the generic grey roof. The RGB palette is an illustrative choice, not calibrated source colour. Footprints, height estimates, flat roof geometry and schematic glazing are unchanged; this does not establish a current facade match.

| OSM feature | Observed roof | Capture date |
|---|---|---|
| [Main hall 380676074](https://www.openstreetmap.org/way/380676074) | Broad green/teal roof | 2026-01-21 |
| [Southwest 887784323](https://www.openstreetmap.org/way/887784323) | Large green roof | 2025-03-22 |
| [Between fields 911080722](https://www.openstreetmap.org/way/911080722) | Long narrow grey/brown roof | 2025-03-22 |
| [North of ordinary course 911080726](https://www.openstreetmap.org/way/911080726) | Dark grey/blue roof, separate from the green canopy to its west | 2025-03-22 |
| [Northeast rectangle 1226934249](https://www.openstreetmap.org/way/1226934249) | Green rectangular roof with dark central patch; patch geometry not rendered | 2025-03-22 |

None of these five saved OSM features supplies `height` or `building:levels`. The existing 6m shell remains explicitly estimated, not measured. An OSM `disused` tag is not proof of current disuse. The [context audit](context-feature-audit.json) preserves source tags, geometry and uncertainty.

Source: [Esri World Imagery Wayback](https://www.arcgis.com/home/item.html?id=929ed51e5c11448c8ff82ef637bf42d6), [imagery metadata](https://www.arcgis.com/home/item.html?id=664b3448e36641628dc43c67b943e8c6). The canonical 2025-12-18 release contains imagery captured 2025-03-22; reported positional accuracy is 8.47m, before manual picking error. Original aerial imagery is not redistributed. OSM geometry remains attributed to [OpenStreetMap contributors / ODbL](https://www.openstreetmap.org/copyright).

## Unresolved southeast roof — deliberately not rendered as a building

The separate dark-teal roof immediately beside the ordinary-course southeast apron is absent from the saved 54 OSM features. Its observed roof outline is recorded in [roof-observations.geojson](roof-observations.geojson), at game XZ `(29.465,18.896)`, `(41.521,15.586)`, `(46.012,23.860)`, `(32.302,27.878)`.

This is a visible **roof plan, not a surveyed ground footprint**. Height, curvature, thickness, pillars, walls, underside access and collision geometry remain unknown. Source positional accuracy, roughly 2m manual picking uncertainty and uncorrected roof parallax apply; decimal coordinates do not imply precision. No 4m raised plane, arched canopy or collision box is invented from this evidence.

The green barrel-vault canopies in the [2021 entrance photo](https://commons.wikimedia.org/wiki/File:Seed_Cube_Changdong_under_construction_from_Dobong_Driving_license_examination_office.jpg) and [2022 entrance photo](https://commons.wikimedia.org/wiki/File:Seed_Cube_Changdong_with_Dobong_Driving_license_examination_office.jpg) belong to the entrance/road-driving waiting cluster. Their exact OSM mapping has not been resolved. They are **not evidence of the southeast roof's section** and have not been relocated there.

Current eye-level site imagery and measured heights are still required to reproduce the requested actual exterior.
