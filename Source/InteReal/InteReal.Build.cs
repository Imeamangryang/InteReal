// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class InteReal : ModuleRules
{
	public InteReal(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "Json", "JsonUtilities","DynamicMesh", "GeometryCore", "GeometryFramework", "GeometryScriptingCore", "UMG", "Slate", "SlateCore"});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[]
		{
			"InteReal/EditMode",
			"InteReal/Harness"
		});
	}
}
