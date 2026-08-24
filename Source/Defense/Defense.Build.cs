// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Defense : ModuleRules
{
	public Defense(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"NavigationSystem",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"LevelSequence",
			"MovieScene",
			"MovieSceneTracks",
			"GeometryCollectionEngine",
			"FieldSystemEngine",
			"UMG",
			"Slate",
			"GameplayTags",
			"Niagara",
			"SlateCore"
		});

		PublicIncludePaths.AddRange(new string[] {
			"Defense",
			"Defense/Variant_Platforming",
			"Defense/Variant_Platforming/Animation",
			"Defense/Variant_Combat",
			"Defense/Variant_Combat/AI",
			"Defense/Variant_Combat/Animation",
			"Defense/Variant_Combat/Gameplay",
			"Defense/Variant_Combat/Interfaces",
			"Defense/Variant_Combat/UI",
			"Defense/Variant_SideScrolling",
			"Defense/Variant_SideScrolling/AI",
			"Defense/Variant_SideScrolling/Gameplay",
			"Defense/Variant_SideScrolling/Interfaces",
			"Defense/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
