#pragma once

#include "CoreMinimal.h"
#include "ZoneGraphTypes.h"

namespace StreetMapZoneGraph
{
	/**
	 * Lane profile built directly from real per-segment NVDB attributes (TN_ROAD_NUMBEROFLANES /
	 * TN_ROAD_WIDTH), unlike MakeLaneProfileForRoadType's road-class heuristic - see
	 * Tools/nvdb/DECISIONS.md. LaneWidthCm is already resolved (total paved width / lane count,
	 * see DECISIONS.md "Per-lane width") by the Python preprocessor.
	 */
	FZoneLaneProfile MakeLaneProfileForNvdbSegment(int32 LanesForward, int32 LanesBackward, float LaneWidthCm);

	/**
	 * Intersection-leg variant, mirroring MakeLaneProfileForIntersectionLeg's one-way re-orientation
	 * - only matters when the segment is effectively one-way (one direction's lane count is zero);
	 * a real two-way NVDB segment already carries both directions' true lane counts, so no flip is
	 * needed for that case.
	 */
	FZoneLaneProfile MakeLaneProfileForNvdbIntersectionLeg(int32 LanesForward, int32 LanesBackward, float LaneWidthCm, bool bLegIsAway);
}
