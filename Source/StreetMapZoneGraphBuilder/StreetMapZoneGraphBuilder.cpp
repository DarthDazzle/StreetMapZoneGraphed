#include "StreetMapZoneGraphBuilder.h"
#include "Modules/ModuleManager.h"

class FStreetMapZoneGraphBuilderModule : public IModuleInterface
{
public:
	virtual void StartupModule() override {}
	virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE( FStreetMapZoneGraphBuilderModule, StreetMapZoneGraphBuilder )
