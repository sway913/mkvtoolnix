/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL v2
   see the file COPYING for details
   or visit https://www.gnu.org/licenses/old-licenses/gpl-2.0.html

   extracts chapters and tags as cue sheets from Matroska files

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <cassert>
#include <cmath>

#include <ebml/EbmlHead.h>
#include <ebml/EbmlSubHead.h>
#include <ebml/EbmlStream.h>
#include <ebml/EbmlVoid.h>
#include <matroska/FileKax.h>

#include <matroska/KaxChapters.h>
#include <matroska/KaxCluster.h>
#include <matroska/KaxClusterData.h>
#include <matroska/KaxSegment.h>
#include <matroska/KaxTag.h>
#include <matroska/KaxTags.h>

#include "common/chapters/chapters.h"
#include "common/ebml.h"
#include "common/hacks.h"
#include "common/kax_analyzer.h"
#include "common/mm_io.h"
#include "common/mm_io_x.h"
#include "common/strings/formatting.h"
#include "common/tags/tags.h"
#include "extract/mkvextract.h"

using namespace libmatroska;

static KaxTag *
find_tag_for_track(int idx,
                   int64_t tuid,
                   int64_t cuid,
                   EbmlMaster &m) {
  auto sidx = fmt::to_string(idx);

  size_t i;
  for (i = 0; i < m.ListSize(); i++) {
    if (!Is<KaxTag>(m[i]))
      continue;

    int64_t tag_cuid = mtx::tags::get_cuid(*static_cast<KaxTag *>(m[i]));
    if ((0 == cuid) && (-1 != tag_cuid) && (0 != tag_cuid))
      continue;

    if ((0 < cuid) && (tag_cuid != cuid))
      continue;

    int64_t tag_tuid = mtx::tags::get_tuid(*static_cast<KaxTag *>(m[i]));
    if (((-1 == tuid) || (-1 == tag_tuid) || (tuid == tag_tuid)) && ((mtx::tags::get_simple_value("PART_NUMBER", *static_cast<EbmlMaster *>(m[i])) == sidx) || (-1 == idx)))
      return static_cast<KaxTag *>(m[i]);
  }

  return nullptr;
}

static std::string
get_global_tag(const char *name,
               int64_t tuid,
               KaxTags &tags) {
  KaxTag *tag = find_tag_for_track(-1, tuid, 0, tags);
  if (!tag)
    return "";

  return mtx::tags::get_simple_value(name, *tag);
}

static int64_t
get_chapter_index(int idx,
                  KaxChapterAtom &atom) {
  size_t i;
  std::string sidx = fmt::format("INDEX {0:02}", idx);
  for (i = 0; i < atom.ListSize(); i++)
    if (   Is<KaxChapterAtom>(atom[i])
        && (mtx::chapters::get_name(*static_cast<KaxChapterAtom *>(atom[i])) == sidx))
      return mtx::chapters::get_start(*static_cast<KaxChapterAtom *>(atom[i]));

  return -1;
}

void
write_cuesheet(std::string file_name,
               KaxChapters &chapters,
               KaxTags &tags,
               int64_t tuid,
               mm_io_c &out) {
  if (chapters.ListSize() == 0)
    return;

  KaxTag *tag{};

  auto print_if_global = [&out, &tags, &tuid](char const *name, char const *format) {
    auto global = get_global_tag(name, tuid, tags);
    if (!global.empty())
      out.puts(fmt::format(format, global));
  };

  auto print_if_available = [&out, &tags, &tag, &tuid](char const *name, char const *format) {
    auto value = mtx::tags::get_simple_value(name, *tag);
    if (!value.empty() && (value != get_global_tag(name, tuid, tags)))
      out.puts(fmt::format(format, value));
  };

  auto print_comments = [&out, &tag](char const *prefix) {
    for (auto simple : *tag)
      if (Is<KaxTagSimple>(simple)
          && (   (mtx::tags::get_simple_name(*static_cast<KaxTagSimple *>(simple)) == "COMMENT")
              || (mtx::tags::get_simple_name(*static_cast<KaxTagSimple *>(simple)) == "COMMENTS")))
        out.puts(fmt::format("{0}REM \"{1}\"\n", prefix, mtx::tags::get_simple_value(*static_cast<KaxTagSimple *>(simple))));
  };

  if (mtx::hacks::is_engaged(mtx::hacks::NO_VARIABLE_DATA))
    file_name = "no-variable-data";

  out.write_bom("UTF-8");

  print_if_global("CATALOG",        "CATALOG {0}\n"); // until 0.9.6
  print_if_global("CATALOG_NUMBER", "CATALOG {0}\n"); // 0.9.7 and newer
  print_if_global("ARTIST",         "PERFORMER \"{0}\"\n");
  print_if_global("TITLE",          "TITLE \"{0}\"\n");
  print_if_global("DATE",           "REM DATE \"{0}\"\n"); // until 0.9.6
  print_if_global("DATE_RELEASED",  "REM DATE \"{0}\"\n"); // 0.9.7 and newer
  print_if_global("DISCID",         "REM DISCID {0}\n");

  tag = find_tag_for_track(-1, tuid, 0, tags);
  if (tag)
    print_comments("");

  out.puts(fmt::format("FILE \"{0}\" WAVE\n", file_name));

  size_t i;
  for (i = 0; i < chapters.ListSize(); i++) {
    KaxChapterAtom &atom =  *static_cast<KaxChapterAtom *>(chapters[i]);

    out.puts(fmt::format("  TRACK {0:02} AUDIO\n", i + 1));
    tag = find_tag_for_track(i + 1, tuid, mtx::chapters::get_uid(atom), tags);
    if (!tag)
      continue;

    print_if_available("TITLE",               "    TITLE \"{0}\"\n");
    print_if_available("ARTIST",              "    PERFORMER \"{0}\"\n");
    print_if_available("ISRC",                "    ISRC {0}\n");
    print_if_available("CDAUDIO_TRACK_FLAGS", "    FLAGS {0}\n");

    int k;
    for (k = 0; 100 > k; ++k) {
      int64_t temp_index = get_chapter_index(k, atom);
      if (-1 == temp_index)
        continue;

      out.puts(fmt::format("    INDEX {0:02} {1:02}:{2:02}:{3:02}\n",
                           k,
                            temp_index / 1'000'000 / 1'000  / 60,
                           (temp_index / 1'000'000 / 1'000) % 60,
                           std::llround(static_cast<double>(temp_index % 1'000'000'000ll) * 75.0 / 1'000'000'000.0)));
    }

    print_if_available("DATE",          "    REM DATE \"{0}\"\n"); // until 0.9.6
    // 0.9.7 and newer:
    print_if_available("DATE_RELEASED", "    REM DATE \"{0}\"\n");
    print_if_available("GENRE",         "    REM GENRE \"{0}\"\n");
    print_comments("    ");
  }
}

bool
extract_cuesheet(kax_analyzer_c &analyzer,
                 options_c::mode_options_c &options) {
  KaxChapters all_chapters;
  auto chapters_m       = analyzer.read_all(EBML_INFO(KaxChapters));
  auto tags_m           = analyzer.read_all(EBML_INFO(KaxTags));
  KaxChapters *chapters = dynamic_cast<KaxChapters *>(chapters_m.get());
  KaxTags *all_tags     = dynamic_cast<KaxTags *>(    tags_m.get());

  if (!chapters || !all_tags)
    return true;

  for (auto chapter_entry : *chapters) {
    if (!dynamic_cast<KaxEditionEntry *>(chapter_entry))
      continue;

    auto eentry = static_cast<KaxEditionEntry *>(chapter_entry);
    for (auto edition_entry : *eentry)
      if (dynamic_cast<KaxChapterAtom *>(edition_entry))
        all_chapters.PushElement(*edition_entry);
  }

  write_cuesheet(analyzer.get_file().get_file_name(), all_chapters, *all_tags, -1, *open_output_file(options.m_output_file_name));

  while (all_chapters.ListSize() > 0)
    all_chapters.Remove(0);

  return true;
}
