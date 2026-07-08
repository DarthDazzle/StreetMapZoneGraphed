#include "NvdbRoadLaneProfiles.h"

namespace StreetMapZoneGraph
{
	FZoneLaneProfile MakeLaneProfileForNvdbSegment(int32 LanesForward, int32 LanesBackward, float LaneWidthCm)
	{
		FZoneLaneProfile Profile;
		Profile.Name = FName(*FString::Printf(TEXT("Nvdb_F%d_B%d_W%d"), LanesForward, LanesBackward, FMath::RoundToInt(LaneWidthCm)));

		// Backward lanes first (left side, opposing traffic), then forward lanes (right side) -
		// matches StreetMapRoadLaneProfiles.cpp's convention, not derived from any NVDB field.
		for (int32 LaneIndex = 0; LaneIndex < LanesBackward; ++LaneIndex)
		{
			FZoneLaneDesc Lane;
			Lane.Width = LaneWidthCm;
			Lane.Direction = EZoneLaneDirection::Backward;
			Profile.Lanes.Add(Lane);
		}
		for (int32 LaneIndex = 0; LaneIndex < LanesForward; ++LaneIndex)
		{
			FZoneLaneDesc Lane;
			Lane.Width = LaneWidthCm;
			Lane.Direction = EZoneLaneDirection::Forward;
			Profile.Lanes.Add(Lane);
		}

		return Profile;
	}

	FZoneLaneProfile MakeLaneProfileForNvdbIntersectionLeg(int32 LanesForward, int32 LanesBackward, float LaneWidthCm, bool bLegIsAway)
	{
		FZoneLaneProfile Profile = MakeLaneProfileForNvdbSegment(LanesForward, LanesBackward, LaneWidthCm);

		// Only a segment with lanes in ONE direction needs leg re-orientation - matches
		// MakeLaneProfileForIntersectionLeg's one-way case exactly (see StreetMapRoadLaneProfiles.h).
		const bool bEffectivelyOneWay = (LanesForward == 0) != (LanesBackward == 0);
		if (bEffectivelyOneWay)
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
