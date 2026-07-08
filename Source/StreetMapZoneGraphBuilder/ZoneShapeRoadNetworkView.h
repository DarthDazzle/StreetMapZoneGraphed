#pragma once

#include "CoreMinimal.h"
#include "ZoneGraphTypes.h"

class AZoneGraphData;

namespace StreetMapZoneGraph
{
	/**
	 * Source-agnostic view of one road (a polyline that gets tessellated into a single ZoneGraph
	 * lane-profile shape, possibly split further at interior nodes). Both the OSM path
	 * (StreetMapToZoneShapeConverter, wrapping UStreetMap::GetRoads()) and the NVDB path
	 * (NvdbToZoneShapeConverter, wrapping the Tools/nvdb intermediate file) build this from their
	 * own data so the shared tessellation logic in ZoneShapeRoadNetworkBuilder.cpp never needs to
	 * know which source it came from. See Tools/nvdb/DECISIONS.md "Shared tessellation code".
	 */
	struct FZoneRoadView
	{
		// Already fully resolved 3D positions (local/world units, Z baked in by the caller -
		// e.g. via FStreetMapHeightGrid for OSM, or pre-baked in Python for NVDB).
		TArray<FVector> Points;

		// Parallel to Points. INDEX_NONE for a point that isn't a graph node; otherwise an index
		// into the sibling Nodes array. Any non-INDEX_NONE entry (not just the first/last point)
		// splits this road into separately-tessellated-but-stitched-together shapes.
		TArray<int32> NodeIndices;

		// Profile for the plain road-segment shape (TessellateRoadSegments).
		FZoneLaneProfile MainProfile;

		// Profile for an intersection leg departing this road AWAY from a node (continuing toward
		// higher point indices). Differs from MainProfile only when the road is effectively
		// one-way, where lane direction must be re-expressed relative to which side of the
		// junction the leg sits on - see StreetMapRoadLaneProfiles.h's original comment.
		FZoneLaneProfile AwayLegProfile;

		// Profile for an intersection leg arriving at this road's node (from lower point indices).
		FZoneLaneProfile TowardLegProfile;
	};

	struct FZoneNodeRoadRef
	{
		int32 RoadIndex = INDEX_NONE;
		int32 RoadPointIndex = INDEX_NONE;
	};

	struct FZoneNodeView
	{
		TArray<FZoneNodeRoadRef> RoadRefs;
	};

	/**
	 * Builds ZoneGraph zone/lane data from a source-agnostic road network directly into OutData's
	 * storage, in place. Mirrors the tail sequence of FZoneGraphBuilder::Build() (tessellate ->
	 * connect lanes -> recompute bounds -> rebuild BV-tree) but sources shapes from Roads/Nodes
	 * instead of registered UZoneShapeComponents.
	 */
	void BuildZoneGraphFromRoadNetwork(TArrayView<const FZoneRoadView> Roads, TArrayView<const FZoneNodeView> Nodes,
		const FMatrix& LocalToWorld, AZoneGraphData& OutData);
}
