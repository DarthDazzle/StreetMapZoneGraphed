#pragma once

#include "ZoneGraphBuilder.h"

/**
 * FZoneGraphBuilder::ConnectLanes is protected - it's meant to be called only from
 * FZoneGraphBuilder::Build() on registered UZoneShapeComponents. StreetMapToZoneShapeConverter
 * tessellates shapes procedurally from OSM road data instead (no UZoneShapeComponent involved),
 * but still needs to stitch the resulting lanes together across shapes, so this exposes the same
 * static function via inheritance + a using-declaration.
 */
struct FStreetMapZoneGraphBuilderAccess : public FZoneGraphBuilder
{
	using FZoneGraphBuilder::ConnectLanes;
};
