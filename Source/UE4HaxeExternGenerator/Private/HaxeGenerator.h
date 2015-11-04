#pragma once
#include <CoreUObject.h>
#include "HaxeTypes.h"

struct Begin {
  const TCHAR *br;
  Begin(const TCHAR *inBr=TEXT("{")) : br(inBr) 
  {
  }
};

struct End {
  const TCHAR *br;
  End(const TCHAR *inBr=TEXT("}")) : br(inBr) 
  {
  }
};

struct Newline {
  Newline() {}
};

struct Escaped {
  const FString str;
  Escaped(const FString inStr) : str(inStr)
  {
  }
};

struct Comment {
  const FString str;
  Comment(const FString inStr) : str(inStr)
  {
  }
};

class FHelperBuf {
private:
  int32 m_indents;
  FString m_indent;
  FString m_buf;

  bool m_hasContent;

public:
  FHelperBuf() {
  }

  FHelperBuf(FString inBuf) : m_buf(inBuf) 
  {
  }

  FHelperBuf& newline() {
    this->m_hasContent = false;
    return *this << "\n" << m_indent;
  }

  FHelperBuf& begin(const TCHAR *inBr=TEXT("{")) {
    m_indents++;
    m_indent += TEXT("  ");
    if (!*inBr) {
      return *this;
    }

    return *this << inBr << "\n" << m_indent;
  }

  FHelperBuf& end(const TCHAR *inBr=TEXT("}")) {
    m_indents--;
    m_indent = this->m_indent.LeftChop(2);
    if (*inBr) {
      this->newline();
      *this << inBr;
      this->newline();
    }
    return *this;
  }

  FHelperBuf& addEscaped(const FString& inText) {
    this->m_buf += inText.ReplaceCharWithEscapedChar();
    return *this;
  }

  FHelperBuf& addNewlines(const FString& inText, bool inAddNewlineAfter=false) {
    FString left;
    FString right = inText;
    static const FString nl = TEXT("\n");
    while (right.Split(nl, &left, &right, ESearchCase::CaseSensitive, ESearchDir::FromStart)) {
      *this << left;
      this->newline();
    }
    if (!right.IsEmpty()) {
      *this << right;
      if (inAddNewlineAfter) {
        this->newline();
      }
    }

    return *this;
  }

  FHelperBuf& comment(const FString& inText) {
    if (m_hasContent) {
      this->newline();
    }
    this->begin(TEXT("/**"));
    this->addNewlines(inText, false);
    this->end(TEXT("**/"));

    return *this;
  }

  FHelperBuf& operator <<(const FString& inText) {
    this->m_buf += inText;
    this->m_hasContent = true;
    return *this;
  }

  FHelperBuf& operator <<(const TCHAR *inText) {
    this->m_buf += inText;
    this->m_hasContent = true;
    return *this;
  }

  FHelperBuf& operator <<(const char *inText) {
    this->m_buf += UTF8_TO_TCHAR(inText);
    this->m_hasContent = true;
    return *this;
  }

  FHelperBuf& operator <<(const FHelperBuf &inText) {
    this->m_buf += inText.m_buf;
    this->m_hasContent = true;
    return *this;
  }

  FHelperBuf& operator <<(const Begin inBegin) {
    return this->begin(inBegin.br);
  }

  FHelperBuf& operator <<(const End inEnd) {
    return this->end(inEnd.br);
  }

  FHelperBuf& operator <<(const Newline _) {
    return this->newline();
  }

  FHelperBuf& operator <<(const Escaped& inEscaped) {
    return this->addEscaped(inEscaped.str);
  }

  FHelperBuf& operator <<(const Comment& inComment) {
    return this->comment(inComment.str);
  }

  FString toString() {
    return m_buf;
  }

  void addTo(FString &str) {
    str += this->m_buf;
  }
};

class FHaxeGenerator {
private:
  FHelperBuf m_buf;
  FHaxeTypes& m_haxeTypes;
  const FString& m_basePath;

public:
  FHaxeGenerator(FHaxeTypes& inTypes, const FString& inBasePath) : 
    m_buf(FHelperBuf()),
    m_haxeTypes(inTypes),
    m_basePath(inBasePath)
  {
  }

  static FString getHeaderPath(UPackage *inPack, const FString& inPath) {
    if (inPath.IsEmpty()) {
      // this is a particularity of UHT - it sometimes adds no header path to some of the core UObjects
      return FString("CoreUObject.h");
    }
    int32 index = inPath.Find(TEXT("Public"), ESearchCase::IgnoreCase, ESearchDir::FromEnd, inPath.Len());
    if (index < 0) {
      index = inPath.Find(TEXT("Classes"), ESearchCase::IgnoreCase, ESearchDir::FromEnd, inPath.Len());
      if (index >= 0)
        index += sizeof("Classes");
    } else {
      index += sizeof("Public");
    }
    if (index < 0) {
      auto pack = inPack->GetName().RightChop( sizeof("/Script") );
      auto lastSlash = inPath.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromEnd, inPath.Len());
      auto lastBackslash = inPath.Find(TEXT("\\"), ESearchCase::CaseSensitive, ESearchDir::FromEnd, inPath.Len());
      int startPos = (lastSlash > lastBackslash) ? lastSlash : lastBackslash;
      index = inPath.Find(pack, ESearchCase::IgnoreCase, ESearchDir::FromEnd, startPos);
      if (index >= 0)
        index += pack.Len() + 1;
    }
    if (index >= 0) {
      int len = inPath.Len();
      while (len > ++index && (inPath[index] == TCHAR('/') || inPath[index] == TCHAR('\\'))) {
        //advance index
      }
      LOG("%s: %s", *inPath, *inPath.RightChop(index - 1));
      return inPath.RightChop(index - 1);
    }

    UE_LOG(LogHaxeExtern, Fatal, TEXT("Cannot determine header path of %s on package %s"), *inPath, *inPack->GetName());
    return FString();
  }

  bool convertClass(const ClassDescriptor *inClass) {
    static const FName NAME_ToolTip(TEXT("ToolTip"));
    auto hxType = inClass->haxeType;

    if (hxType.pack.Num() > 0) {
      m_buf << TEXT("package ") << FString::Join(hxType.pack, TEXT(".")) << ";" << Newline() << Newline();
    }
    
    auto isInterface = hxType.kind == ETypeKind::KUInterface;
    auto uclass = inClass->uclass;
    // comment
    auto& comment = uclass->GetMetaData(NAME_ToolTip);
    if (!comment.IsEmpty()) {
      m_buf << Comment(comment);
    }
    // @:umodule
    if (!hxType.module.IsEmpty()) {
      m_buf << TEXT("@:umodule(\"") << Escaped(hxType.module) << TEXT("\")") << Newline();
    }
    // @:glueCppIncludes
    m_buf << TEXT("@:glueCppIncludes(\"") << Escaped(getHeaderPath(inClass->uclass->GetOuterUPackage(), inClass->header)) << TEXT("\")") << Newline();
    m_buf << TEXT("@:uextern extern ") << (isInterface ? TEXT("interface ") : TEXT("class ")) << hxType.name;
    if (!isInterface) {
      auto superUClass = uclass->GetSuperClass();
      const ClassDescriptor *super = nullptr;
      if (nullptr != superUClass) {
        super = m_haxeTypes.getDescriptor(superUClass);
        m_buf << " extends " << super->haxeType.toString();
      }
    }

    const auto implements = isInterface ? TEXT(" extends ") : TEXT(" implements ");
    // for now it doesn't seem that Unreal supports interfaces that extend other interfaces, but let's make ourselves ready for it
    for (auto& impl : uclass->Interfaces) {
      auto ifaceType = m_haxeTypes.getDescriptor(impl.Class);
      if (ifaceType->haxeType.kind != ETypeKind::KNone) {
        m_buf << implements << ifaceType->haxeType.toString();
      }
    }
    m_buf << Begin(TEXT(" {"));
    {
      auto wasEditorOnly = false;
      TFieldIterator<UField> fields(uclass, EFieldIteratorFlags::ExcludeSuper);
      for (; fields; ++fields) {
        auto field = *fields;
        if (field->IsA<UProperty>()) {
          auto prop = Cast<UProperty>(field);
          FString type;
          if (upropType(prop, type)) {
            auto isEditorOnly = prop->HasAnyPropertyFlags(CPF_EditorOnly);
            if (isEditorOnly != wasEditorOnly) {
              if (isEditorOnly) {
                m_buf << TEXT("#if WITH_EDITORONLY_DATA") << Newline();
              } else {
                m_buf << TEXT("#end // WITH_EDITORONLY_DATA") << Newline();
              }
              wasEditorOnly = isEditorOnly;
            }
            auto& propComment = prop->GetMetaData(NAME_ToolTip);
            if (!propComment.IsEmpty()) {
              m_buf << Comment(propComment);
            }
            m_buf << (prop->HasAnyPropertyFlags(CPF_Protected) ? TEXT("private var ") : TEXT("public var ")) << prop->GetNameCPP();
            // TODO see if the property is read-only; this might not be supported by UHT atm?
            // if (prop->HasAnyPropertyFlags( CPF_Con
            m_buf << TEXT(" : ") << type << TEXT(";") << Newline();
          }
        } else if (field->IsA<UFunction>()) {
          auto func = Cast<UFunction>(field);
          if (func->GetOwnerClass() != uclass) {
            // we don't need to generate overridden functions' glue code
            continue;
          } else if (func->HasAnyFunctionFlags(FUNC_Private)) {
            // we can't access private functions
            continue;
          }
          // we need to create a local buffer because we will only know if we should
          // generate this function in the end of its processing
          FHelperBuf curBuf;
          curBuf << (func->HasAnyFunctionFlags(FUNC_Public) ? TEXT("public function ") : TEXT("private function ")) << func->GetName() << TEXT("(");
          auto first = true;
          auto shouldExport = true;
          bool hasReturnValue = false;
          for (TFieldIterator<UProperty> params(func); params; ++params) {
            check(!hasReturnValue);
            auto param = *params;
            FString type;
            if (upropType(param, type)) {
              if (param->HasAnyPropertyFlags(CPF_ReturnParm)) {
                hasReturnValue = true;
                curBuf << TEXT(") : ") << type;
              } else {
                if (first) first = false; else curBuf << TEXT(", ");
                curBuf << param->GetNameCPP() << TEXT(" : ") << type;
              }
            } else {
              shouldExport = false;
              break;
            }
          }
          if (!hasReturnValue) {
            curBuf << TEXT(") : Void;");
          } else {
            curBuf << TEXT(";");
          }
          if (shouldExport) {
            // seems like we don't have a editor-only specifier for ufunctions
            if (wasEditorOnly) {
              wasEditorOnly = false;
              m_buf << TEXT("#end // WITH_EDITORONLY_DATA") << Newline();
            }
            auto& fnComment = func->GetMetaData(NAME_ToolTip);
            if (!fnComment.IsEmpty()) {
              m_buf << Comment(fnComment);
            }
            m_buf << curBuf.toString() << Newline();
          }
        } else {
          LOG("Field %s is not a UFUNCTION or UPROERTY", *field->GetName());
        }
      }
      if (wasEditorOnly) {
        wasEditorOnly = false;
        m_buf << TEXT("#end // WITH_EDITORONLY_DATA") << Newline();
      }
    }
    m_buf << End();
    printf("%s\n", TCHAR_TO_UTF8(*m_buf.toString()));
    return true;
  }

  bool writeWithModifiers(const FString &inName, UProperty *inProp, FString &outType) {
    auto end = FString();
    // check all the flags that interest us
    // UStruct pointers aren't supported; so we're left either with PRef, PStruct and Const to check
    if (inProp->HasAnyPropertyFlags(CPF_ReferenceParm)) {
      outType += TEXT("unreal.PRef<");
      end += TEXT(">");
    }
    if (inProp->HasAnyPropertyFlags(CPF_ConstParm) || (inProp->HasAnyPropertyFlags(CPF_ReferenceParm) && !inProp->HasAnyPropertyFlags(CPF_OutParm))) {
      outType += TEXT("unreal.Const<");
      end += TEXT(">");
    }

    outType += inName + end;
    return true;
  }

  // Gets the Haxe representation for a `UProperty` type. This is used both for uproperties and for ufunction arguments
  // Returns an empty string if the type is not supported
  bool upropType(UProperty* inProp, FString &outType) {
    // from the most common to the least
    if (inProp->IsA<UStructProperty>()) {
      auto prop = Cast<UStructProperty>(inProp);
      auto descr = m_haxeTypes.getDescriptor( prop->Struct );
      if (descr == nullptr) {
        LOG("(struct) TYPE NOT SUPPORTED: %s", *prop->Struct->GetName());
        // may happen if we never used this in a way the struct is known
        return false;
      }
      return writeWithModifiers(descr->haxeType.toString(), inProp, outType);
    } else if (inProp->IsA<UObjectProperty>()) {
      if (inProp->IsA<UClassProperty>()) {
        auto prop = Cast<UClassProperty>(inProp);
        if (prop->HasAnyPropertyFlags(CPF_UObjectWrapper)) {
          auto descr = m_haxeTypes.getDescriptor(prop->MetaClass);
          if (descr == nullptr) {
            LOG("(tsubclassof) TYPE NOT SUPPORTED: %s", *prop->PropertyClass->GetName());
            return false;
          }
          outType += TEXT("unreal.TSubclassOf<") + descr->haxeType.toString() + TEXT(">");
          return true;
        }
      }
      auto prop = Cast<UObjectProperty>(inProp);
      auto descr = m_haxeTypes.getDescriptor(prop->PropertyClass);
      if (descr == nullptr) {
        LOG("(uclass) TYPE NOT SUPPORTED: %s", *prop->PropertyClass->GetName());
        return false;
      }
      outType += descr->haxeType.toString();
      return true;
    } else if (inProp->IsA<UNumericProperty>()) {
      if (inProp->IsA<UByteProperty>()) {
        outType += TEXT("unreal.UInt8");
      } else if (inProp->IsA<UInt8Property>()) {
        outType += TEXT("unreal.Int8");
      } else if (inProp->IsA<UInt16Property>()) {
        outType += TEXT("unreal.Int16");
      } else if (inProp->IsA<UIntProperty>()) {
        outType += TEXT("unreal.Int32");
      } else if (inProp->IsA<UInt64Property>()) {
        outType += TEXT("unreal.Int64");
      } else if (inProp->IsA<UUInt16Property>()) {
        outType += TEXT("unreal.UInt16");
      } else if (inProp->IsA<UUInt32Property>()) {
        outType += TEXT("unreal.FakeUInt32");
      } else if (inProp->IsA<UUInt64Property>()) {
        outType += TEXT("unreal.FakeUInt64");
      } else if (inProp->IsA<UFloatProperty>()) {
        outType += TEXT("unreal.Float32");
      } else if (inProp->IsA<UDoubleProperty>()) {
        outType += TEXT("unreal.Float64");
      } else {
        LOG("NUMERIC TYPE NOT SUPPORTED: %s", *inProp->GetClass()->GetName());
        return false;
      }
      return true;
    } else if (inProp->IsA<UBoolProperty>()) {
      outType += TEXT("Bool");
      return true;
    } else if (inProp->IsA<UNameProperty>()) {
      return writeWithModifiers(TEXT("unreal.FName"), inProp, outType);
    } else if (inProp->IsA<UStrProperty>()) {
      return writeWithModifiers(TEXT("unreal.FString"), inProp, outType);
    } else if (inProp->IsA<UArrayProperty>()) {
      auto prop = Cast<UArrayProperty>(inProp);
      FString inner;
      if (!upropType(prop->Inner, inner))
        return false;
      return writeWithModifiers(TEXT("unreal.TArray<") + inner + TEXT(">"), inProp, outType);
    }
    // uenum
    // uinterface
    // udelegate
    //
    // TLazyObjectPtr
    // UAssetObjectPtr - TPersistentObjectPtr
    // UInterfaceProperty
    // UMapProperty (TMap)
    // UDelegateProperty 
    // UMulticastDelegateProperty 

    LOG("Property %s (class %s) not supported", *inProp->GetName(), *inProp->GetClass()->GetName());
    return false;
  }
};
