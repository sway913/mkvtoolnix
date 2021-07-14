def create_iso639_language_list_file
  list_file       = "iso-639-3"
  list_downloaded = false

  if !FileTest.exists?(list_file)
    url = "https://iso639-3.sil.org/sites/iso639-3/files/downloads/iso-639-3.tab"
    runq "wget", url, "wget --quiet -O #{list_file} #{url}"

    list_downloaded = true
  end

  lines   = IO.readlines(list_file).map(&:chomp)
  headers = Hash[ *
    lines.
    shift.
    split(%r{\t}).
    map(&:downcase).
    each_with_index.
    map { |name, index| [ index, name ] }.
    flatten
  ]

  rows = lines.
    map do |line|
    parts = line.split(%r{\t})
    entry = Hash[ *
      (0..parts.size).
      map { |idx| [ headers[idx], !parts[idx] || parts[idx].empty? ? nil : parts[idx] ] }.
      flatten
    ]

    entry
  end.
    reject { |entry| !%r{^[CLS]$}.match(entry["language_type"]) }. # Constructed, Living & Special
    map do |entry|
    {
      "name"           => entry["ref_name"],
      "bibliographic"  => entry["part2b"] && (entry["part2b"] != entry["part2t"]) ? entry["part2b"] : nil,
      "alpha_2"        => entry["part1"],
      "alpha_3"        => entry["part2t"] || entry["id"],
      "alpha_3_to_use" => entry["part2b"] || entry["id"],
      "has_639_2"      => !entry["part2b"].nil?,
    }
  end.map do |entry|
    [ entry["name"].to_u8_cpp_string,
      entry["alpha_3_to_use"].to_cpp_string,
      (entry["alpha_2"] || '').to_cpp_string,
      entry["bibliographic"] ? entry["alpha_3"].to_cpp_string : '""s',
      entry["has_639_2"].to_s,
    ]
  end

  rows += ("a".."d").map do |letter|
    [ %Q{u8"Reserved for local use: qa#{letter}"s},
      %Q{u8"qa#{letter}"s},
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

#include "common/iso639_types.h"

using namespace std::string_literals;

namespace mtx::iso639 {

std::vector<language_t> g_languages;

void
init() {
  g_languages.reserve(#{rows.size});

EOT

  footer = <<EOT
}

} // namespace mtx::iso639
EOT

  content       = header + format_table(rows.sort, :column_suffix => ',', :row_prefix => "  g_languages.emplace_back(", :row_suffix => ");").join("\n") + "\n" + footer
  cpp_file_name = "src/common/iso639_language_list.cpp"

  runq("write", cpp_file_name) { IO.write("#{$source_dir}/#{cpp_file_name}", content); 0 }

ensure
  File.unlink(list_file) if list_downloaded && FileTest.exists?(list_file)
end
