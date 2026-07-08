#pragma once

#include "CoreMinimal.h"
#include "ZoneGraphTypes.h"
#include "StreetMap.h"

namespace StreetMapZoneGraph
{
	/**
	 * Synthetic per-RoadType lane profile. The StreetMap OSM importer never parses lane counts,
	 * widths, or speed limits (only highway=* class collapsed to Street/MajorRoad/Highway/Other,
	 * plus oneway) - so lane geometry here is a fixed approximation by road class, not derived
	 * from real OSM tags.
	 */
	FZoneLaneProfile MakeLaneProfileForRoadType(EStreetMapRoadType RoadType, bool bIsOneWay);

	/**
	 * Lane profile for one specific leg of an intersection stub. Differs from
	 * MakeLaneProfileForRoadType only for one-way roads: a one-way road's single lane always flows
	 * in the OSM way's forward (increasing RoadPointIndex) direction, which is a DIFFERENT direction
	 * relative to the junction depending on which side of the junction this leg sits on. At the node
	 * the road departs from (bLegIsAway true, i.e. the way continues on to a higher index from here),
	 * that flow points away from the junction - the junction feeds this leg, so it's a destination
	 * stub (Backward). At the node the road arrives at (bLegIsAway false), the flow points into the
	 * junction - a source stub (Forward). Two-way roads are unaffected (both directions already
	 * present in the base profile either way).
	 */
	FZoneLaneProfile MakeLaneProfileForIntersectionLeg(EStreetMapRoadType RoadType, bool bIsOneWay, bool bLegIsAway);
}
