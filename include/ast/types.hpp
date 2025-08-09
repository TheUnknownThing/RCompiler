#pragma once

#include <map>
#include <set>
#include <string>

namespace rc {
enum class LiteralType {
  I32,
  U32,
  ISIZE,
  USIZE,
  STRING,
  RAW_STRING,
  C_STRING,
  RAW_C_STRING,
  BYTE_STRING,
  RAW_BYTE_STRING,
  CHAR,
  BYTE
};

std::map<std::string, LiteralType> literal_type_map = {
    {"i32", LiteralType::I32},
    {"u32", LiteralType::U32},
    {"isize", LiteralType::ISIZE},
    {"usize", LiteralType::USIZE},
    {"string", LiteralType::STRING},
    {"raw_string", LiteralType::RAW_STRING},
    {"c_string", LiteralType::C_STRING},
    {"raw_c_string", LiteralType::RAW_C_STRING},
    {"byte_string", LiteralType::BYTE_STRING},
    {"raw_byte_string", LiteralType::RAW_BYTE_STRING},
    {"char", LiteralType::CHAR},
    {"byte", LiteralType::BYTE}
};

std::map<LiteralType, std::string> literal_type_reverse_map = {
    {LiteralType::I32, "i32"},
    {LiteralType::U32, "u32"},
    {LiteralType::ISIZE, "isize"},
    {LiteralType::USIZE, "usize"},
    {LiteralType::STRING, "string"},
    {LiteralType::RAW_STRING, "raw_string"},
    {LiteralType::C_STRING, "c_string"},
    {LiteralType::RAW_C_STRING, "raw_c_string"},
    {LiteralType::BYTE_STRING, "byte_string"},
    {LiteralType::RAW_BYTE_STRING, "raw_byte_string"},
    {LiteralType::CHAR, "char"},
    {LiteralType::BYTE, "byte"}
};

std::set<std::string> valid_literal_types = {
    "i32", "u32", "isize", "usize", "string", "raw_string",
    "c_string", "raw_c_string", "byte_string", "raw_byte_string",
    "char", "byte"
};
}