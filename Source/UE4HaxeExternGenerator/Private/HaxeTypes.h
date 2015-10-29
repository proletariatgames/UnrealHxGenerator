#pragma once
#include <CoreUObject.h>
DECLARE_LOG_CATEGORY_EXTERN(LogHaxeExtern, Log, All);

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
    bool ret;
    UPackage *pack = cls->uclass->GetOuterUPackage();
    if (pack == m_thisPackage) {
      this->sameModuleRefs.Add(cls, &ret);
    } else {
      this->otherModuleRefs.Add(cls, &ret);
    }

    return ret;
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
  UStruct *ustruct;

  StructDescriptor(UStruct *inUStruct) : ustruct(inUStruct)
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
    // examine all properties and see if any of them uses a struct or enum in a way that would need to include the actual file
    TFieldIterator<UProperty> props(inClass, EFieldIteratorFlags::ExcludeSuper);
    for (; props; ++props) {
      UProperty *prop = *props;
      // see UnrealType.h for all possible variations
      if (prop->IsA<UStructProperty>()) {
        auto structProp = Cast<UStructProperty>(prop);
        // see ObjectBase.h for all possible variations
        if (!structProp->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm | CPF_ReferenceParm)) {
          // this->touchStruct(structProp->
        }
      } else if (prop->IsA<UByteProperty>()) {
        auto numeric = Cast<UByteProperty>(prop);
        UEnum *uenum = numeric->GetIntPropertyEnum();
        if (nullptr != uenum) {
          // is enum
        }
      }
    }
    // for (TFieldIterator<UProperty> ParamIt(
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
