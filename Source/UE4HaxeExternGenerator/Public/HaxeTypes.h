#pragma once
#include <CoreUObject.h>
DECLARE_LOG_CATEGORY_EXTERN(LogHaxeExtern, Log, All);

#define LOG(str,...) UE_LOG(LogHaxeExtern, Log, TEXT(str), __VA_ARGS__)

enum class ETypeKind {
  KNone,
  KUObject,
  KUInterface,
  KUStruct,
  KUEnum,
  KUDelegate
};

struct FHaxeTypeRef {
  const TArray<FString> pack;
  const FString name;
  const ETypeKind kind;
  const FString module;

  FHaxeTypeRef(const TArray<FString> inPack, const FString inName, ETypeKind inKind, const FString inModule) :
    pack(inPack),
    name(inName),
    kind(inKind),
    module(inModule)
  {
  }

  FHaxeTypeRef(const FString inName, ETypeKind inKind) :
    name(inName),
    kind(inKind),
    module(FString())
  {
  }

  FString toString() const {
    if (this->pack.Num() == 0) {
      return this->name;
    }

    return FString::Join(this->pack, TEXT(".")) + TEXT(".") + this->name;
  }
};

static const TArray<FString> getHaxePackage(UPackage *inPack, FString * outModule) {
  static const TCHAR *CoreUObject = TEXT("/Script/CoreUObject");
  static const TCHAR *Engine = TEXT("/Script/Engine");
  static const TCHAR *UnrealEd = TEXT("/Script/UnrealEd");
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
    *outModule = FString(TEXT("UnrealEd"));
    return ret;
  }
  *outModule = inPack->GetName().RightChop( sizeof("/Script") );
  TArray<FString> ret;
  ret.Push("unreal");
  ret.Push((*outModule).ToLower());
  return ret;
}

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
    return FHaxeTypeRef(
      getHaxePackage(pack, &module),
      prefix + inUClass->GetName(),
      isInterface ? ETypeKind::KUInterface : ETypeKind::KUObject,
      module);
  }
};

class ModuleDescriptor {
private:
  TArray<ClassDescriptor *> m_classes;
  UPackage *m_module;

public:
  TSet<FString> headers;
  FString moduleName;

  ModuleDescriptor(UPackage *inPackage) :
    m_module(inPackage)
  {
  }

  void touch(ClassDescriptor *inClass, FString inModuleName) {
    this->m_classes.Push(inClass);
    this->headers.Add(inClass->header);

    if (this->moduleName.IsEmpty())
      this->moduleName = inModuleName;
  }

  UPackage *getPackage() {
    return m_module;
  }
};

struct NonClassDescriptor {
  TSet<const ClassDescriptor *> sameModuleRefs;
  TSet<const ClassDescriptor *> otherModuleRefs;
  const FHaxeTypeRef haxeType;
  const ModuleDescriptor *module;

  bool addRef(const ClassDescriptor *cls) {
    bool unused;
    UPackage *pack = cls->uclass->GetOuterUPackage();
    if (pack == m_thisPackage) {
      this->sameModuleRefs.Add(cls, &unused);
      return true;
    } else {
      this->otherModuleRefs.Add(cls, &unused);
      return false;
    }
    // silly c++
    return false;
  }

  FString getHeader() const {
    for (auto m : sameModuleRefs) {
      return m->header;
    }
    for (auto m : otherModuleRefs) {
      return m->header;
    }

    check(false);
    return FString();
  }

protected:
  const UPackage *m_thisPackage;
  NonClassDescriptor(FHaxeTypeRef inName, ModuleDescriptor *inModule) : 
    haxeType(inName),
    module(inModule)
  {
  }
};

struct EnumDescriptor : public NonClassDescriptor {
  UEnum *uenum;

  EnumDescriptor (UEnum *inUEnum, ModuleDescriptor *inModule) : 
    NonClassDescriptor(getHaxeType(inUEnum), inModule),
    uenum(inUEnum)
  {
    this->m_thisPackage = inUEnum->GetOutermost();
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
    auto newPack = TArray<FString>(getHaxePackage(pack, &module));
    auto hxName = packArr.Pop( false );
    for (auto& packPart : packArr) {
      newPack.Push(packPart);
    }
    return FHaxeTypeRef(
      newPack,
      hxName,
      ETypeKind::KUEnum,
      module);
  }
};

struct StructDescriptor : public NonClassDescriptor {
  UScriptStruct *ustruct;

  StructDescriptor(UScriptStruct *inUStruct, ModuleDescriptor *inModule) :
    NonClassDescriptor(getHaxeType(inUStruct), inModule),
    ustruct(inUStruct)
  {
    this->m_thisPackage = inUStruct->GetOutermost();
  }

private:
  static FHaxeTypeRef getHaxeType(UStruct *inStruct) {
    auto pack = inStruct->GetOutermost();
    FString module;
    return FHaxeTypeRef(
      getHaxePackage(pack, &module),
      inStruct->GetPrefixCPP() + inStruct->GetName(),
      ETypeKind::KUStruct,
      module);
  }
};

class FHaxeTypes {
private:
  TMap<FString, ClassDescriptor *> m_classes;
  TMap<FString, EnumDescriptor *> m_enums;
  TMap<FString, StructDescriptor *> m_structs;

  TMap<UPackage *, ModuleDescriptor *> m_upackageToModule;

  const static FHaxeTypeRef nulltype;

public:
  void touchClass(UClass *inClass, const FString &inHeader, const FString &inModule) {
    if (m_classes.Contains(inClass->GetName())) {
      return; // we've already touched this type; probably it's UObject which gets added every time (!)
    }
    ClassDescriptor *cls = new ClassDescriptor(inClass, inHeader);
    m_classes.Add(inClass->GetName(), cls);
    LOG("Class name %s", *cls->haxeType.toString());
    auto module = getModule(inClass->GetOuterUPackage());
    module->touch(cls, inModule);

    // examine all properties and see if any of them uses a struct or enum in a way that would need to include the actual file
    TFieldIterator<UProperty> props(inClass, EFieldIteratorFlags::ExcludeSuper);
    for (; props; ++props) {
      UProperty *prop = *props;
      touchProperty(prop, cls);
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

  void touchProperty(UProperty *inProp, ClassDescriptor *inClass) {
    // see UnrealType.h for all possible variations
    if (inProp->IsA<UStructProperty>()) {
      auto structProp = Cast<UStructProperty>(inProp);
      if (!structProp->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm | CPF_ReferenceParm)) {
        this->touchStruct(structProp->Struct, inClass);
      }
    } else if (inProp->IsA<UNumericProperty>()) {
      auto numeric = Cast<UNumericProperty>(inProp);
      UEnum *uenum = numeric->GetIntPropertyEnum();
      if (nullptr != uenum) {
        // is enum
        this->touchEnum(uenum, inClass);
      }
    } else if (inProp->IsA<UArrayProperty>()) {
      auto prop = Cast<UArrayProperty>(inProp);
      touchProperty(prop->Inner, inClass);
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
    // UE_LOG(LogHaxeExtern,Log,TEXT("Struct %s dependson %s"), *inStruct->GetName(), *inClass->uclass->GetName());
    // UE_LOG(LogHaxeExtern,Log,TEXT("prefix %s"), inStruct->GetPrefixCPP());
    // inStruct->StructMacroDeclaredLineNumber
    auto name = inStruct->GetName();
    if (!m_structs.Contains(name)) {
      m_structs.Add(name, new StructDescriptor(inStruct, this->getModule(inStruct->GetOutermost())));
    }
    auto descr = m_structs[name];
    descr->addRef(inClass);
    // if (sameModule) UE_LOG(LogHaxeExtern,Log,TEXT("same module"));
  }

  /**
   * add a reference from the class `inClass` to enum `inEnum`
   * @see `touchStruct`
   **/
  void touchEnum(UEnum *inEnum, ClassDescriptor *inClass) {
    auto name = inEnum->GetName();
    if (!m_enums.Contains(name)) {
      m_enums.Add(name, new EnumDescriptor(inEnum, this->getModule(inEnum->GetOutermost())));
    }
    auto descr = m_enums[name];
    LOG("Haxe enum name: %s", *descr->haxeType.toString());
    descr->addRef(inClass);
    // if (sameModule) LOG("Same module %s", TEXT("YAY"));
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

  const FHaxeTypeRef& toHaxeType(UScriptStruct *inStruct) {
    FString name = inStruct->GetName();
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
    if (!m_structs.Contains(name)) {
      return nullptr;
    }

    return m_structs[name];
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
      ret.Add(elem.Value);
    }
    return ret;
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
  }
};
