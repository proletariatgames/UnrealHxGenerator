using System;
using System.IO;
using UnrealBuildTool;

public class UE4HaxeExternGenerator : ModuleRules {
  public UE4HaxeExternGenerator(TargetInfo target) {
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
