def create_iso639_language_list_file
  cpp_file_name = "src/common/iso639_language_list.cpp"
  iso639_2      = JSON.parse(IO.readlines("/usr/share/iso-codes/json/iso_639-2.json").join(''))
  rows          = iso639_2["639-2"].
    reject { |entry| %r{^qaa}.match(entry["alpha_3"]) }.
    map do |entry|
    [ entry["name"].to_u8_cpp_string,
      (entry["bibliographic"] || entry["alpha_3"]).to_cpp_string,
      (entry["alpha_2"] || '').to_cpp_string,
      entry["bibliographic"] ? entry["alpha_3"].to_cpp_string : '""s',
      'true ',
    ]
  end

  rows += ("a".."d").map do |letter|
    [ %Q{"Reserved for local use: qa#{letter}"},
      %Q{"qa#{letter}"},
      '""s',
      '""s',
      'true ',
    ]
  end

  header = <<EOT
/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL v2
   see the file COPYING for details
   or visit https://www.gnu.org/licenses/old-licenses/gpl-2.0.html

   ISO 639 language definitions, lookup functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

// -----------------------------------------------------------------------
// NOTE: this file is auto-generated by the "dev:iso639_list" rake target.
// -----------------------------------------------------------------------

#include "common/common_pch.h"

#include "common/iso639.h"

namespace mtx::iso639 {

std::vector<language_t> const g_languages{
EOT

  footer = <<EOT
};

} // namespace mtx::iso639
EOT

  content = header + format_table(rows.sort, :column_suffix => ',', :row_prefix => "  { ", :row_suffix => " },").join("\n") + "\n" + footer

  runq("write", cpp_file_name) { IO.write("#{$source_dir}/#{cpp_file_name}", content); 0 }
end
