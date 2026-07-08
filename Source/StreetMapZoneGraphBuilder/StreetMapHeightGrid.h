#pragma once

#include "CoreMinimal.h"

class UStreetMap;

/**
 * Loads a height grid produced by Tools/heightdata/fetch_height_grid.py (a sidecar
 * <name>.heightgrid.json + <name>.heightgrid.bin pair next to a UStreetMap's source .osm
 * file) and bilinearly samples elevation at a local-frame XY position - the same
 * tangent-plane-meters frame that FStreetMapRoad::RoadPoints are expressed in, relative to
 * UStreetMap::GetOriginLatitude()/GetOriginLongitude() (see StreetMapFactory.cpp's
 * ConvertLatLongToMetersRelative).
 *
 * Editor-only: relies on UStreetMap::GetSourceFilePath() (AssetImportData).
 */
class FStreetMapHeightGrid
{
public:
	/** Locates and loads the height-grid sidecar for StreetMap's source .osm file.
	    Returns false (leaving IsValid() == false) if no sidecar exists, it fails to parse,
	    or its recorded origin doesn't match StreetMap's within tolerance (stale grid from a
	    different import). Callers should treat false as "no elevation data available" and
	    fall back to flat Z, not as a hard error. */
	bool LoadForStreetMap(const UStreetMap& StreetMap);

	bool IsValid() const { return bIsValid; }

	/** Bilinearly samples height at a local-frame position in UE centimeters (RoadPoints
	    units), returning height in UE centimeters. Returns 0.0 if invalid or the sampled
	    cell(s) are nodata; out-of-bounds positions clamp to the grid edge. */
	double SampleHeightCm(const FVector2D& LocalPositionCm) const;

private:
	bool bIsValid = false;
	double BoundsMinXMeters = 0.0;
	double BoundsMinYMeters = 0.0;
	double CellSizeMeters = 1.0;
	int32 Cols = 0;
	int32 Rows = 0;
	float NodataValue = -9999.0f;
	TArray<float> Heights; // row-major, row 0 = min Y, meters
};
