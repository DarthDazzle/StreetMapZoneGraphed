#include "ZoneShapeRoadNetworkView.h"
#include "StreetMapZoneGraphBuilderAccess.h"

#include "ZoneGraphData.h"
#include "ZoneShapeUtilities.h"

#include "Misc/ScopeLock.h"

namespace
{
	using namespace StreetMapZoneGraph;

	// Minimum/extra clearance (cm) used when trimming road segments back from a true intersection,
	// so tessellated lane endpoints don't collapse onto a single degenerate point at the node.
	constexpr float MinIntersectionSetback = 300.0f;
	constexpr float IntersectionSetbackPadding = 100.0f;

	// One road-segment-end meeting at an intersection node. A node can be a true interior point of
	// a road (not just its start/end) when a single road passes through a junction with another
	// road without being split there - in that case the road contributes two legs (arriving from
	// before, leaving toward after), not one.
	struct FIntersectionLeg
	{
		int32 RoadIndex = INDEX_NONE;
		int32 RoadPointIndex = INDEX_NONE;
		bool bAway = false; // true: continues toward RoadPointIndex+1; false: arrives from RoadPointIndex-1.
		FVector Direction = FVector::ForwardVector; // Unit vector pointing away from the node, along the leg.
		float EdgeLength = 0.0f;
		FZoneLaneProfile LaneProfile;
		FVector TrimmedPosition = FVector::ZeroVector;
	};

	struct FIntersectionInfo
	{
		FVector Position = FVector::ZeroVector;
		TArray<FIntersectionLeg> Legs;
	};

	// Key: (RoadIndex, RoadPointIndex, bAway) -> that leg's trimmed endpoint position.
	FIntVector MakeLegKey(int32 RoadIndex, int32 RoadPointIndex, bool bAway)
	{
		return FIntVector(RoadIndex, RoadPointIndex, bAway ? 1 : 0);
	}

	// Finds every node with >= 3 road-segment-ends meeting it (a true intersection, as opposed to a
	// plain pass-through or dead-end) and computes a per-leg trim distance clamped to that leg's own
	// first-edge length, so the intersection polygon's boundary stubs coincide exactly (within
	// ZoneGraph's ~2cm connection tolerance) with the trimmed road segment endpoints built alongside.
	void FindTrueIntersections(TArrayView<const FZoneRoadView> Roads, TArrayView<const FZoneNodeView> Nodes,
		TMap<int32, FIntersectionInfo>& OutIntersections, TMap<FIntVector, FVector>& OutLegTrimPositions)
	{
		for (int32 NodeIndex = 0; NodeIndex < Nodes.Num(); ++NodeIndex)
		{
			const FZoneNodeView& Node = Nodes[NodeIndex];

			FIntersectionInfo Info;
			bool bHasPosition = false;

			for (const FZoneNodeRoadRef& RoadRef : Node.RoadRefs)
			{
				if (!Roads.IsValidIndex(RoadRef.RoadIndex))
				{
					continue;
				}
				const FZoneRoadView& Road = Roads[RoadRef.RoadIndex];
				const int32 PointIdx = RoadRef.RoadPointIndex;
				if (!Road.Points.IsValidIndex(PointIdx))
				{
					continue;
				}

				const FVector NodePos = Road.Points[PointIdx];
				if (!bHasPosition)
				{
					Info.Position = NodePos;
					bHasPosition = true;
				}

				if (PointIdx < Road.Points.Num() - 1)
				{
					FIntersectionLeg Leg;
					Leg.RoadIndex = RoadRef.RoadIndex;
					Leg.RoadPointIndex = PointIdx;
					Leg.bAway = true;
					const FVector ToNext = Road.Points[PointIdx + 1] - NodePos;
					Leg.EdgeLength = ToNext.Size();
					Leg.Direction = Leg.EdgeLength > KINDA_SMALL_NUMBER ? (ToNext / Leg.EdgeLength) : FVector::ForwardVector;
					Leg.LaneProfile = Road.AwayLegProfile;
					Info.Legs.Add(Leg);
				}
				if (PointIdx > 0)
				{
					FIntersectionLeg Leg;
					Leg.RoadIndex = RoadRef.RoadIndex;
					Leg.RoadPointIndex = PointIdx;
					Leg.bAway = false;
					const FVector ToPrev = Road.Points[PointIdx - 1] - NodePos;
					Leg.EdgeLength = ToPrev.Size();
					Leg.Direction = Leg.EdgeLength > KINDA_SMALL_NUMBER ? (ToPrev / Leg.EdgeLength) : FVector::BackwardVector;
					Leg.LaneProfile = Road.TowardLegProfile;
					Info.Legs.Add(Leg);
				}
			}

			if (Info.Legs.Num() < 3)
			{
				continue;
			}

			float WidestTotalWidth = 0.0f;
			for (const FIntersectionLeg& Leg : Info.Legs)
			{
				WidestTotalWidth = FMath::Max(WidestTotalWidth, Leg.LaneProfile.GetLanesTotalWidth());
			}
			const float DesiredSetback = FMath::Max(MinIntersectionSetback, WidestTotalWidth * 0.5f + IntersectionSetbackPadding);

			// Clamp to the shortest leg's edge across the WHOLE intersection, not per-leg -
			// otherwise one short adjoining road segment collapses only its own leg toward the
			// node while other legs keep their full setback, producing a lopsided polygon instead
			// of a symmetric one.
			float MaxAllowedSetback = DesiredSetback;
			for (const FIntersectionLeg& Leg : Info.Legs)
			{
				MaxAllowedSetback = FMath::Min(MaxAllowedSetback, Leg.EdgeLength * 0.4f);
			}

			for (FIntersectionLeg& Leg : Info.Legs)
			{
				Leg.TrimmedPosition = Info.Position + Leg.Direction * MaxAllowedSetback;
				OutLegTrimPositions.Add(MakeLegKey(Leg.RoadIndex, Leg.RoadPointIndex, Leg.bAway), Leg.TrimmedPosition);
			}

			// Order legs around the node so the polygon boundary winds sensibly.
			Info.Legs.Sort([](const FIntersectionLeg& A, const FIntersectionLeg& B)
			{
				return FMath::Atan2(A.Direction.Y, A.Direction.X) < FMath::Atan2(B.Direction.Y, B.Direction.X);
			});

			OutIntersections.Add(NodeIndex, MoveTemp(Info));
		}
	}

	void TessellateRoadSegments(TArrayView<const FZoneRoadView> Roads, const TMap<FIntVector, FVector>& LegTrimPositions,
		const FMatrix& LocalToWorld, FZoneGraphStorage& Storage, TArray<FZoneShapeLaneInternalLink>& InternalLinks)
	{
		for (int32 RoadIndex = 0; RoadIndex < Roads.Num(); ++RoadIndex)
		{
			const FZoneRoadView& Road = Roads[RoadIndex];
			const int32 NumPoints = Road.Points.Num();
			if (NumPoints < 2)
			{
				continue;
			}

			// Break the road into segments at every node (junction/endpoint), not just its overall
			// start/end - a road can pass through an intermediate true intersection, or (NVDB path)
			// an attribute-change breakpoint that isn't a real junction at all.
			TArray<int32> BreakIndices;
			BreakIndices.Add(0);
			for (int32 PointIdx = 1; PointIdx < NumPoints - 1; ++PointIdx)
			{
				if (Road.NodeIndices.IsValidIndex(PointIdx) && Road.NodeIndices[PointIdx] != INDEX_NONE)
				{
					BreakIndices.Add(PointIdx);
				}
			}
			BreakIndices.Add(NumPoints - 1);

			for (int32 SegmentIdx = 0; SegmentIdx < BreakIndices.Num() - 1; ++SegmentIdx)
			{
				const int32 StartIdx = BreakIndices[SegmentIdx];
				const int32 EndIdx = BreakIndices[SegmentIdx + 1];
				if (EndIdx <= StartIdx)
				{
					continue;
				}

				TArray<FZoneShapePoint> ShapePoints;
				ShapePoints.Reserve(EndIdx - StartIdx + 1);
				for (int32 PointIdx = StartIdx; PointIdx <= EndIdx; ++PointIdx)
				{
					FZoneShapePoint ShapePoint;
					ShapePoint.Position = Road.Points[PointIdx];
					ShapePoint.Type = (PointIdx == StartIdx || PointIdx == EndIdx) ? FZoneShapePointType::LaneProfile : FZoneShapePointType::AutoBezier;
					ShapePoints.Add(ShapePoint);
				}

				if (const FVector* Trimmed = LegTrimPositions.Find(MakeLegKey(RoadIndex, StartIdx, true)))
				{
					ShapePoints[0].Position = *Trimmed;
				}
				if (const FVector* Trimmed = LegTrimPositions.Find(MakeLegKey(RoadIndex, EndIdx, false)))
				{
					ShapePoints.Last().Position = *Trimmed;
				}

				const int32 LastPointIdx = ShapePoints.Num() - 1;
				const FVector StartDir = (ShapePoints[1].Position - ShapePoints[0].Position).GetSafeNormal();
				ShapePoints[0].SetRotationFromForwardAndUp(StartDir, FVector::UpVector);
				const FVector EndDir = (ShapePoints[LastPointIdx].Position - ShapePoints[LastPointIdx - 1].Position).GetSafeNormal();
				ShapePoints[LastPointIdx].SetRotationFromForwardAndUp(EndDir, FVector::UpVector);

				UE::ZoneShape::Utilities::TessellateSplineShape(ShapePoints, Road.MainProfile, FZoneGraphTagMask::None, LocalToWorld, Storage, InternalLinks);
			}
		}
	}

	void TessellateIntersections(TMap<int32, FIntersectionInfo>& Intersections, const FMatrix& LocalToWorld,
		FZoneGraphStorage& Storage, TArray<FZoneShapeLaneInternalLink>& InternalLinks)
	{
		for (TPair<int32, FIntersectionInfo>& Pair : Intersections)
		{
			const FIntersectionInfo& Info = Pair.Value;

			TArray<FZoneShapePoint> PolyPoints;
			TArray<FZoneLaneProfile> PolyProfiles;
			PolyPoints.Reserve(Info.Legs.Num());
			PolyProfiles.Reserve(Info.Legs.Num());

			for (int32 LegIdx = 0; LegIdx < Info.Legs.Num(); ++LegIdx)
			{
				const FIntersectionLeg& Leg = Info.Legs[LegIdx];

				FZoneShapePoint Point;
				Point.Position = Leg.TrimmedPosition;
				Point.Type = FZoneShapePointType::LaneProfile;
				Point.LaneProfile = static_cast<uint8>(LegIdx);
				// Lane profile points face into the shape, opposite of the leg's away-from-node direction.
				Point.SetRotationFromForwardAndUp(-Leg.Direction, FVector::UpVector);

				PolyPoints.Add(Point);
				PolyProfiles.Add(Leg.LaneProfile);
			}

			UE::ZoneShape::Utilities::TessellatePolygonShape(PolyPoints, EZoneShapePolygonRoutingType::Bezier, PolyProfiles, FZoneGraphTagMask::None, LocalToWorld, Storage, InternalLinks);
		}
	}
}

namespace StreetMapZoneGraph
{
	void BuildZoneGraphFromRoadNetwork(TArrayView<const FZoneRoadView> Roads, TArrayView<const FZoneNodeView> Nodes,
		const FMatrix& LocalToWorld, AZoneGraphData& OutData)
	{
		FScopeLock StorageLock(&OutData.GetStorageLock());
		OutData.Modify();

		FZoneGraphStorage& Storage = OutData.GetStorageMutable();
		Storage.Reset();

		TArray<FZoneShapeLaneInternalLink> InternalLinks;

		TMap<int32, FIntersectionInfo> Intersections;
		TMap<FIntVector, FVector> LegTrimPositions;
		FindTrueIntersections(Roads, Nodes, Intersections, LegTrimPositions);

		TessellateRoadSegments(Roads, LegTrimPositions, LocalToWorld, Storage, InternalLinks);
		TessellateIntersections(Intersections, LocalToWorld, Storage, InternalLinks);

		FStreetMapZoneGraphBuilderAccess::ConnectLanes(InternalLinks, Storage);

		Storage.Bounds = FBox(ForceInit);
		for (FZoneData& Zone : Storage.Zones)
		{
			Storage.Bounds += Zone.Bounds;
		}

		// NOTE: FZoneGraphBVTree::Build() is not exported (no ZONEGRAPH_API) from the ZoneGraph
		// module, so it can't be linked from here - the spatial BV-tree acceleration structure is
		// left unbuilt. Debug-draw and direct Zones/Lanes inspection still work; ZoneGraphQuery
		// spatial lookups (needed later for Mass AI navigation) will not until this is resolved,
		// either by finding an exported rebuild path or switching to UZoneShapeComponent-based
		// generation through FZoneGraphBuilder::BuildAll().

		OutData.UpdateDrawing();
	}
}
