#include "IHaxeExternGenerator.h"
#include <Features/IModularFeatures.h>
#include <CoreUObject.h>
#include "HaxeGenerator.h"
#include "HaxeTypes.h"

DEFINE_LOG_CATEGORY(LogHaxeExtern);

static const FName NAME_ToolTip(TEXT("ToolTip"));

class FHaxeExternGenerator : public IHaxeExternGenerator {
protected:
  FString m_pluginPath;
  FHaxeTypes m_types;
  static FString currentModule;
public:

  virtual void StartupModule() override {
    IModularFeatures::Get().RegisterModularFeature(TEXT("ScriptGenerator"), this);
  }

  virtual void ShutdownModule() override {
    IModularFeatures::Get().UnregisterModularFeature(TEXT("ScriptGenerator"), this);
  }

  /** Name of module that is going to be compiling generated script glue */
  virtual FString GetGeneratedCodeModuleName() const override {
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
    currentModule = ModuleName;
    return true;
  }

  /** Initializes this plugin with build information */
  virtual void Initialize(const FString& RootLocalPath, const FString& RootBuildPath, const FString& OutputDirectory, const FString& IncludeBase) override {
    UE_LOG(LogHaxeExtern,Log,TEXT("INITIALIZE %s %s %s %s"), *RootLocalPath, *RootBuildPath, *OutputDirectory, *IncludeBase);
    this->m_pluginPath = IncludeBase + TEXT("/../../");
  }

  /** Exports a single class. May be called multiple times for the same class (as UHT processes the entire hierarchy inside modules. */
  virtual void ExportClass(class UClass* Class, const FString& SourceHeaderFilename, const FString& GeneratedHeaderFilename, bool bHasChanged) override {
    UE_LOG(LogHaxeExtern,Log,TEXT("EXPORT CLASS %s %s %s %s"), *(Class->GetDesc()), *SourceHeaderFilename, *GeneratedHeaderFilename, bHasChanged ? TEXT("CHANGED") : TEXT("NOT CHANGED"));
    FString comment = Class->GetMetaData(NAME_ToolTip);
    m_types.touchClass(Class, SourceHeaderFilename, currentModule);
  }

  /** Called once all classes have been exported */
  virtual void FinishExport() override {
    UE_LOG(LogHaxeExtern,Log,TEXT("FINISH_EXPORT 1"));
    // now start generating
    for (auto& cls : m_types.getAllClasses()) {
      UE_LOG(LogHaxeExtern,Log,TEXT("got %s"), *cls->uclass->GetName())
      auto gen = FHaxeGenerator(this->m_types, this->m_pluginPath);
      gen.generateClass(cls);
    }

    UE_LOG(LogHaxeExtern,Log,TEXT("STARTING USTRUCTS"));
    for (auto& s : m_types.getAllStructs()) {
      UE_LOG(LogHaxeExtern,Log,TEXT("BEGIN"));
      UE_LOG(LogHaxeExtern,Log,TEXT("got %s"), *s->ustruct->GetName())
      auto gen = FHaxeGenerator(this->m_types, this->m_pluginPath);
      gen.generateStruct(s);
    }
  }

  /** Name of the generator plugin, mostly for debuggind purposes */
  virtual FString GetGeneratorName() const override {
    return TEXT("Haxe Extern Generator Plugin");
  }

};

IMPLEMENT_MODULE(FHaxeExternGenerator, UE4HaxeExternGenerator)

FString FHaxeExternGenerator::currentModule = FString();
