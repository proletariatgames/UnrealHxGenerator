#pragma once
#include <CoreUObject.h>
#include "HaxeTypes.h"

class FHelperBuf {
private:
  int32 m_indents;
  FString m_indent;
  FString m_buf;

public:
  FHelperBuf() {
  }

  FHelperBuf(FString buf) : m_buf(buf) {
  }

  FHelperBuf& newline() {
    return *this << "\n" << m_indent;
  }

  FHelperBuf& begin(const TCHAR *br=TEXT("{")) {
    m_indents++;
    m_indent += TEXT("  ");
    if (*br) {
      return *this;
    }

    return *this << br << "\n" << m_indent;
  }

  FHelperBuf& end(const TCHAR *br=TEXT("}")) {
    m_indents--;
    m_indent = this->m_indent.LeftChop(2);
    if (*br) {
      this->newline();
      *this << br;
      this->newline();
    }
    return *this;
  }

  FHelperBuf& addNewlines(const FString& text, bool addNewlineAfter=false) {
    FString left;
    FString right = text;
    static const FString nl = TEXT("\n");
    while (right.Split(nl, &left, &right, ESearchCase::CaseSensitive, ESearchDir::FromStart)) {
      *this << left;
      this->newline();
    }
    if (!right.IsEmpty()) {
      *this << text;
      if (addNewlineAfter) {
        this->newline();
      }
    }

    return *this;
  }

  FHelperBuf& operator <<(const FString& text) {
    this->m_buf += text;
    return *this;
  }

  FHelperBuf& operator <<(const TCHAR *text) {
    this->m_buf += text;
    return *this;
  }

  FHelperBuf& operator <<(const char *text) {
    this->m_buf += UTF8_TO_TCHAR(text);
    return *this;
  }

  FHelperBuf& operator <<(const FHelperBuf &text) {
    this->m_buf += text.m_buf;
    return *this;
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
  FString m_name;
  const FString &m_module;

public:
  FHaxeGenerator(FString &name, const FString &module) : 
    m_buf(FHelperBuf()),
    m_name(name),
    m_module(module) {
  }
};
