#include "StreetMapHeightGrid.h"
#include "StreetMap.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	// Matches StreetMapFactory.cpp's OSMToCentimetersScaleFactor
	constexpr double CentimetersPerMeter = 100.0;

	// A height grid is considered aligned to a StreetMap if their recorded origin lat/lon
	// match within this tolerance (~1cm at these latitudes) - guards against sampling a
	// stale grid left over from a previous import of a different (or edited) .osm file.
	constexpr double OriginToleranceDegrees = 1e-7;
}

bool FStreetMapHeightGrid::LoadForStreetMap(const UStreetMap& StreetMap)
{
	bIsValid = false;

#if WITH_EDITORONLY_DATA
	const FString OsmPath = StreetMap.GetSourceFilePath();
	if (OsmPath.IsEmpty())
	{
		return false;
	}

	const FString BasePath = FPaths::GetPath(OsmPath) / FPaths::GetBaseFilename(OsmPath);
	const FString JsonPath = BasePath + TEXT(".heightgrid.json");
	const FString BinPath = BasePath + TEXT(".heightgrid.bin");

	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *JsonPath))
	{
		// No height grid alongside this .osm - not an error, just nothing to inject.
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("StreetMapHeightGrid: failed to parse %s"), *JsonPath);
		return false;
	}

	FString Format;
	if (!Root->TryGetStringField(TEXT("format"), Format) || Format != TEXT("vvtm-heightgrid-v1"))
	{
		UE_LOG(LogTemp, Warning, TEXT("StreetMapHeightGrid: unrecognized format in %s"), *JsonPath);
		return false;
	}

	double OriginLatitude = 0.0, OriginLongitude = 0.0;
	Root->TryGetNumberField(TEXT("origin_latitude"), OriginLatitude);
	Root->TryGetNumberField(TEXT("origin_longitude"), OriginLongitude);

	if (FMath::Abs(OriginLatitude - StreetMap.GetOriginLatitude()) > OriginToleranceDegrees ||
		FMath::Abs(OriginLongitude - StreetMap.GetOriginLongitude()) > OriginToleranceDegrees)
	{
		UE_LOG(LogTemp, Warning, TEXT("StreetMapHeightGrid: %s origin (%.7f, %.7f) doesn't match StreetMap's (%.7f, %.7f) - stale height grid? Ignoring."),
			*JsonPath, OriginLatitude, OriginLongitude, StreetMap.GetOriginLatitude(), StreetMap.GetOriginLongitude());
		return false;
	}

	double CellSize = 1.0, MinX = 0.0, MinY = 0.0, Nodata = -9999.0;
	int32 InCols = 0, InRows = 0;
	Root->TryGetNumberField(TEXT("cell_size"), CellSize);
	Root->TryGetNumberField(TEXT("bounds_min_x"), MinX);
	Root->TryGetNumberField(TEXT("bounds_min_y"), MinY);
	Root->TryGetNumberField(TEXT("nodata_value"), Nodata);
	Root->TryGetNumberField(TEXT("cols"), InCols);
	Root->TryGetNumberField(TEXT("rows"), InRows);

	if (InCols <= 0 || InRows <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("StreetMapHeightGrid: invalid grid dimensions in %s"), *JsonPath);
		return false;
	}

	TArray<uint8> RawBytes;
	if (!FFileHelper::LoadFileToArray(RawBytes, *BinPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("StreetMapHeightGrid: couldn't load %s"), *BinPath);
		return false;
	}

	const int64 ExpectedBytes = int64(InCols) * int64(InRows) * sizeof(float);
	if (int64(RawBytes.Num()) != ExpectedBytes)
	{
		UE_LOG(LogTemp, Warning, TEXT("StreetMapHeightGrid: %s size %d doesn't match expected %lld bytes for %dx%d grid"),
			*BinPath, RawBytes.Num(), ExpectedBytes, InCols, InRows);
		return false;
	}

	Heights.SetNumUninitialized(InCols * InRows);
	FMemory::Memcpy(Heights.GetData(), RawBytes.GetData(), RawBytes.Num());

	BoundsMinXMeters = MinX;
	BoundsMinYMeters = MinY;
	CellSizeMeters = CellSize;
	Cols = InCols;
	Rows = InRows;
	NodataValue = float(Nodata);
	bIsValid = true;
	return true;
#else
	return false;
#endif	// WITH_EDITORONLY_DATA
}

double FStreetMapHeightGrid::SampleHeightCm(const FVector2D& LocalPositionCm) const
{
	if (!bIsValid)
	{
		return 0.0;
	}

	const double XMeters = LocalPositionCm.X / CentimetersPerMeter;
	const double YMeters = LocalPositionCm.Y / CentimetersPerMeter;

	const double Col = (XMeters - BoundsMinXMeters) / CellSizeMeters;
	const double Row = (YMeters - BoundsMinYMeters) / CellSizeMeters;

	const int32 Col0 = FMath::Clamp(FMath::FloorToInt(Col), 0, Cols - 1);
	const int32 Row0 = FMath::Clamp(FMath::FloorToInt(Row), 0, Rows - 1);
	const int32 Col1 = FMath::Min(Col0 + 1, Cols - 1);
	const int32 Row1 = FMath::Min(Row0 + 1, Rows - 1);

	const double FracCol = FMath::Clamp(Col - Col0, 0.0, 1.0);
	const double FracRow = FMath::Clamp(Row - Row0, 0.0, 1.0);

	auto SampleAt = [this](int32 C, int32 R) -> float
	{
		return Heights[R * Cols + C];
	};

	const float V00 = SampleAt(Col0, Row0);
	const float V01 = SampleAt(Col1, Row0);
	const float V10 = SampleAt(Col0, Row1);
	const float V11 = SampleAt(Col1, Row1);

	if (V00 == NodataValue || V01 == NodataValue || V10 == NodataValue || V11 == NodataValue)
	{
		return 0.0;
	}

	const double Top = FMath::Lerp(double(V00), double(V01), FracCol);
	const double Bottom = FMath::Lerp(double(V10), double(V11), FracCol);
	const double HeightMeters = FMath::Lerp(Top, Bottom, FracRow);

	return HeightMeters * CentimetersPerMeter;
}
