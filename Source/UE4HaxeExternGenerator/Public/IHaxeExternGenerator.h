#pragma once

#include "IScriptGeneratorPluginInterface.h"
#include "Modules/ModuleManager.h"
#include <cstdio>

class IHaxeExternGenerator : public IScriptGeneratorPluginInterface
{
public:
  static inline IHaxeExternGenerator& Get() {
    printf("Generator: GETTING HAXE EXTERN GENERATOR\n\n");
    return FModuleManager::LoadModuleChecked<IHaxeExternGenerator>("HaxeExternGenerator");
  }

  static inline bool IsAvailable() {
    printf("Generator: IS AVAILABLE HAXE EXTERN GENERATOR? %d\n\n", FModuleManager::Get().IsModuleLoaded("HaxeExternGenerator"));
    return FModuleManager::Get().IsModuleLoaded("HaxeExternGenerator");
  }
};
