#pragma once

#include "CoreMinimal.h"

class UStreetMap;
class AZoneGraphData;

namespace StreetMapZoneGraph
{
	/**
	 * Builds ZoneGraph zone/lane data from a UStreetMap's OSM-derived road graph
	 * (UStreetMap::GetRoads()/GetNodes()) directly into OutData's storage, in place.
	 * Mirrors the tail sequence of FZoneGraphBuilder::Build() (tessellate -> connect lanes ->
	 * recompute bounds -> rebuild BV-tree) but sources shapes from StreetMap road/node data
	 * instead of registered UZoneShapeComponents.
	 */
	void BuildZoneGraphFromStreetMap(const UStreetMap& StreetMap, AZoneGraphData& OutData);
}
