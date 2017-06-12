using System;
using System.IO;
using UnrealBuildTool;

public class UE4HaxeExternGenerator : ModuleRules {
  public UE4HaxeExternGenerator(ReadOnlyTargetRules target) : base(target) {
    PublicIncludePaths.AddRange(
      new string[] {
        "Programs/UnrealHeaderTool/Public",
      }
    );
    PublicDependencyModuleNames.AddRange(
      new string[]
      {
        "Core",
        "CoreUObject",
      }
    );
  }
}
