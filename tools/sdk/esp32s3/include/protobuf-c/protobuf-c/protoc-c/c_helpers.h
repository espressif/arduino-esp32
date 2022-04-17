// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// http://code.google.com/p/protobuf/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Author: kenton@google.com (Kenton Varda)
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.

// Copyright (c) 2008-2013, Dave Benson.  All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Modified to implement C code by Dave Benson.

#ifndef GOOGLE_PROTOBUF_COMPILER_C_HELPERS_H__
#define GOOGLE_PROTOBUF_COMPILER_C_HELPERS_H__

#include <string>
#include <vector>
#include <sstream>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/io/printer.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace c {

// Returns the non-nested type name for the given type.  If "qualified" is
// true, prefix the type with the full namespace.  For example, if you had:
//   package foo.bar;
//   message Baz { message Qux {} }
// Then the qualified ClassName for Qux would be:
//   Foo__Bar__Baz_Qux
// While the non-qualified version would be:
//   Baz_Qux
string ClassName(const Descriptor* descriptor, bool qualified);
string ClassName(const EnumDescriptor* enum_descriptor, bool qualified);

// --- Borrowed from stubs. ---
template <typename T> string SimpleItoa(T n) {
  std::stringstream stream;
  stream << n;
  return stream.str();
}

string SimpleFtoa(float f);
string SimpleDtoa(double f);
void SplitStringUsing(const string &str, const char *delim, std::vector<string> *out);
string CEscape(const string& src);
string StringReplace(const string& s, const string& oldsub, const string& newsub, bool replace_all);
inline bool HasSuffixString(const string& str, const string& suffix) { return str.size() >= suffix.size() && str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0; }
inline string StripSuffixString(const string& str, const string& suffix) { if (HasSuffixString(str, suffix)) { return str.substr(0, str.size() - suffix.size()); } else { return str; } }
char* FastHexToBuffer(int i, char* buffer);


// Get the (unqualified) name that should be used for this field in C code.
// The name is coerced to lower-case to emulate proto1 behavior.  People
// should be using lowercase-with-underscores style for proto field names
// anyway, so normally this just returns field->name().
string FieldName(const FieldDescriptor* field);

// Get macro string for deprecated field
string FieldDeprecated(const FieldDescriptor* field);

// Returns the scope where the field was defined (for extensions, this is
// different from the message type to which the field applies).
inline const Descriptor* FieldScope(const FieldDescriptor* field) {
  return field->is_extension() ?
    field->extension_scope() : field->containing_type();
}

// convert a CamelCase class name into an all uppercase affair
// with underscores separating words, e.g. MyClass becomes MY_CLASS.
string CamelToUpper(const string &class_name);
string CamelToLower(const string &class_name);

// lowercased, underscored name to camel case
string ToCamel(const string &name);

// lowercase the string
string ToLower(const string &class_name);
string ToUpper(const string &class_name);

// full_name() to lowercase with underscores
string FullNameToLower(const string &full_name);
string FullNameToUpper(const string &full_name);

// full_name() to c-typename (with underscores for packages, otherwise camel case)
string FullNameToC(const string &class_name);

// Splits, indents, formats, and prints comment lines
void PrintComment (io::Printer* printer, string comment);

// make a string of spaces as long as input
string ConvertToSpaces(const string &input);

// Strips ".proto" or ".protodevel" from the end of a filename.
string StripProto(const string& filename);

// Get the C++ type name for a primitive type (e.g. "double", "::google::protobuf::int32", etc.).
// Note:  non-built-in type names will be qualified, meaning they will start
// with a ::.  If you are using the type as a template parameter, you will
// need to insure there is a space between the < and the ::, because the
// ridiculous C++ standard defines "<:" to be a synonym for "[".
const char* PrimitiveTypeName(FieldDescriptor::CppType type);

// Get the declared type name in CamelCase format, as is used e.g. for the
// methods of WireFormat.  For example, TYPE_INT32 becomes "Int32".
const char* DeclaredTypeMethodName(FieldDescriptor::Type type);

// Convert a file name into a valid identifier.
string FilenameIdentifier(const string& filename);

// Return the name of the BuildDescriptors() function for a given file.
string GlobalBuildDescriptorsName(const string& filename);

// return 'required', 'optional', or 'repeated'
string GetLabelName(FieldDescriptor::Label label);


// write IntRanges entries for a bunch of sorted values.
// returns the number of ranges there are to bsearch.
unsigned WriteIntRanges(io::Printer* printer, int n_values, const int *values, const string &name);

struct NameIndex
{
  unsigned index;
  const char *name;
};
int compare_name_indices_by_name(const void*, const void*);

// Return the syntax version of the file containing the field.
// This wrapper is needed to be able to compile against protobuf2.
inline int FieldSyntax(const FieldDescriptor* field) {
#ifdef HAVE_PROTO3
  return field->file()->syntax() == FileDescriptor::SYNTAX_PROTO3 ? 3 : 2;
#else
  return 2;
#endif
}

}  // namespace c
}  // namespace compiler
}  // namespace protobuf

}  // namespace google
#endif  // GOOGLE_PROTOBUF_COMPILER_C_HELPERS_H__
