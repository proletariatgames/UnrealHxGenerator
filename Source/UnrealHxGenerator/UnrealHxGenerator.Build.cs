using System;
using System.IO;
using UnrealBuildTool;

public class UnrealHxGenerator : ModuleRules {
  public UnrealHxGenerator(ReadOnlyTargetRules target) : base(target) {
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
