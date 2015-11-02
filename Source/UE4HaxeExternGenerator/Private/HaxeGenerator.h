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

class FHelperBuf {
private:
  int32 m_indents;
  FString m_indent;
  FString m_buf;

public:
  FHelperBuf() {
  }

  FHelperBuf(FString inBuf) : m_buf(inBuf) 
  {
  }

  FHelperBuf& newline() {
    return *this << "\n" << m_indent;
  }

  FHelperBuf& begin(const TCHAR *inBr=TEXT("{")) {
    m_indents++;
    m_indent += TEXT("  ");
    if (*inBr) {
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
      *this << inText;
      if (inAddNewlineAfter) {
        this->newline();
      }
    }

    return *this;
  }

  FHelperBuf& operator <<(const FString& inText) {
    this->m_buf += inText;
    return *this;
  }

  FHelperBuf& operator <<(const TCHAR *inText) {
    this->m_buf += inText;
    return *this;
  }

  FHelperBuf& operator <<(const char *inText) {
    this->m_buf += UTF8_TO_TCHAR(inText);
    return *this;
  }

  FHelperBuf& operator <<(const FHelperBuf &inText) {
    this->m_buf += inText.m_buf;
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
    // if (inPath.Contains( TEXT("Public"), ESearchCase::IgnoreCase, ESearchDir::FromEnd )) {
    //   inPath.RightPad( inPath.Find
    // }
  }

  bool convertClass(const ClassDescriptor *inClass) {
    if (inClass == nullptr) {
      UE_LOG(LogHaxeExtern,Fatal,TEXT("assert"));
      return false;
    }

    TArray<FString> pack;
    inClass->hxName.ParseIntoArray(pack, TEXT("."), true);
    auto name = pack.Pop( true );

    if (pack.Num() > 0) {
      m_buf << TEXT("package ") << FString::Join(pack, TEXT(".")) << ";" << Newline() << Newline();
    }
    
    // @:glueCppIncludes
    m_buf << TEXT("@:glueCppIncludes(\"") << Escaped(getHeaderPath(inClass->uclass->GetOuterUPackage(), inClass->header)) << TEXT("\")") << Newline();
    m_buf << TEXT("@:uextern extern class ") << name;
    LOG("%s", *m_buf.toString());

    return true;
  }
};
