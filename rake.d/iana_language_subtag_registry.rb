module Mtx::IANALanguageSubtagRegistry
  @@registry_mutex = Mutex.new
  @@registry       = nil

  def self.fetch_registry
    @@registry_mutex.synchronize {
      return @@registry if @@registry

      shorten_description_for = %w{1959acad abl1943 ao1990 colb1945}
      @@registry              = {}
      entry                   = {}
      process                 = lambda do
        type = entry[:type]

        if shorten_description_for.include? entry[:subtag]
          entry[:description].gsub!(%r{ +\(.*?\)}, '')
        end

        if type
          @@registry[type] ||= []
          @@registry[type]  << entry
        end

        entry = {}
      end

      current_sym             = nil

      Mtx::OnlineFile.download("https://www.iana.org/assignments/language-subtag-registry/language-subtag-registry").
        split(%r{\n+}).
        map(&:chomp).
        each do |line|

        if line == '%%'
          process.call
          current_sym = nil

        elsif %r{^Prefix: *(.+)}i.match(line)
          entry[:prefix] ||= []
          entry[:prefix]  << $1
          current_sym      = nil

        elsif %r{^(.*?): *(.+)}i.match(line)
          key, value         = $1, $2
          current_sym        = key.downcase.gsub(%r{-}, '_').to_sym
          entry[current_sym] = value

        elsif %r{^ +(.+)}.match(line) && current_sym
          entry[current_sym] += " #{$1}"

        end
      end

      process.call
    }

    return @@registry
  end

  def self.format_one_extlang_variant entry
    if entry[:prefix]
      prefix = 'VS{ ' + entry[:prefix].sort.map(&:to_cpp_string).join(', ') + ' }'
    else
      prefix = 'VS{}'
    end

    [ entry[:subtag].downcase.to_cpp_string,
      entry[:description].to_u8_cpp_string,
      prefix,
      entry.key?(:deprecated).to_s,
    ]
  end

  def self.format_extlangs_variants entries, type, name
    rows = entries[type].map { |entry| self.format_one_extlang_variant entry }

    "  g_#{name}.reserve(#{entries[type].size});\n\n" +
      format_table(rows.sort, :column_suffix => ',', :row_prefix => "  g_#{name}.emplace_back(", :row_suffix => ");").join("\n") +
      "\n"
  end

  def self.format_one_grandfathered entry
    [ entry[:tag].to_cpp_string,
      entry[:description].to_u8_cpp_string,
      'VS{}',
      'true',
    ]
  end

  def self.format_grandfathered entries
    rows = entries["grandfathered"].map { |entry| self.format_one_grandfathered entry }

    "  g_grandfathered.reserve(#{entries["grandfathered"].size});\n\n" +
      format_table(rows.sort, :column_suffix => ',', :row_prefix => "  g_grandfathered.emplace_back(", :row_suffix => ");").join("\n") +
      "\n"
  end

  def self.preferred_value_type_original type, pv
    return %r{-}.match(pv) ? :tag : type.to_sym
  end

  def self.preferred_value_type_target type, pv
    return %r{-|^[a-z]{2,3}$}.match(pv) ? :tag : type.to_sym
  end

  def self.format_one_preferred_value_construction pv_type, pv
    pv_str = pv.to_cpp_string

    return "mtx::bcp47::language_c::parse(#{pv_str})"                                              if [:tag,    :language].include?(pv_type)
    return "mtx::bcp47::language_c{}.set_#{pv_type}(#{pv_str}).set_valid(true)"                    if [:region, :grandfathered].include?(pv_type)
    return "mtx::bcp47::language_c{}.set_extended_language_subtags(VS{#{pv_str}}).set_valid(true)" if :extlang == pv_type
    return "mtx::bcp47::language_c{}.set_variants(VS{#{pv_str}}).set_valid(true)"                  if :variant == pv_type

    fail "unknown pv_type #{pv_type}"
  end

  def self.format_one_preferred_value_target type, pv
    pv_type = self.preferred_value_type type, pv
    pv_str  = pv.to_cpp_string

    return "mtx::bcp47::language_c::parse(#{pv_str})"                                              if [:tag,    :language].include?(pv_type)
    return "mtx::bcp47::language_c{}.set_#{pv_type}(#{pv_str}).set_valid(true)"                    if [:region, :grandfathered].include?(pv_type)
    return "mtx::bcp47::language_c{}.set_extended_language_subtags(VS{#{pv_str}}).set_valid(true)" if :extlang == pv_type
    return "mtx::bcp47::language_c{}.set_variants(VS{#{pv_str}}).set_valid(true)"                  if :variant == pv_type

    fail "unknown pv_type #{pv_type}"
  end

  def self.format_one_preferred_value entry
    return [
      self.format_one_preferred_value_construction(self.preferred_value_type_original(entry[:type], entry[:original_value]),  entry[:original_value]),
      self.format_one_preferred_value_construction(self.preferred_value_type_target(  entry[:type], entry[:preferred_value]), entry[:preferred_value]),
    ]
  end

  def self.format_preferred_values entries
    rows = entries.
      values.
      map     { |v| v.select { |e| e.key?(:preferred_value) } }.
      flatten.
      map     { |e| e[:original_value] = (e.key?(:prefix) ? e[:prefix].first + "-" : "") + (e[:subtag] || e[:tag]); e }.
      sort_by { |e| [ 10 - e[:original_value].gsub(%r{[^-]+}, '').length, e[:original_value].downcase ] }.
      map     { |e| self.format_one_preferred_value e }

    "  g_preferred_values.reserve(#{rows.size});\n\n" +
      format_table(rows, :column_suffix => ',', :row_prefix => "  g_preferred_values.emplace_back(", :row_suffix => ");").join("\n") +
      "\n"
  end

  def self.do_create_cpp entries
    cpp_file_name = "src/common/iana_language_subtag_registry_list.cpp"
    formatted     = [
      self.format_extlangs_variants(entries, "extlang", "extlangs"),
      self.format_extlangs_variants(entries, "variant", "variants"),
      self.format_grandfathered(entries),
    ]

    header = <<EOT
/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL v2
   see the file COPYING for details
   or visit https://www.gnu.org/licenses/old-licenses/gpl-2.0.html

   IANA language subtag registry

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

// ----------------------------------------------------------------------------------------------
// NOTE: this file is auto-generated by the "dev:iana_language_subtag_registry_list" rake target.
// ----------------------------------------------------------------------------------------------

#include "common/common_pch.h"

#include "common/bcp47.h"
#include "common/iana_language_subtag_registry.h"

namespace mtx::iana::language_subtag_registry {

std::vector<entry_t> g_extlangs, g_variants, g_grandfathered;
std::vector<std::pair<mtx::bcp47::language_c, mtx::bcp47::language_c>> g_preferred_values;

using VS = std::vector<std::string>;

void
init() {
EOT

    middle = <<EOT
}

void
init_preferred_values() {
EOT

    footer = <<EOT
}

} // namespace mtx::iana::language_subtag_registry
EOT

    content = header +
      formatted.join("\n") +
      middle +
      self.format_preferred_values(entries) + "\n" +
      footer

    runq("write", cpp_file_name) { IO.write("#{$source_dir}/#{cpp_file_name}", content); 0 }
  end

  def self.create_cpp
    do_create_cpp(self.fetch_registry)
  end
end
