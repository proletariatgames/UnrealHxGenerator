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

  bool m_hasContent = false;

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
    this->addNewlines(inText.Replace(TEXT("*/"), TEXT("*")), false);
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
  bool m_hasStructs;
  TSet<FString> m_generatedFields;

  void collectSuperFields(UStruct *inSuper);
public: 
  FHaxeGenerator(FHaxeTypes& inTypes, const FString& inBasePath) : 
    m_buf(FHelperBuf()),
    m_haxeTypes(inTypes),
    m_basePath(inBasePath)
  {
  }

  bool generateClass(const ClassDescriptor *inClass);

  bool generateStruct(const StructDescriptor *inStruct);

  bool generateEnum(const EnumDescriptor *inEnum);

  FString toString() {
    return m_buf.toString();
  }

protected:
  bool writeWithModifiers(const FString &inName, UProperty *inProp, FString &outType);

  bool writeBasicWithModifiers(const FString &inName, UProperty *inProp, FString &outType);

  // Gets the Haxe representation for a `UProperty` type. This is used both for uproperties and for ufunction arguments
  // Returns an empty string if the type is not supported
  bool upropType(UProperty* inProp, FString &outType);

  static FString getHeaderPath(UPackage *inPack, const FString& inPath);

  void generateFields(UStruct *inStruct, bool onlyProps);
  void generateIncludeMetas(const NonClassDescriptor *inDesc);
};
