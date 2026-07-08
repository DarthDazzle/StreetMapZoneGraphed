#include "StreetMapZoneGraphActions.h"
#include "StreetMapToZoneShapeConverter.h"
#include "NvdbToZoneShapeConverter.h"

#include "StreetMapActor.h"
#include "StreetMapComponent.h"
#include "StreetMap.h"
#include "ZoneGraphData.h"

bool UStreetMapZoneGraphActions::BuildZoneGraphFromStreetMapActor(AStreetMapActor* StreetMapActor, AZoneGraphData* TargetZoneGraphData)
{
	if (StreetMapActor == nullptr || TargetZoneGraphData == nullptr)
	{
		return false;
	}

	UStreetMapComponent* Component = StreetMapActor->GetStreetMapComponent();
	UStreetMap* StreetMap = Component ? Component->GetStreetMap() : nullptr;
	if (StreetMap == nullptr)
	{
		return false;
	}

	StreetMapZoneGraph::BuildZoneGraphFromStreetMap(*StreetMap, *TargetZoneGraphData);
	return true;
}

bool UStreetMapZoneGraphActions::BuildZoneGraphFromNvdbFile(const FString& NvdbJsonFilePath, AZoneGraphData* TargetZoneGraphData)
{
	if (TargetZoneGraphData == nullptr)
	{
		return false;
	}

	return StreetMapZoneGraph::BuildZoneGraphFromNvdbFile(NvdbJsonFilePath, *TargetZoneGraphData);
}
