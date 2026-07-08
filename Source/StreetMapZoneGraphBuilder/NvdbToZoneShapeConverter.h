#pragma once

#include "CoreMinimal.h"

class AZoneGraphData;

namespace StreetMapZoneGraph
{
	/**
	 * Builds ZoneGraph zone/lane data directly from a Tools/nvdb/build_nvdb_network.py
	 * intermediate file ("vvtm-nvdbnetwork-v1"), bypassing UStreetMap entirely - see
	 * Tools/nvdb/DECISIONS.md for why. Returns false (OutData left untouched) if the file is
	 * missing or fails to parse the expected format.
	 */
	bool BuildZoneGraphFromNvdbFile(const FString& JsonFilePath, AZoneGraphData& OutData);
}
