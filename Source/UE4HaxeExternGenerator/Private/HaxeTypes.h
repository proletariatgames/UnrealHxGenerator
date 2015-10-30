#pragma once
#include <CoreUObject.h>
DECLARE_LOG_CATEGORY_EXTERN(LogHaxeExtern, Log, All);

#define LOG(str,...) UE_LOG(LogHaxeExtern, Log, TEXT(str), __VA_ARGS__)

struct ClassDescriptor {
  UClass *uclass;
  FString header;

  ClassDescriptor(UClass *inUClass, const FString &inHeader) :
    uclass(inUClass),
    header(inHeader) 
  {
  }
};

struct NonClassDescriptor {
  TSet<const ClassDescriptor *> sameModuleRefs;
  TSet<const ClassDescriptor *> otherModuleRefs;

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
};

struct EnumDescriptor : public NonClassDescriptor {
  UEnum *uenum;

  EnumDescriptor (UEnum *inUEnum) : uenum(inUEnum)
  {
    this->m_thisPackage = inUEnum->GetOutermost();
  }
};

struct StructDescriptor : public NonClassDescriptor {
  UScriptStruct *ustruct;

  StructDescriptor(UScriptStruct *inUStruct) : ustruct(inUStruct)
  {
    this->m_thisPackage = inUStruct->GetOutermost();
  }
};

class FHaxeTypes {
private:
  TMap<FString, ClassDescriptor *> m_classes;
  TMap<FString, EnumDescriptor *> m_enums;
  TMap<FString, StructDescriptor *> m_structs;

  TMap<UPackage *, FString> m_upackageToModule;

public:
  void touchClass(UClass *inClass, const FString &inHeader, const FString &inModule) {
    if (m_classes.Contains(inClass->GetName())) {
      return; // we've already touched this type; probably it's UObject which gets added every time (!)
    }
    ClassDescriptor *cls = new ClassDescriptor(inClass, inHeader);
    if (!m_upackageToModule.Contains(inClass->GetOuterUPackage())) {
      UE_LOG(LogHaxeExtern,Log,TEXT("UPACKAGE %s; Module %s"), *inClass->GetOuterUPackage()->GetName(), *inModule);
      m_upackageToModule.Add(inClass->GetOuterUPackage(), inModule);
    }

    // examine all properties and see if any of them uses a struct or enum in a way that would need to include the actual file
    TFieldIterator<UProperty> props(inClass, EFieldIteratorFlags::ExcludeSuper);
    for (; props; ++props) {
      UProperty *prop = *props;
      // see UnrealType.h for all possible variations
      if (prop->IsA<UStructProperty>()) {
        auto structProp = Cast<UStructProperty>(prop);
        // see ObjectBase.h for all possible variations
        if (!structProp->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm | CPF_ReferenceParm)) {
          UE_LOG(LogHaxeExtern,Log,TEXT("Declaration %s"), *structProp->GetCPPType(nullptr, CPPF_None));
          this->touchStruct(structProp->Struct, cls);
        } else {
          UE_LOG(LogHaxeExtern,Log,TEXT("Property %s is a pointer/ref"),*structProp->GetName());
        }
      } else if (prop->IsA<UByteProperty>()) {
        auto numeric = Cast<UByteProperty>(prop);
        UEnum *uenum = numeric->GetIntPropertyEnum();
        if (nullptr != uenum) {
          // is enum
          LOG("UENUM FOUND: %s (declaration %s)", *uenum->GetName(), *prop->GetCPPType(nullptr, CPPF_None));
          this->touchEnum(uenum, cls);
        }
      }
    }
  }

  // add a reference from the class `inClass` to struct `inStruct`
  // this reference allows us to be sure that we can include a header that includes the definition
  // to the struct. We need that because the current UHT exporter interface doesn't pass the headers
  // of structs and enums to us. So we need to ignore structs/enums that we aren't sure to have a header
  // that has included its entire definition
  void touchStruct(UScriptStruct *inStruct, ClassDescriptor *inClass) {
    UE_LOG(LogHaxeExtern,Log,TEXT("Struct %s dependson %s"), *inStruct->GetName(), *inClass->uclass->GetName());
    UE_LOG(LogHaxeExtern,Log,TEXT("prefix %s"), inStruct->GetPrefixCPP());
    // inStruct->StructMacroDeclaredLineNumber
    auto name = inStruct->GetName();
    if (!m_structs.Contains(name)) {
      m_structs.Add(name, new StructDescriptor(inStruct));
    }
    auto descr = m_structs[name];
    bool sameModule = descr->addRef(inClass);
    if (sameModule) UE_LOG(LogHaxeExtern,Log,TEXT("same module"));
  }

  // add a reference from the class `inClass` to enum `inEnum`
  void touchEnum(UEnum *inEnum, ClassDescriptor *inClass) {
    auto name = inEnum->GetName();
    if (!m_enums.Contains(name)) {
      m_enums.Add(name, new EnumDescriptor(inEnum));
    }
    auto descr = m_enums[name];
    bool sameModule = descr->addRef(inClass);
    if (sameModule) LOG("Same module %s", TEXT("YAY"));
  }

  ///////////////////////////////////////////////////////
  // Haxe Type handling
  ///////////////////////////////////////////////////////

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
