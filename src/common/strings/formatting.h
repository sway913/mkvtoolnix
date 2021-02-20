/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL v2
   see the file COPYING for details
   or visit https://www.gnu.org/licenses/old-licenses/gpl-2.0.html

   string formatting functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#pragma once

#include "common/common_pch.h"

#include <ostream>
#include <sstream>

#include "common/strings/editing.h"
#include "common/timestamp.h"

namespace mtx::string {

constexpr auto WRAP_AT_TERMINAL_WIDTH = -1;

std::string format_timestamp(int64_t timestamp, unsigned int precision = 9);
std::string format_timestamp(int64_t timestamp, std::string const &format);

template<typename T>
std::string
format_timestamp(basic_timestamp_c<T> const &timestamp,
                 unsigned int precision = 9) {
  if (!timestamp.valid())
    return "<InvTC>";
  return format_timestamp(timestamp.to_ns(), precision);
}

template<typename T>
std::string
format_timestamp(basic_timestamp_c<T> const &timestamp,
                 std::string const &format) {
  if (!timestamp.valid())
    return "<InvTC>";
  return format_timestamp(timestamp.to_ns(), format);
}

std::string format_file_size(int64_t size);

std::string format_paragraph(std::string const &text_to_wrap,
                             int indent_column                    = 0,
                             std::string const &indent_first_line = {},
                             std::string indent_following_lines   = {},
                             int wrap_column                      = WRAP_AT_TERMINAL_WIDTH,
                             const char *break_chars              = " ,.)/:");

std::string format_rational(int64_t numerator, int64_t denominator, unsigned int precision);
std::string format_number(int64_t number);
std::string format_number(uint64_t number);

std::string to_hex(const unsigned char *buf, size_t size, bool compact = false);
inline std::string
to_hex(const std::string &buf,
       bool compact = false) {
  return to_hex(reinterpret_cast<const unsigned char *>(buf.c_str()), buf.length(), compact);
}
inline std::string
to_hex(memory_cptr const &buf,
       bool compact = false) {
  return to_hex(buf->get_buffer(), buf->get_size(), compact);
}
inline std::string
to_hex(libebml::EbmlBinary *bin,
       bool compact = false) {
  return to_hex(bin->GetBuffer(), bin->GetSize(), compact);
}
inline std::string
to_hex(libebml::EbmlBinary &bin,
       bool compact = false) {
  return to_hex(bin.GetBuffer(), bin.GetSize(), compact);
}

std::string create_minutes_seconds_time_string(unsigned int seconds, bool omit_minutes_if_zero = false);
std::string elide_string(std::string s, unsigned int max_length = 60);

template<typename RangeT, typename SeparatorT>
std::string
join(RangeT const &range,
     SeparatorT const &separator) {
  return fmt::format("{}", fmt::join(range, separator));
}

template<typename IteratorT, typename SeparatorT>
std::string
join(IteratorT first,
     IteratorT last,
     SeparatorT const &separator) {
  return fmt::format("{}", fmt::join(first, last, separator));
}

std::string to_lower_ascii(std::string const &src);
std::string to_upper_ascii(std::string const &src);

std::vector<std::string> to_lower_ascii(std::vector<std::string> const &src);
std::vector<std::string> to_upper_ascii(std::vector<std::string> const &src);

} // mtx::string

template<typename T>
std::ostream &
operator <<(std::ostream &out,
            basic_timestamp_c<T> const &timestamp) {
  out << mtx::string::format_timestamp(timestamp);
  return out;
}
