#include "StreetMapRoadLaneProfiles.h"

namespace StreetMapZoneGraph
{
	FZoneLaneProfile MakeLaneProfileForRoadType(EStreetMapRoadType RoadType, bool bIsOneWay)
	{
		int32 LanesPerDirection = 1;
		float LaneWidth = 350.0f;
		const TCHAR* RoadTypeName = TEXT("Street");

		switch (RoadType)
		{
		case EStreetMapRoadType::MajorRoad:
			LanesPerDirection = 2;
			LaneWidth = 350.0f;
			RoadTypeName = TEXT("MajorRoad");
			break;

		case EStreetMapRoadType::Highway:
			LanesPerDirection = 3;
			LaneWidth = 370.0f;
			RoadTypeName = TEXT("Highway");
			break;

		case EStreetMapRoadType::Other:
			LanesPerDirection = 1;
			LaneWidth = 350.0f;
			RoadTypeName = TEXT("Other");
			break;

		case EStreetMapRoadType::Street:
		default:
			LanesPerDirection = 1;
			LaneWidth = 350.0f;
			RoadTypeName = TEXT("Street");
			break;
		}

		FZoneLaneProfile Profile;
		Profile.Name = FName(*FString::Printf(TEXT("StreetMap_%s_%s"), RoadTypeName, bIsOneWay ? TEXT("OneWay") : TEXT("TwoWay")));

		// Backward lanes first (left side, opposing traffic), then forward lanes (right side) -
		// matches the drive-on-the-right convention; not derived from any OSM data.
		if (!bIsOneWay)
		{
			for (int32 LaneIndex = 0; LaneIndex < LanesPerDirection; ++LaneIndex)
			{
				FZoneLaneDesc Lane;
				Lane.Width = LaneWidth;
				Lane.Direction = EZoneLaneDirection::Backward;
				Profile.Lanes.Add(Lane);
			}
		}

		for (int32 LaneIndex = 0; LaneIndex < LanesPerDirection; ++LaneIndex)
		{
			FZoneLaneDesc Lane;
			Lane.Width = LaneWidth;
			Lane.Direction = EZoneLaneDirection::Forward;
			Profile.Lanes.Add(Lane);
		}

		return Profile;
	}

	FZoneLaneProfile MakeLaneProfileForIntersectionLeg(EStreetMapRoadType RoadType, bool bIsOneWay, bool bLegIsAway)
	{
		FZoneLaneProfile Profile = MakeLaneProfileForRoadType(RoadType, bIsOneWay);
		if (bIsOneWay)
		{
			const EZoneLaneDirection LegDirection = bLegIsAway ? EZoneLaneDirection::Backward : EZoneLaneDirection::Forward;
			for (FZoneLaneDesc& Lane : Profile.Lanes)
			{
				Lane.Direction = LegDirection;
			}
		}
		return Profile;
	}
}
