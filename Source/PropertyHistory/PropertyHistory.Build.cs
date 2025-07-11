// Copyright Voxel Plugin SAS. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class PropertyHistory : ModuleRules
{
	public PropertyHistory(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bUseUnity = false;

		PrivateIncludePaths.Add(Path.Combine(GetModuleDirectory("PropertyEditor"), "Private"));

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Slate",
			"SlateCore",
			"InputCore",
			"UnrealEd",
			"ToolMenus",
			"SceneOutliner",
			"MaterialEditor",
			"PropertyEditor",
			"SourceControl",
			"StructUtilsEditor",
			"SourceControlWindows",
			"WorkspaceMenuStructure",
		});
	}
}