#include "StreetMapToZoneShapeConverter.h"
#include "StreetMapRoadLaneProfiles.h"
#include "ZoneShapeRoadNetworkView.h"
#include "StreetMapHeightGrid.h"

#include "StreetMap.h"

namespace
{
	using namespace StreetMapZoneGraph;

	// HeightGrid may be nullptr (no sidecar height grid found for this StreetMap's source .osm) -
	// in that case every point stays flat (Z=0), same as before height injection existed.
	FVector ToWorld(const FVector2D& Point, const FStreetMapHeightGrid* HeightGrid)
	{
		const double Z = HeightGrid ? HeightGrid->SampleHeightCm(Point) : 0.0;
		return FVector(Point.X, Point.Y, Z);
	}

	// Builds the source-agnostic road/node views straight from UStreetMap's OSM-derived arrays -
	// see ZoneShapeRoadNetworkView.h for why this indirection exists (shared with the NVDB path).
	void BuildRoadNetworkViews(const UStreetMap& StreetMap, const FStreetMapHeightGrid* HeightGrid,
		TArray<FZoneRoadView>& OutRoads, TArray<FZoneNodeView>& OutNodes)
	{
		const TArray<FStreetMapRoad>& Roads = StreetMap.GetRoads();
		const TArray<FStreetMapNode>& Nodes = StreetMap.GetNodes();

		OutRoads.Reserve(Roads.Num());
		for (const FStreetMapRoad& Road : Roads)
		{
			FZoneRoadView RoadView;
			RoadView.Points.Reserve(Road.RoadPoints.Num());
			for (const FVector2D& Point : Road.RoadPoints)
			{
				RoadView.Points.Add(ToWorld(Point, HeightGrid));
			}
			RoadView.NodeIndices = Road.NodeIndices;
			RoadView.MainProfile = MakeLaneProfileForRoadType(Road.RoadType, Road.IsOneWay());
			RoadView.AwayLegProfile = MakeLaneProfileForIntersectionLeg(Road.RoadType, Road.IsOneWay(), /*bLegIsAway=*/true);
			RoadView.TowardLegProfile = MakeLaneProfileForIntersectionLeg(Road.RoadType, Road.IsOneWay(), /*bLegIsAway=*/false);
			OutRoads.Add(MoveTemp(RoadView));
		}

		OutNodes.Reserve(Nodes.Num());
		for (const FStreetMapNode& Node : Nodes)
		{
			FZoneNodeView NodeView;
			NodeView.RoadRefs.Reserve(Node.RoadRefs.Num());
			for (const FStreetMapRoadRef& RoadRef : Node.RoadRefs)
			{
				NodeView.RoadRefs.Add(FZoneNodeRoadRef{RoadRef.RoadIndex, RoadRef.RoadPointIndex});
			}
			OutNodes.Add(MoveTemp(NodeView));
		}
	}
}

namespace StreetMapZoneGraph
{
	void BuildZoneGraphFromStreetMap(const UStreetMap& StreetMap, AZoneGraphData& OutData)
	{
		FStreetMapHeightGrid HeightGrid;
		const bool bHasHeightGrid = HeightGrid.LoadForStreetMap(StreetMap);
		const FStreetMapHeightGrid* HeightGridPtr = bHasHeightGrid ? &HeightGrid : nullptr;

		TArray<FZoneRoadView> Roads;
		TArray<FZoneNodeView> Nodes;
		BuildRoadNetworkViews(StreetMap, HeightGridPtr, Roads, Nodes);

		BuildZoneGraphFromRoadNetwork(Roads, Nodes, FMatrix::Identity, OutData);
	}
}
