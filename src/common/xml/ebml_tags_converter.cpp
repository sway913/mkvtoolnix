/*
  mkvmerge -- utility for splicing together matroska files
  from component media subtypes

  Distributed under the GPL v2
  see the file COPYING for details
  or visit https://www.gnu.org/licenses/old-licenses/gpl-2.0.html

  EBML/XML converter specialization for tags

  Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/bcp47.h"
#include "common/iso639.h"
#include "common/mm_io_x.h"
#include "common/strings/formatting.h"
#include "common/xml/ebml_tags_converter.h"

using namespace libmatroska;

namespace mtx::xml {

ebml_tags_converter_c::ebml_tags_converter_c()
{
  setup_maps();
}

ebml_tags_converter_c::~ebml_tags_converter_c() {
}

void
ebml_tags_converter_c::setup_maps() {
  m_debug_to_tag_name_map["TagTargets"]         = "Targets";
  m_debug_to_tag_name_map["TagTrackUID"]        = "TrackUID";
  m_debug_to_tag_name_map["TagEditionUID"]      = "EditionUID";
  m_debug_to_tag_name_map["TagChapterUID"]      = "ChapterUID";
  m_debug_to_tag_name_map["TagAttachmentUID"]   = "AttachmentUID";
  m_debug_to_tag_name_map["TagTargetType"]      = "TargetType";
  m_debug_to_tag_name_map["TagTargetTypeValue"] = "TargetTypeValue";
  m_debug_to_tag_name_map["TagSimple"]          = "Simple";
  m_debug_to_tag_name_map["TagName"]            = "Name";
  m_debug_to_tag_name_map["TagString"]          = "String";
  m_debug_to_tag_name_map["TagBinary"]          = "Binary";
  m_debug_to_tag_name_map["TagLanguage"]        = "TagLanguage";
  m_debug_to_tag_name_map["TagLanguageIETF"]    = "TagLanguageIETF";
  m_debug_to_tag_name_map["TagDefault"]         = "DefaultLanguage";

  m_limits["TagDefault"]                        = limits_t{ true, true, 0, 1 };
  m_limits["TagLanguageIETF"]                   = limits_t{ true, true, 0, 1 };

  reverse_debug_to_tag_name_map();

  if (debugging_c::requested("ebml_converter_semantics"))
    dump_semantics("Tags");
}

void
ebml_tags_converter_c::write_xml(KaxTags &tags,
                                 mm_io_c &out) {
  document_cptr doc(new pugi::xml_document);

  doc->append_child(pugi::node_comment).set_value(" <!DOCTYPE Tags SYSTEM \"matroskatags.dtd\"> ");

  ebml_tags_converter_c converter;
  converter.to_xml(tags, doc);

  out.write_bom("UTF-8");

  std::stringstream out_stream;
  doc->save(out_stream, "  ");
  out.puts(out_stream.str());
}

void
ebml_tags_converter_c::fix_ebml(EbmlMaster &root)
  const {
  for (auto child : root)
    if (dynamic_cast<KaxTag *>(child))
      fix_tag(*static_cast<KaxTag *>(child));
}

void
ebml_tags_converter_c::fix_tag(KaxTag &tag)
  const {
  auto have_simple_tag = false;

  for (auto child : tag) {
    if (dynamic_cast<KaxTag *>(child))
      fix_tag(*static_cast<KaxTag *>(child));

    else if (dynamic_cast<KaxTagSimple *>(child)) {
      fix_simple_tag(*static_cast<KaxTagSimple *>(child));
      have_simple_tag = true;
    }
  }

  if (!have_simple_tag)
    throw conversion_x{ Y("<Tag> is missing the <Simple> child.") };
}

void
ebml_tags_converter_c::fix_simple_tag(KaxTagSimple &simple_tag)
  const {
  if (!FindChild<KaxTagName>(simple_tag))
    throw conversion_x{ Y("<Simple> is missing the <Name> child.") };

  auto string = FindChild<KaxTagString>(simple_tag);
  auto binary = FindChild<KaxTagBinary>(simple_tag);
  if (string && binary)
    throw conversion_x{ Y("Only one of <String> and <Binary> may be used beneath <Simple> but not both at the same time.") };
  if (!string && !binary && !FindChild<KaxTagSimple>(simple_tag))
    throw conversion_x{ Y("<Simple> must contain either a <String> or a <Binary> child.") };

  auto tlanguage_ietf  = FindChild<KaxTagLanguageIETF>(simple_tag);
  auto tlanguage       = FindChild<KaxTagLangue>(simple_tag);
  auto value_to_parse  = tlanguage_ietf ? tlanguage_ietf->GetValue()
                       : tlanguage      ? tlanguage->GetValue()
                       :                  "und"s;
  auto parsed_language = mtx::bcp47::language_c::parse(value_to_parse);

  if (!parsed_language.is_valid())
    throw conversion_x{fmt::format(Y("'{0}' is not a valid IETF BCP 47/RFC 5646 language tag. Additional information from the parser: {1}"), value_to_parse, parsed_language.get_error())};

  if (!tlanguage_ietf && !mtx::bcp47::language_c::is_disabled()) {
    tlanguage_ietf = new KaxTagLanguageIETF;
    simple_tag.PushElement(*tlanguage_ietf);
  }

  if (tlanguage_ietf)
    tlanguage_ietf->SetValue(parsed_language.format());

  if (!tlanguage) {
    tlanguage = new KaxTagLangue;
    simple_tag.PushElement(*tlanguage);
  }

  tlanguage->SetValue(parsed_language.get_closest_iso639_2_alpha_3_code());
}

std::shared_ptr<KaxTags>
ebml_tags_converter_c::parse_file(std::string const &file_name,
                                  bool throw_on_error) {
  auto parse = [&file_name]() -> auto {
    auto master = ebml_tags_converter_c{}.to_ebml(file_name, "Tags");
    fix_mandatory_elements(static_cast<KaxTags *>(master.get()));
    return std::dynamic_pointer_cast<KaxTags>(master);
  };

  if (throw_on_error)
    return parse();

  try {
    return parse();

  } catch (mtx::mm_io::exception &ex) {
    mxerror(fmt::format(Y("The XML tag file '{0}' could not be read.\n"), file_name));

  } catch (mtx::xml::xml_parser_x &ex) {
    mxerror(fmt::format(Y("The XML tag file '{0}' contains an error at position {2}: {1}\n"), file_name, ex.result().description(), ex.result().offset));

  } catch (mtx::xml::exception &ex) {
    mxerror(fmt::format(Y("The XML tag file '{0}' contains an error: {1}\n"), file_name, ex.what()));
  }

  return {};
}

}
