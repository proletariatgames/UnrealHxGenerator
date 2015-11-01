#pragma once
#include <CoreUObject.h>
DECLARE_LOG_CATEGORY_EXTERN(LogHaxeExtern, Log, All);

#define LOG(str,...) UE_LOG(LogHaxeExtern, Log, TEXT(str), __VA_ARGS__)

static const FString getHaxePackage(UPackage *pack) {
  static const TCHAR *CoreUObject = TEXT("/Script/CoreUObject");
  static const TCHAR *Engine = TEXT("/Script/Engine");
  static const TCHAR *UnrealEd = TEXT("/Script/UnrealEd");
  if (pack->GetName() == CoreUObject || pack->GetName() == Engine) {
    static const FString ret = FString(TEXT("unreal."));
    return ret;
  } else if (pack->GetName() == UnrealEd) {
    static const FString ret = FString(TEXT("unreal.editor."));
    return ret;
  }
  return FString::Printf( TEXT("unreal.%s."), *pack->GetName().RightChop( sizeof("/Script") ).ToLower() );
}

struct ClassDescriptor {
  UClass *uclass;
  FString header;
  const FString hxName;

  ClassDescriptor(UClass *inUClass, const FString &inHeader) :
    uclass(inUClass),
    header(inHeader),
    hxName(getHxName(inUClass))
  {
  }

private:
  FString getHxName(UClass *inUClass) {
    auto pack = inUClass->GetOuterUPackage();
    return FString::Printf( TEXT("%s%s%s"), *getHaxePackage(pack), inUClass->GetPrefixCPP(), *inUClass->GetName() );
  }
};

struct NonClassDescriptor {
  TSet<const ClassDescriptor *> sameModuleRefs;
  TSet<const ClassDescriptor *> otherModuleRefs;
  const FString hxName;

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

protected:
  const UPackage *m_thisPackage;
  NonClassDescriptor(FString name) : 
    hxName(name)
  {
  }
};

struct EnumDescriptor : public NonClassDescriptor {
  UEnum *uenum;

  EnumDescriptor (UEnum *inUEnum) : 
    NonClassDescriptor(getHxName(inUEnum)),
    uenum(inUEnum)
  {
    this->m_thisPackage = inUEnum->GetOutermost();
  }

private:
  static FString getHxName(UEnum *inEnum) {
    auto pack = inEnum->GetOutermost();
    auto name = inEnum->CppType;
    TArray<FString> packArr;
    name.ParseIntoArray(packArr, TEXT("::"), true);
    switch (inEnum->GetCppForm()) {
    case UEnum::ECppForm::Namespaced:
      packArr.Pop(true);
    default: {}
    }
    if (packArr.Num() > 1) {
      for (int i = 0; i < packArr.Num() - 1; i++) {
        packArr[i] = packArr[i].ToLower();
      }
    }

    return FString::Printf( TEXT("%s%s"), *getHaxePackage(pack), *FString::Join(packArr, TEXT(".")) );
  }
};

struct StructDescriptor : public NonClassDescriptor {
  UScriptStruct *ustruct;

  StructDescriptor(UScriptStruct *inUStruct) :
    NonClassDescriptor(getHxName(inUStruct)),
    ustruct(inUStruct)
  {
    this->m_thisPackage = inUStruct->GetOutermost();
  }

private:
  static FString getHxName(UStruct *inStruct) {
    auto pack = inStruct->GetOutermost();
    return FString::Printf( TEXT("%s%s%s"), *getHaxePackage(pack), inStruct->GetPrefixCPP(), *inStruct->GetName() );
  }
};

class FHaxeTypes {
private:
  TMap<FString, ClassDescriptor *> m_classes;
  TMap<FString, EnumDescriptor *> m_enums;
  TMap<FString, StructDescriptor *> m_structs;

  TMap<UPackage *, FString> m_upackageToModule;

  const static FString nullstring;

public:
  void touchClass(UClass *inClass, const FString &inHeader, const FString &inModule) {
    if (m_classes.Contains(inClass->GetName())) {
      return; // we've already touched this type; probably it's UObject which gets added every time (!)
    }
    ClassDescriptor *cls = new ClassDescriptor(inClass, inHeader);
    LOG("Class name %s", *cls->hxName);
    if (!m_upackageToModule.Contains(inClass->GetOuterUPackage())) {
      // UE_LOG(LogHaxeExtern,Log,TEXT("UPACKAGE %s; Module %s"), *inClass->GetOuterUPackage()->GetName(), *inModule);
      m_upackageToModule.Add(inClass->GetOuterUPackage(), inModule);
    }

    // examine all properties and see if any of them uses a struct or enum in a way that would need to include the actual file
    TFieldIterator<UProperty> props(inClass, EFieldIteratorFlags::ExcludeSuper);
    for (; props; ++props) {
      UProperty *prop = *props;
      // LOG("Property %s (%s)", *prop->GetClass()->GetName(), *prop->GetCPPType(nullptr, CPPF_None));
      // see UnrealType.h for all possible variations
      if (prop->IsA<UStructProperty>()) {
        auto structProp = Cast<UStructProperty>(prop);
        // see ObjectBase.h for all possible variations
        if (!structProp->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm | CPF_ReferenceParm)) {
          // UE_LOG(LogHaxeExtern,Log,TEXT("Declaration %s"), *structProp->GetCPPType(nullptr, CPPF_None));
          this->touchStruct(structProp->Struct, cls);
        } else {
          // UE_LOG(LogHaxeExtern,Log,TEXT("Property %s is a pointer/ref"),*structProp->GetName());
        }
      } else if (prop->IsA<UByteProperty>()) {
        auto numeric = Cast<UByteProperty>(prop);
        UEnum *uenum = numeric->GetIntPropertyEnum();
        if (nullptr != uenum) {
          // is enum
          // LOG("UENUM FOUND: %s (declaration %s)", *uenum->GetName(), *prop->GetCPPType(nullptr, CPPF_None));
          this->touchEnum(uenum, cls);
        }
      }
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
      m_structs.Add(name, new StructDescriptor(inStruct));
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
      m_enums.Add(name, new EnumDescriptor(inEnum));
    }
    auto descr = m_enums[name];
    LOG("Haxe enum name: %s", *descr->hxName);
    descr->addRef(inClass);
    // if (sameModule) LOG("Same module %s", TEXT("YAY"));
  }

  ///////////////////////////////////////////////////////
  // Haxe Type handling
  ///////////////////////////////////////////////////////

  const FString& toHaxeType(UClass *inClass) {
    FString name = inClass->GetName();
    if (!m_classes.Contains(name)) {
      return nullstring;
    }

    auto cls = m_classes[name];
    return nullstring;
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
