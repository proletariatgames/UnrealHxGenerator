using System;
using System.IO;
using UnrealBuildTool;

public class HaxeExternGenerator : ModuleRules {
  public HaxeExternGenerator(TargetInfo target) {
    string modulePath = RulesCompiler.GetModuleFilename("HaxeExternGenerator");
    string baseDir = Path.GetFullPath(modulePath + "/../");
    // if (UnrealBuildTool.UnrealBuildTool.RunningRocket()) {
    //   Console.WriteLine("Running Rocket " + baseDir);
    //   // if we're doing a rocket build, use our own 'Deps' include path
    //   // since the rocket build won't contain that header
    //   // TODO: select the header based on version (when it actually change from version to version)
    //   PublicIncludePaths.AddRange(
    //     new string[] {
    //       baseDir + "/Deps",
    //     }
    //   );
    // } else {
    //   Console.WriteLine("Not Running Rocket");
      PublicIncludePaths.AddRange(
        new string[] {
          "Programs/UnrealHeaderTool/Public",
        }
      );
    // }
    PublicDependencyModuleNames.AddRange(
      new string[]
      {
        "Core",
        "CoreUObject",
      }
    );
  }
}
