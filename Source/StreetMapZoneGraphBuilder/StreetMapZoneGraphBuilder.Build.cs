namespace UnrealBuildTool.Rules
{
    public class StreetMapZoneGraphBuilder : ModuleRules
    {
        public StreetMapZoneGraphBuilder(ReadOnlyTargetRules Target)
			: base(Target)
        {
            PublicDependencyModuleNames.AddRange(
                new string[] {
                    "Core",
                    "CoreUObject",
                    "Engine",
                    "StreetMapRuntime",
                    "ZoneGraph"
                }
            );

            PrivateDependencyModuleNames.AddRange(
                new string[] {
                    "UnrealEd",
                    "Json"
                }
            );
        }
    }
}
