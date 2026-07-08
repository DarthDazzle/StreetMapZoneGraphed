#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "StreetMapZoneGraphActions.generated.h"

class AStreetMapActor;
class AZoneGraphData;

UCLASS()
class STREETMAPZONEGRAPHBUILDER_API UStreetMapZoneGraphActions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Bakes ZoneGraph lane data from StreetMapActor's OSM road graph into TargetZoneGraphData, in
	 * place - a manual step, mirroring ZoneGraph's own "Build ZoneGraph" convention. Does not touch
	 * StreetMapActor's existing mesh/rendering.
	 */
	UFUNCTION(BlueprintCallable, Category = "StreetMap|ZoneGraph", CallInEditor)
	static bool BuildZoneGraphFromStreetMapActor(AStreetMapActor* StreetMapActor, AZoneGraphData* TargetZoneGraphData);

	/**
	 * Bakes ZoneGraph lane data from a Tools/nvdb/build_nvdb_network.py intermediate file directly
	 * into TargetZoneGraphData, in place - bypasses UStreetMap/AStreetMapActor entirely. See
	 * Tools/nvdb/DECISIONS.md.
	 */
	UFUNCTION(BlueprintCallable, Category = "StreetMap|ZoneGraph", CallInEditor)
	static bool BuildZoneGraphFromNvdbFile(const FString& NvdbJsonFilePath, AZoneGraphData* TargetZoneGraphData);
};
