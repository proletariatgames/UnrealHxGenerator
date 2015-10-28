#include "IHaxeExternGenerator.h"
#include <Features/IModularFeatures.h>
#include <CoreUObject.h>

DECLARE_LOG_CATEGORY_EXTERN(LogHaxeExtern, Log, All);
DEFINE_LOG_CATEGORY(LogHaxeExtern);


class FHaxeExternGenerator : public IHaxeExternGenerator {
  virtual void StartupModule() override {
    printf("Generator: STARTUP MODULE! REGISTERING\n\n");
    IModularFeatures::Get().RegisterModularFeature(TEXT("ScriptGenerator"), this);
  }

  virtual void ShutdownModule() override {
    printf("Generator: SHUTDOWN MODULE!\n\n");
    IModularFeatures::Get().UnregisterModularFeature(TEXT("ScriptGenerator"), this);
  }

  /** Name of module that is going to be compiling generated script glue */
  virtual FString GetGeneratedCodeModuleName() const override {
    UE_LOG(LogHaxeExtern,Log,TEXT("CODE MODULE NAME!"));
    // return TEXT("UE4HaxeExternGenerator");
    // return TEXT("CoreUObject");
    return TEXT("HaxeInit");
  }

  /** Returns true if this plugin supports exporting scripts for the specified target. This should handle game as well as editor target names */
  virtual bool SupportsTarget(const FString& TargetName) const override { 
    UE_LOG(LogHaxeExtern,Log,TEXT("SUPPORTS %s"), *TargetName);
    return true;
  }
  /** Returns true if this plugin supports exporting scripts for the specified module */
  virtual bool ShouldExportClassesForModule(const FString& ModuleName, EBuildModuleType::Type ModuleType, const FString& ModuleGeneratedIncludeDirectory) const override {
    UE_LOG(LogHaxeExtern,Log,TEXT("SHOULD EXPORT %s (inc %s)"), *ModuleName, *ModuleGeneratedIncludeDirectory);
    return true;
  }

  /** Initializes this plugin with build information */
  virtual void Initialize(const FString& RootLocalPath, const FString& RootBuildPath, const FString& OutputDirectory, const FString& IncludeBase) override {
    UE_LOG(LogHaxeExtern,Log,TEXT("INITIALIZE %s %s %s %s"), *RootLocalPath, *RootBuildPath, *OutputDirectory, *IncludeBase);
  }

  /** Exports a single class. May be called multiple times for the same class (as UHT processes the entire hierarchy inside modules. */
  virtual void ExportClass(class UClass* Class, const FString& SourceHeaderFilename, const FString& GeneratedHeaderFilename, bool bHasChanged) override {
    UE_LOG(LogHaxeExtern,Log,TEXT("EXPORT CLASS %s %s %s %s"), *(Class->GetDesc()), *SourceHeaderFilename, *GeneratedHeaderFilename, bHasChanged ? TEXT("CHANGED") : TEXT("NOT CHANGED"));
  }

  /** Called once all classes have been exported */
  virtual void FinishExport() override {
    UE_LOG(LogHaxeExtern,Log,TEXT("FINISH_EXPORT"));
  }

  /** Name of the generator plugin, mostly for debuggind purposes */
  virtual FString GetGeneratorName() const override {
    UE_LOG(LogHaxeExtern,Log,TEXT("GENERATOR NAME"));
    return TEXT("Haxe Extern Generator Plugin");
  }

};

IMPLEMENT_MODULE(FHaxeExternGenerator, UE4HaxeExternGenerator)
