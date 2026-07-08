#include "NvdbToZoneShapeConverter.h"
#include "NvdbRoadLaneProfiles.h"
#include "ZoneShapeRoadNetworkView.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"

namespace
{
	using namespace StreetMapZoneGraph;

	constexpr float MetersToCentimeters = 100.0f;
	constexpr const TCHAR* ExpectedFormat = TEXT("vvtm-nvdbnetwork-v1");

	bool ParseSegment(const TSharedPtr<FJsonObject>& SegObj, const TMap<int32, int32>& NodeIdToIndex,
		int32 RoadIndex, TArray<FZoneRoadView>& OutRoads, TArray<FZoneNodeView>& OutNodes, FString& OutError)
	{
		const TArray<TSharedPtr<FJsonValue>>* PointsArray = nullptr;
		if (!SegObj->TryGetArrayField(TEXT("points_cm"), PointsArray) || PointsArray->Num() < 2)
		{
			OutError = TEXT("segment missing points_cm (or <2 points)");
			return false;
		}

		FZoneRoadView RoadView;
		RoadView.Points.Reserve(PointsArray->Num());
		for (const TSharedPtr<FJsonValue>& PointVal : *PointsArray)
		{
			const TArray<TSharedPtr<FJsonValue>>& XYZ = PointVal->AsArray();
			if (XYZ.Num() != 3)
			{
				OutError = TEXT("points_cm entry is not a 3-element [x,y,z]");
				return false;
			}
			RoadView.Points.Add(FVector(XYZ[0]->AsNumber(), XYZ[1]->AsNumber(), XYZ[2]->AsNumber()));
		}

		RoadView.NodeIndices.Init(INDEX_NONE, RoadView.Points.Num());

		int32 StartNodeJsonId = 0, EndNodeJsonId = 0;
		SegObj->TryGetNumberField(TEXT("start_node"), StartNodeJsonId);
		SegObj->TryGetNumberField(TEXT("end_node"), EndNodeJsonId);
		const int32* StartIndex = NodeIdToIndex.Find(StartNodeJsonId);
		const int32* EndIndex = NodeIdToIndex.Find(EndNodeJsonId);
		if (!StartIndex || !EndIndex)
		{
			OutError = TEXT("segment references an unknown start_node/end_node id");
			return false;
		}
		RoadView.NodeIndices[0] = *StartIndex;
		RoadView.NodeIndices.Last() = *EndIndex;
		OutNodes[*StartIndex].RoadRefs.Add(FZoneNodeRoadRef{RoadIndex, 0});
		OutNodes[*EndIndex].RoadRefs.Add(FZoneNodeRoadRef{RoadIndex, RoadView.Points.Num() - 1});

		int32 LanesForward = 1, LanesBackward = 1;
		double LaneWidthM = 3.5, SpeedKph = 50.0;
		SegObj->TryGetNumberField(TEXT("lanes_forward"), LanesForward);
		SegObj->TryGetNumberField(TEXT("lanes_backward"), LanesBackward);
		SegObj->TryGetNumberField(TEXT("lane_width_m"), LaneWidthM);
		SegObj->TryGetNumberField(TEXT("speed_kph"), SpeedKph);
		const float LaneWidthCm = static_cast<float>(LaneWidthM) * MetersToCentimeters;

		RoadView.MainProfile = MakeLaneProfileForNvdbSegment(LanesForward, LanesBackward, LaneWidthCm);
		RoadView.AwayLegProfile = MakeLaneProfileForNvdbIntersectionLeg(LanesForward, LanesBackward, LaneWidthCm, /*bLegIsAway=*/true);
		RoadView.TowardLegProfile = MakeLaneProfileForNvdbIntersectionLeg(LanesForward, LanesBackward, LaneWidthCm, /*bLegIsAway=*/false);

		OutRoads.Add(MoveTemp(RoadView));
		return true;
	}
}

namespace StreetMapZoneGraph
{
	bool BuildZoneGraphFromNvdbFile(const FString& JsonFilePath, AZoneGraphData& OutData)
	{
		FString JsonText;
		if (!FFileHelper::LoadFileToString(JsonText, *JsonFilePath))
		{
			UE_LOG(LogTemp, Warning, TEXT("BuildZoneGraphFromNvdbFile: could not read '%s'"), *JsonFilePath);
			return false;
		}

		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("BuildZoneGraphFromNvdbFile: '%s' is not valid JSON"), *JsonFilePath);
			return false;
		}

		FString Format;
		if (!Root->TryGetStringField(TEXT("format"), Format) || Format != ExpectedFormat)
		{
			UE_LOG(LogTemp, Warning, TEXT("BuildZoneGraphFromNvdbFile: '%s' has unexpected format '%s' (expected '%s')"),
				*JsonFilePath, *Format, ExpectedFormat);
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* NodesArray = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* SegmentsArray = nullptr;
		if (!Root->TryGetArrayField(TEXT("nodes"), NodesArray) || !Root->TryGetArrayField(TEXT("segments"), SegmentsArray))
		{
			UE_LOG(LogTemp, Warning, TEXT("BuildZoneGraphFromNvdbFile: '%s' missing 'nodes'/'segments' arrays"), *JsonFilePath);
			return false;
		}

		// JSON node "id"s are the Python NodeRegistry's own ids (real-NODE indices followed by
		// synthesized ones) and are NOT necessarily contiguous from 0 - only nodes actually
		// referenced by an in-bbox segment are emitted. Map id -> local array index.
		TMap<int32, int32> NodeIdToIndex;
		NodeIdToIndex.Reserve(NodesArray->Num());
		TArray<FZoneNodeView> Nodes;
		Nodes.SetNum(NodesArray->Num());
		for (int32 i = 0; i < NodesArray->Num(); ++i)
		{
			int32 JsonId = 0;
			(*NodesArray)[i]->AsObject()->TryGetNumberField(TEXT("id"), JsonId);
			NodeIdToIndex.Add(JsonId, i);
		}

		TArray<FZoneRoadView> Roads;
		Roads.Reserve(SegmentsArray->Num());
		for (int32 i = 0; i < SegmentsArray->Num(); ++i)
		{
			// RoadIndex must be Roads.Num() (the index this segment WOULD occupy), not the JSON
			// loop index i - a skipped segment must not leave later RoadRefs pointing at the wrong
			// (shifted) entry once failed segments are excluded from Roads.
			FString Error;
			if (!ParseSegment((*SegmentsArray)[i]->AsObject(), NodeIdToIndex, Roads.Num(), Roads, Nodes, Error))
			{
				UE_LOG(LogTemp, Warning, TEXT("BuildZoneGraphFromNvdbFile: skipping segment %d in '%s': %s"), i, *JsonFilePath, *Error);
			}
		}

		if (Roads.Num() == 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("BuildZoneGraphFromNvdbFile: '%s' produced zero usable road segments"), *JsonFilePath);
			return false;
		}

		BuildZoneGraphFromRoadNetwork(Roads, Nodes, FMatrix::Identity, OutData);
		return true;
	}
}
