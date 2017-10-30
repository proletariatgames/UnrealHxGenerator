#pragma once
#include <CoreUObject.h>
DECLARE_LOG_CATEGORY_EXTERN(LogHaxeExtern, Log, All);

// unfortunately we need to define the log as Log since UBT makes UHT ignore all logs that are not warnings
#if false
#define LOG(str,...) UE_LOG(LogHaxeExtern, Log, TEXT(str), __VA_ARGS__)
#else
#define LOG(str,...)
#endif

#include "../Launch/Resources/Version.h"
#ifndef ENGINE_MINOR_VERSION
#error "Version not found"
#endif

#define UE_VER (ENGINE_MAJOR_VERSION * 100 + ENGINE_MINOR_VERSION)

enum class ETypeKind {
  KNone,
  KUObject,
  KUInterface,
  KUStruct,
  KUEnum,
  KUDelegate
};

struct FHaxeTypeRef {
  TArray<FString> pack;
  FString name;
  FString haxeModule;
  FString uname;
  bool haxeGenerated;
  const ETypeKind kind;
  const FString module;

  FHaxeTypeRef(const TArray<FString> inPack, const FString inName, ETypeKind inKind, const FString inModule) :
    pack(inPack),
    name(inName),
    uname(inName),
    haxeGenerated(false),
    kind(inKind),
    module(inModule)
  {
  }

  FHaxeTypeRef(const FString inName, ETypeKind inKind) :
    name(inName),
    uname(inName),
    haxeGenerated(false),
    kind(inKind),
    module(FString())
  {
  }

  FString toString() const {
    if (this->pack.Num() == 0) {
      return this->name;
    }

    if (this->haxeModule.IsEmpty()) {
      return FString::Join(this->pack, TEXT(".")) + TEXT(".") + this->name;
    } else {
      return FString::Join(this->pack, TEXT(".")) + TEXT(".") + this->haxeModule + TEXT(".") + this->name;
    }
  }
};

struct HaxeTypeHelpers {

  static void replaceHaxeType(UField *inField, FHaxeTypeRef& outRef) {
    if (inField != nullptr) {
      FString hxClass = inField->GetMetaData(TEXT("HaxeStaticClass"));
      if (!hxClass.IsEmpty()) {
        outRef.haxeGenerated = true;
        TArray<FString> fullName;
        hxClass.ParseIntoArray(fullName,TEXT("."),false);
        outRef.name = fullName.Pop();
        outRef.pack = fullName;

        FString hxModule = inField->GetMetaData(TEXT("HaxeModule"));
        if (!hxModule.IsEmpty()) {
          outRef.haxeModule = hxModule;
        }
      }
    }
  }

  static const TArray<FString> getHaxePackage(UPackage *inPack, FString& outModule) {
    static const TCHAR *CoreUObject = TEXT("/Script/CoreUObject");
    static const TCHAR *Engine = TEXT("/Script/Engine");
    static const TCHAR *UnrealEd = TEXT("/Script/UnrealEd");
    static bool isCompilingGameCode = compilingGameCode();
    if (inPack->GetName() == CoreUObject || inPack->GetName() == Engine) {
      static TArray<FString> ret;
      if (ret.Num() == 0)
        ret.Push(FString("unreal"));
      return ret;
    } else if (inPack->GetName() == UnrealEd) {
      static TArray<FString> ret;
      if (ret.Num() == 0) {
        ret.Push(FString("unreal"));
        ret.Push(FString("editor"));
      }
      outModule = FString(TEXT("UnrealEd"));
      return ret;
    }
    outModule = inPack->GetName().RightChop( sizeof("/Script") );
    TArray<FString> ret;
    if (!isCompilingGameCode || !shouldCompileModule(*outModule)) {
      ret.Push("unreal");
    }
    ret.Push(outModule.ToLower());
    return ret;
  }

  static bool shouldCompileModule(const FString& name) {
    static TArray<FString> targetsToCompile(getTargetsToCompile());
    if (targetsToCompile.Num() != 0 && targetsToCompile.Find(name) < 0) {
      return false;
    }
    return name != FString(TEXT("HaxeInit"));
  }

  static bool compilingGameCode() {
    static TArray<FString> targetsToCompile(getTargetsToCompile());
    return (targetsToCompile.Num() > 0);
  }

private:
  static TArray<FString> getTargetsToCompile() {
    static TCHAR env[256];
    FPlatformMisc::GetEnvironmentVariable(TEXT("EXTERN_MODULES"), env, 255);
    TArray<FString> ret;
    FString(env).ParseIntoArray(ret,TEXT(","),true);
    return ret;
  }

};

struct ClassDescriptor {
  UClass *uclass;
  FString header;
  const FHaxeTypeRef haxeType;

  ClassDescriptor(UClass *inUClass, const FString &inHeader) :
    uclass(inUClass),
    header(inHeader),
    haxeType(getHaxeType(inUClass))
  {
  }

private:
  FHaxeTypeRef getHaxeType(UClass *inUClass) {
    auto pack = inUClass->GetOuterUPackage();
    auto prefix = inUClass->GetPrefixCPP();
    auto isInterface = inUClass->HasAnyClassFlags(CLASS_Interface);
    if (isInterface) {
      prefix = TEXT("I");
    }
    FString module;
    auto haxePack = HaxeTypeHelpers::getHaxePackage(pack, module);
    FHaxeTypeRef ret(
      haxePack,
      prefix + inUClass->GetName(),
      isInterface ? ETypeKind::KUInterface : ETypeKind::KUObject,
      module);
    HaxeTypeHelpers::replaceHaxeType(inUClass, ret);
    return ret;
  }
};

class ModuleDescriptor {
private:
  TArray<ClassDescriptor *> m_classes;
  UPackage *m_module;

public:
  TSet<FString> headers;
  TArray<FString> headerOrder;
  FString moduleName;

  ModuleDescriptor(UPackage *inPackage) :
    m_module(inPackage)
  {
  }

  void touch(ClassDescriptor *inClass, FString inModuleName) {
    this->m_classes.Push(inClass);
    if (!this->headers.Contains(inClass->header)) {
      this->headers.Add(inClass->header);
      this->headerOrder.Push(inClass->header);
    }

    if (this->moduleName.IsEmpty())
      this->moduleName = inModuleName;
  }

  UPackage *getPackage() const {
    return m_module;
  }
};

struct NonClassDescriptor {
  TSet<const ClassDescriptor *> sameModuleRefs;
  TSet<const ClassDescriptor *> otherModuleRefs;
  const FHaxeTypeRef haxeType;
  const ModuleDescriptor *module;
  FString moduleSourcePath;

  bool addRef(const ClassDescriptor *cls) {
    bool unused;
    UPackage *pack = cls->uclass->GetOuterUPackage();
    if (pack == module->getPackage()) {
      this->sameModuleRefs.Add(cls, &unused);
      return true;
    } else {
      this->otherModuleRefs.Add(cls, &unused);
      return false;
    }
    // silly c++
    return false;
  }

  TArray<FString> getHeaders() const {
    TArray<FString> ret;
    if (module->moduleName == TEXT("UMG")) {
      ret.Push(TEXT("UMG.h"));
    }

    if (!moduleSourcePath.IsEmpty()) {
      ret.Push(moduleSourcePath);
      return ret;
    }

    for (auto m : sameModuleRefs) {
      ret.Push(m->header);
      return ret;
    }
    for (auto m : otherModuleRefs) {
      ret.Push(m->header);
      return ret;
    }

    static const TCHAR *Engine = TEXT("/Script/Engine");
    if (module->getPackage()->GetName() == Engine) {
      ret.Push(TEXT("Engine.h"));
      return ret;
    }

    for (auto header : module->headers) {
      ret.Push(header);
    }
    ret.Sort();

    return ret;
  }

protected:
  NonClassDescriptor(FHaxeTypeRef inName, ModuleDescriptor *inModule, UField *inField) :
    haxeType(inName),
    module(inModule),
    moduleSourcePath(inField->GetMetaData(TEXT("ModuleRelativePath")))
  {
    if (moduleSourcePath.IsEmpty()) {
      this->moduleSourcePath = inField->GetMetaData(TEXT("IncludePath"));
    }
  }
};

struct EnumDescriptor : public NonClassDescriptor {
  UEnum *uenum;

  EnumDescriptor (UEnum *inUEnum, ModuleDescriptor *inModule) :
    NonClassDescriptor(getHaxeType(inUEnum), inModule, inUEnum),
    uenum(inUEnum)
  {
  }

private:
  static FHaxeTypeRef getHaxeType(UEnum *inEnum) {
    auto pack = inEnum->GetOutermost();
    auto name = inEnum->CppType;
    TArray<FString> packArr;
    name.ParseIntoArray(packArr, TEXT("::"), true);
    switch (inEnum->GetCppForm()) {
    case UEnum::ECppForm::Namespaced:
      packArr.Pop(false);
    default: {}
    }
    if (packArr.Num() > 1) {
      for (int i = 0; i < packArr.Num() - 1; i++) {
        packArr[i] = packArr[i].ToLower();
      }
    }

    FString module;
    auto newPack = TArray<FString>(HaxeTypeHelpers::getHaxePackage(pack, module));
    auto hxName = packArr.Pop( false );
    for (auto& packPart : packArr) {
      newPack.Push(packPart);
    }
    FHaxeTypeRef ret(
      newPack,
      hxName,
      ETypeKind::KUEnum,
      module);
    HaxeTypeHelpers::replaceHaxeType(inEnum, ret);
    return ret;
  }
};

struct StructDescriptor : public NonClassDescriptor {
  UScriptStruct *ustruct;

  StructDescriptor(UScriptStruct *inUStruct, ModuleDescriptor *inModule) :
    NonClassDescriptor(getHaxeType(inUStruct), inModule, inUStruct),
    ustruct(inUStruct)
  {
  }

private:
  static FHaxeTypeRef getHaxeType(UStruct *inStruct) {
    auto pack = inStruct->GetOutermost();
    FString module;
    auto haxePack = HaxeTypeHelpers::getHaxePackage(pack, module);
    FHaxeTypeRef ret(
      haxePack,
      inStruct->GetPrefixCPP() + inStruct->GetName(),
      ETypeKind::KUStruct,
      module);
    HaxeTypeHelpers::replaceHaxeType(inStruct, ret);
    return ret;
  }
};

struct DelegateDescriptor : public NonClassDescriptor {
  UFunction *delegateSignature;

  DelegateDescriptor(UFunction *inDelegate, ModuleDescriptor *inModule) :
    NonClassDescriptor(getHaxeType(inDelegate), inModule, inDelegate),
    delegateSignature(inDelegate)
  {
  }

private:
  static FHaxeTypeRef getHaxeType(UFunction *inFunction) {
    auto pack = inFunction->GetOutermost();
    FString module;
    auto haxePack = HaxeTypeHelpers::getHaxePackage(pack, module);
    auto name = inFunction->GetName();
    if (!name.EndsWith(TEXT("__DelegateSignature"))) {
      UE_LOG(LogHaxeExtern, Fatal, TEXT("Bad delegate signature %s - it doesn't contain __DelegateSignature"), *name);
    }
    name = name.LeftChop(sizeof("__DelegateSignature") - 1);
    FHaxeTypeRef ret(
      haxePack,
      inFunction->GetPrefixCPP() + name,
      ETypeKind::KUDelegate,
      module);
    HaxeTypeHelpers::replaceHaxeType(inFunction, ret);
    return ret;
  }
};

class FHaxeTypes {
private:
  TMap<FString, ClassDescriptor *> m_classes;
  TMap<FString, EnumDescriptor *> m_enums;
  TMap<FString, StructDescriptor *> m_structs;
  TMap<FString, DelegateDescriptor *> m_delegates;

  TMap<UPackage *, ModuleDescriptor *> m_upackageToModule;

  const static FHaxeTypeRef nulltype;

  FString m_outPath;

  void deleteFileIfExists(FHaxeTypeRef haxeType) {
    auto outPath = this->m_outPath / FString::Join(haxeType.pack, TEXT("/")) / haxeType.name + TEXT(".hx");
    if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*outPath)) {
      LOG("Deleting previously generated file %s", *outPath);
      FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*outPath);
    }
  }

public:
  FHaxeTypes(FString inOutPath) : m_outPath(inOutPath)
  {
  }

  FHaxeTypes() {}

  void touchClass(UClass *inClass, const FString &inHeader, const FString &inModule) {
    if (m_classes.Contains(inClass->GetName())) {
      return; // we've already touched this type; probably it's UObject which gets added every time (!)
    }
    if (inClass->HasMetaData(TEXT("UHX_Internal"))) {
      // internal class, shouldn't be exported
      return;
    }
    FString header;
    if (inModule == TEXT("UMG")) {
      static const FString umgHeader = TEXT("UMG.h");
      header = umgHeader;
    } else {
      header = inHeader;
    }
    ClassDescriptor *cls = new ClassDescriptor(inClass, header);
    m_classes.Add(inClass->GetName(), cls);
    LOG("Class name %s", *cls->haxeType.toString());
    auto module = getModule(inClass->GetOuterUPackage());
    module->touch(cls, inModule);

    // examine all properties and see if any of them uses a struct or enum in a way that would need to include the actual file
    TFieldIterator<UProperty> props(inClass, EFieldIteratorFlags::ExcludeSuper);
    for (; props; ++props) {
      UProperty *prop = *props;
      touchProperty(prop, cls, false);
    }

    TFieldIterator<UFunction> funcs(inClass, EFieldIteratorFlags::ExcludeSuper);
    for (; funcs; ++funcs) {
      auto func = *funcs;
      for (TFieldIterator<UProperty> args(func); args; ++args) {
        auto arg = *args;
        // nullptr because the type can be forward declared here
        touchProperty(arg, cls, true);
      }
    }
  }

  ModuleDescriptor *getModule(UPackage *inPackage) {
    if (m_upackageToModule.Contains(inPackage)) {
      return m_upackageToModule[inPackage];
    }
    auto module = new ModuleDescriptor(inPackage);
    m_upackageToModule.Add(inPackage, module);
    return module;
  }

  void touchProperty(UProperty *inProp, ClassDescriptor *inClass, bool inMayForward) {
    // see UnrealType.h for all possible variations
    if (inProp->IsA<UStructProperty>()) {
      auto structProp = Cast<UStructProperty>(inProp);
      if (!inMayForward && !structProp->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm | CPF_ReferenceParm)) {
        this->touchStruct(structProp->Struct, inClass);
      } else {
        this->touchStruct(structProp->Struct, nullptr);
      }
    } else if (inProp->IsA<UNumericProperty>()) {
      auto numeric = Cast<UNumericProperty>(inProp);
      UEnum *uenum = numeric->GetIntPropertyEnum();
      if (nullptr != uenum) {
        // is enum
        this->touchEnum(uenum, inClass);
      }
#if UE_VER >= 416
    } else if (inProp->IsA<UEnumProperty>()) {
      auto enumProp = Cast<UEnumProperty>(inProp);
      UEnum *uenum = enumProp->GetEnum();
      if (nullptr != uenum) {
        // is enum
        this->touchEnum(uenum, inClass);
      }
#endif
    } else if (inProp->IsA<UArrayProperty>()) {
      auto prop = Cast<UArrayProperty>(inProp);
      touchProperty(prop->Inner, inClass, inMayForward);
    } else if (inProp->IsA<UDelegateProperty>()) {
      auto prop = Cast<UDelegateProperty>(inProp);
      touchDelegate(prop->SignatureFunction, inClass);
    } else if (inProp->IsA<UMulticastDelegateProperty>()) {
      auto prop = Cast<UMulticastDelegateProperty>(inProp);
      touchDelegate(prop->SignatureFunction, inClass);
    }
  }

  /**
   * add a reference from the class `inClass` to struct `inStruct`
   * this reference allows us to be sure that we can include a header that includes the definition
   * to the struct. We need that because the current UHT exporter interface doesn't pass the headers
   * of structs and enums to us. So we need to ignore structs/enums that we aren't sure to have a header
   * that has included its entire definition
   **/
  void touchStruct(UScriptStruct *inStruct, ClassDescriptor *inClass) {
    if (inStruct->HasMetaData(TEXT("UHX_Internal"))) {
      // internal class, shouldn't be exported
      return;
    }
    auto name = inStruct->GetName();
    if (!m_structs.Contains(name)) {
      m_structs.Add(name, new StructDescriptor(inStruct, this->getModule(inStruct->GetOutermost())));
    }
    auto descr = m_structs[name];
    if (inClass != nullptr)
      descr->addRef(inClass);

    auto super = inStruct->GetSuperStruct();
    while (super != nullptr) {
      if (super->IsA<UScriptStruct>()) {
        this->touchStruct(Cast<UScriptStruct>(super), inClass);
      }
      super = super->GetSuperStruct();
    }

    TFieldIterator<UProperty> props(inStruct, EFieldIteratorFlags::ExcludeSuper);
    for (; props; ++props) {
      UProperty *prop = *props;
      touchProperty(prop, inClass, false);
    }
  }

  /**
   * add a reference from the class `inClass` to enum `inEnum`
   * @see `touchStruct`
   **/
  void touchEnum(UEnum *inEnum, ClassDescriptor *inClass) {
    if (inEnum->HasMetaData(TEXT("UHX_Internal"))) {
      // internal class, shouldn't be exported
      return;
    }
    auto name = inEnum->GetName();
    if (!m_enums.Contains(name)) {
      m_enums.Add(name, new EnumDescriptor(inEnum, this->getModule(inEnum->GetOutermost())));
    }
    auto descr = m_enums[name];
    LOG("Haxe enum name: %s", *descr->haxeType.toString());
    if (inClass != nullptr)
      descr->addRef(inClass);
  }

  /**
   * add a reference from the class `inClass` to delegate `inDelegate`
   * this reference allows us to be sure that we can include a header that includes the definition
   * to the struct. We need that because the current UHT exporter interface doesn't pass the headers
   * of structs and enums to us. So we need to ignore structs/enums that we aren't sure to have a header
   * that has included its entire definition
   **/
  void touchDelegate(UFunction *inDelegate, ClassDescriptor *inClass) {
    if (inDelegate->HasMetaData(TEXT("UHX_Internal"))) {
      // internal class, shouldn't be exported
      return;
    }
    auto name = inDelegate->GetName();
    if ( (inDelegate->FunctionFlags & FUNC_Delegate) == 0) {
      UE_LOG(LogHaxeExtern, Warning, TEXT("Delegate %s does not have the delegate flag set"), *name);
      return;
    }
    if (!name.EndsWith(TEXT("__DelegateSignature"))) {
      UE_LOG(LogHaxeExtern, Warning, TEXT("Delegate %s's name doesn't contain __DelegateSignature"), *name);
      return;
    }
    if (!m_delegates.Contains(name)) {
      m_delegates.Add(name, new DelegateDescriptor(inDelegate, this->getModule(inDelegate->GetOutermost())));
    }
    auto descr = m_delegates[name];
    if (inClass != nullptr) {
      descr->addRef(inClass);
    }

    TFieldIterator<UProperty> props(inDelegate, EFieldIteratorFlags::ExcludeSuper);
    for (; props; ++props) {
      UProperty *prop = *props;
      touchProperty(prop, inClass, false);
    }
  }

  ///////////////////////////////////////////////////////
  // Haxe Type handling
  ///////////////////////////////////////////////////////

  const FHaxeTypeRef& toHaxeType(UClass *inClass) {
    FString name = inClass->GetName();
    if (!m_classes.Contains(name)) {
      return nulltype;
    }

    auto cls = m_classes[name];
    return cls->haxeType;
  }

  const FHaxeTypeRef& toHaxeType(UEnum *inEnum) {
    FString name = inEnum->GetName();
    if (!m_enums.Contains(name)) {
      return nulltype;
    }

    auto e = m_enums[name];
    return e->haxeType;
  }

  static bool isBadType(const FString& inTypeName) {
    static const FString materialInput = TEXT("MaterialInput");
    return inTypeName == materialInput;
  }

  const FHaxeTypeRef& toHaxeType(UScriptStruct *inStruct) {
    FString name = inStruct->GetName();
    // deal with some types that can't be generated
    if (isBadType(name)) {
      return nulltype;
    }

    if (!m_structs.Contains(name)) {
      return nulltype;
    }

    auto s = m_structs[name];
    return s->haxeType;
  }

  const ClassDescriptor *getDescriptor(UClass *inClass) {
    if (inClass == nullptr) return nullptr;
    FString name = inClass->GetName();
    if (!m_classes.Contains(name)) {
      return nullptr;
    }

    return m_classes[name];
  }

  const EnumDescriptor *getDescriptor(UEnum *inEnum) {
    if (inEnum == nullptr) return nullptr;
    FString name = inEnum->GetName();
    if (!m_enums.Contains(name)) {
      return nullptr;
    }

    return m_enums[name];
  }

  const StructDescriptor *getDescriptor(UScriptStruct *inStruct) {
    if (inStruct == nullptr) return nullptr;
    FString name = inStruct->GetName();
    if (isBadType(name)) {
      return nullptr;
    }

    if (!m_structs.Contains(name)) {
      return nullptr;
    }

    return m_structs[name];
  }

  const DelegateDescriptor *getDescriptor(UFunction *inFunction) {
    if (inFunction == nullptr) return nullptr;
    FString name = inFunction->GetName();
    if (isBadType(name)) {
      return nullptr;
    }
    if (!m_delegates.Contains(name)) {
      return nullptr;
    }

    return m_delegates[name];
  }

  TArray<const ModuleDescriptor *> getAllModules() {
    TArray<const ModuleDescriptor *> ret;
    for (auto& elem : m_upackageToModule) {
      ret.Add(elem.Value);
    }
    return ret;
  }

  TArray<const ClassDescriptor *> getAllClasses() {
    TArray<const ClassDescriptor *> ret;
    for (auto& elem : m_classes) {
      ret.Add(elem.Value);
    }
    return ret;
  }

  TArray<const EnumDescriptor *> getAllEnums() {
    TArray<const EnumDescriptor *> ret;
    for (auto& elem : m_enums) {
      ret.Add(elem.Value);
    }
    return ret;
  }

  TArray<const StructDescriptor *> getAllStructs() {
    TArray<const StructDescriptor *> ret;
    for (auto& elem : m_structs) {
      if (!isBadType(elem.Key)) {
        ret.Add(elem.Value);
      }
    }
    return ret;
  }

  TArray<const DelegateDescriptor *> getAllDelegates() {
    TArray<const DelegateDescriptor *> ret;
    for (auto& elem : m_delegates) {
      if (!isBadType(elem.Key)) {
        ret.Add(elem.Value);
      }
    }
    return ret;
  }

  void doNotExportDelegate(const DelegateDescriptor *inDelegate) {
    m_delegates.Remove(inDelegate->delegateSignature->GetName());
    delete inDelegate;
  }

  ~FHaxeTypes() {
    for (auto& elem : m_enums) {
      delete elem.Value;
    }
    for (auto& elem : m_structs) {
      delete elem.Value;
    }
    for (auto& elem : m_classes) {
      delete elem.Value;
    }
    for (auto& elem : m_upackageToModule) {
      delete elem.Value;
    }
  }
};
