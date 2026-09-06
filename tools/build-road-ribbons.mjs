import {readFileSync,writeFileSync} from 'node:fs';
const data=JSON.parse(readFileSync(new URL('../docs/road-driving/route-roads.geojson',import.meta.url)));
const lines=['#pragma once','#include "road_routes.h"','namespace dobong_road_source {','// Pavement widths are practice estimates, NOT measured lane counts.','struct Road { const Point* points; std::size_t count; float width; bool oneWay; };'];
for(const [i,f] of data.features.entries()) {
  lines.push(`inline constexpr Point kRoad${i}[]={${f.properties.game_xz.map(p=>`{${p[0].toFixed(3)}f,${p[1].toFixed(3)}f}`).join(',')}};`);
}
lines.push('inline constexpr Road kRoads[]={');
for(const [i,f] of data.features.entries()) {
  const one=f.properties.tags.oneway==='yes';
  const width=f.properties.tags.highway==='service'?6:one?9:14;
  lines.push(`{kRoad${i},sizeof(kRoad${i})/sizeof(Point),${width}.f,${one}},`);
}
lines.push('};','}');
writeFileSync(new URL('../src/dobong/road_ribbons.h',import.meta.url),lines.join('\n')+'\n');
