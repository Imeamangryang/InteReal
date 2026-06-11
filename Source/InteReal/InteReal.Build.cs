// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class InteReal : ModuleRules
{
	public InteReal(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "Json", "JsonUtilities", "Http",
			"DynamicMesh", "GeometryCore", "GeometryFramework", "GeometryScriptingCore", "UMG", "Slate", "SlateCore", "Niagara"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { "ProceduralMeshComponent" });

		PublicIncludePaths.AddRange(new string[]
		{
			"InteReal/EditMode",
			"InteReal/Harness"
		});
	}
}