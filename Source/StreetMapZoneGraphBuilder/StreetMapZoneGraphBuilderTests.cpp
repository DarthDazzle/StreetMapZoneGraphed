#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "StreetMapZoneGraphActions.h"
#include "StreetMapActor.h"
#include "ZoneGraphData.h"
#include "EngineUtils.h"
#include "Editor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStreetMapZoneGraphBuildTest, "StreetMap.ZoneGraph.BuildFromLevelActors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FStreetMapZoneGraphBuildTest::RunTest(const FString& Parameters)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("Editor world"), World))
	{
		return false;
	}

	AStreetMapActor* StreetMapActor = nullptr;
	for (TActorIterator<AStreetMapActor> It(World); It; ++It)
	{
		StreetMapActor = *It;
		break;
	}

	AZoneGraphData* ZoneGraphData = nullptr;
	for (TActorIterator<AZoneGraphData> It(World); It; ++It)
	{
		ZoneGraphData = *It;
		break;
	}

	if (!TestNotNull(TEXT("StreetMapActor in level"), StreetMapActor)
		|| !TestNotNull(TEXT("AZoneGraphData in level"), ZoneGraphData))
	{
		return false;
	}

	const bool bBuilt = UStreetMapZoneGraphActions::BuildZoneGraphFromStreetMapActor(StreetMapActor, ZoneGraphData);
	TestTrue(TEXT("BuildZoneGraphFromStreetMapActor returned true"), bBuilt);

	const FZoneGraphStorage& Storage = ZoneGraphData->GetStorage();
	AddInfo(FString::Printf(TEXT("Zones=%d Lanes=%d LanePoints=%d BoundaryPoints=%d"),
		Storage.Zones.Num(), Storage.Lanes.Num(), Storage.LanePoints.Num(), Storage.BoundaryPoints.Num()));

	TestTrue(TEXT("At least one zone was generated"), Storage.Zones.Num() > 0);
	TestTrue(TEXT("At least one lane was generated"), Storage.Lanes.Num() > 0);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
