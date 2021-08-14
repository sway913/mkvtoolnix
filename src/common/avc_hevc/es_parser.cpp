/** AVC & HEVC ES parser base class

   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL v2
   see the file COPYING for details
   or visit https://www.gnu.org/licenses/old-licenses/gpl-2.0.html

   \author Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/avc_hevc/es_parser.h"
#include "common/strings/formatting.h"

namespace mtx::avc_hevc {

std::unordered_map<int, std::string> es_parser_c::ms_nalu_names_by_type;

es_parser_c::es_parser_c(std::string const &debug_type,
                         std::size_t num_slice_types,
                         std::size_t num_nalu_types)
  : m_debug_keyframe_detection{fmt::format("{0}_parser|{0}_keyframe_detection", debug_type, debug_type)}
  , m_debug_nalu_types{        fmt::format("{0}_parser|{0}_nalu_types",         debug_type, debug_type)}
  , m_debug_timestamps{        fmt::format("{0}_parser|{0}_timestamps",         debug_type, debug_type)}
  , m_debug_sps_info{          fmt::format("{0}_parser|{0}_sps|{0}_sps_info",   debug_type, debug_type, debug_type)}
  , m_stats{num_slice_types, num_nalu_types}
{
}

es_parser_c::~es_parser_c() {
}

void
es_parser_c::discard_actual_frames(bool discard) {
  m_discard_actual_frames = discard;
}

// void
// es_parser_c::add_bytes(unsigned char *buffer,
//                        size_t size) {
//   mtx::mem::slice_cursor_c cursor;
//   int marker_size              = 0;
//   int previous_marker_size     = 0;
//   int previous_pos             = -1;
//   uint64_t previous_parsed_pos = m_parsed_position;

//   if (m_unparsed_buffer && (0 != m_unparsed_buffer->get_size()))
//     cursor.add_slice(m_unparsed_buffer);
//   cursor.add_slice(buffer, size);

//   if (3 <= cursor.get_remaining_size()) {
//     uint32_t marker =                               1 << 24
//                     | (unsigned int)cursor.get_char() << 16
//                     | (unsigned int)cursor.get_char() <<  8
//                     | (unsigned int)cursor.get_char();

//     while (true) {
//       if (NALU_START_CODE == marker)
//         marker_size = 4;
//       else if (NALU_START_CODE == (marker & 0x00ffffff))
//         marker_size = 3;

//       if (0 != marker_size) {
//         if (-1 != previous_pos) {
//           int new_size = cursor.get_position() - marker_size - previous_pos - previous_marker_size;
//           auto nalu = memory_c::alloc(new_size);
//           cursor.copy(nalu->get_buffer(), previous_pos + previous_marker_size, new_size);
//           m_parsed_position = previous_parsed_pos + previous_pos;

//           mtx::mpeg::remove_trailing_zero_bytes(*nalu);
//           if (nalu->get_size())
//             handle_nalu(nalu, m_parsed_position);
//         }
//         previous_pos         = cursor.get_position() - marker_size;
//         previous_marker_size = marker_size;
//         marker_size          = 0;
//       }

//       if (!cursor.char_available())
//         break;

//       marker <<= 8;
//       marker  |= (unsigned int)cursor.get_char();
//     }
//   }

//   if (-1 == previous_pos)
//     previous_pos = 0;

//   m_stream_position += size;
//   m_parsed_position  = previous_parsed_pos + previous_pos;

//   int new_size = cursor.get_size() - previous_pos;
//   if (0 != new_size) {
//     m_unparsed_buffer = memory_c::alloc(new_size);
//     cursor.copy(m_unparsed_buffer->get_buffer(), previous_pos, new_size);

//   } else
//     m_unparsed_buffer.reset();
// }

void
es_parser_c::force_default_duration(int64_t default_duration) {
  m_forced_default_duration = default_duration;
}

bool
es_parser_c::is_default_duration_forced()
  const {
  return -1 != m_forced_default_duration;
}

void
es_parser_c::set_container_default_duration(int64_t default_duration) {
  m_container_default_duration = default_duration;
}

bool
es_parser_c::has_stream_default_duration()
  const {
  return -1 != m_stream_default_duration;
}

int64_t
es_parser_c::get_stream_default_duration()
  const {
  assert(-1 != m_stream_default_duration);
  return m_stream_default_duration;
}

void
es_parser_c::set_keep_ar_info(bool keep) {
  m_keep_ar_info = keep;
}

void
es_parser_c::set_nalu_size_length(int nalu_size_length) {
  m_nalu_size_length = nalu_size_length;
}

int
es_parser_c::get_nalu_size_length()
  const {
  return m_nalu_size_length;
}

bool
es_parser_c::frame_available() {
  return !m_frames_out.empty();
}

mtx::avc_hevc::frame_t
es_parser_c::get_frame() {
  assert(!m_frames_out.empty());

  auto frame = *m_frames_out.begin();
  m_frames_out.erase(m_frames_out.begin(), m_frames_out.begin() + 1);

  return frame;
}

bool
es_parser_c::configuration_record_changed()
  const {
  return m_configuration_record_changed;
}

void
es_parser_c::add_timestamp(int64_t timestamp) {
  m_provided_timestamps.emplace_back(timestamp, m_stream_position);
  ++m_stats.num_timestamps_in;
}

void
es_parser_c::flush_unhandled_nalus() {
  for (auto const &nalu_with_pos : m_unhandled_nalus)
    handle_nalu(nalu_with_pos.first, nalu_with_pos.second);

  m_unhandled_nalus.clear();
}

int
es_parser_c::get_num_skipped_frames()
  const {
  return m_num_skipped_frames;
}

int64_t
es_parser_c::get_most_often_used_duration()
  const {
  int64_t const s_common_default_durations[] = {
    1000000000ll / 50,
    1000000000ll / 25,
    1000000000ll / 60,
    1000000000ll / 30,
    1000000000ll * 1001 / 48000,
    1000000000ll * 1001 / 24000,
    1000000000ll * 1001 / 60000,
    1000000000ll * 1001 / 30000,
  };

  auto most_often = m_duration_frequency.begin();
  for (auto current = m_duration_frequency.begin(); m_duration_frequency.end() != current; ++current)
    if (current->second > most_often->second)
      most_often = current;

  // No duration at all!? No frame?
  if (m_duration_frequency.end() == most_often) {
    mxdebug_if(m_debug_timestamps, fmt::format("Duration frequency: none found, using 25 FPS\n"));
    return 1000000000ll / 25;
  }

  auto best = std::make_pair(most_often->first, std::numeric_limits<uint64_t>::max());

  for (auto common_default_duration : s_common_default_durations) {
    uint64_t diff = std::abs(most_often->first - common_default_duration);
    if ((diff < 20000) && (diff < best.second))
      best = std::make_pair(common_default_duration, diff);
  }

  mxdebug_if(m_debug_timestamps,
             fmt::format("Duration frequency. Result: {0}, diff {1}. Best before adjustment: {2}. All: {3}\n",
                         best.first, best.second, most_often->first,
                         std::accumulate(m_duration_frequency.begin(), m_duration_frequency.end(), ""s, [](auto const &accu, auto const &pair) {
                           return accu + fmt::format(" <{0} {1}>", pair.first, pair.second);
                         })));

  return best.first;
}

std::vector<int64_t>
es_parser_c::calculate_provided_timestamps_to_use() {
  auto frame_idx                     = 0u;
  auto provided_timestamps_idx       = 0u;
  auto const num_frames              = m_frames.size();
  auto const num_provided_timestamps = m_provided_timestamps.size();

  std::vector<int64_t> provided_timestamps_to_use;
  provided_timestamps_to_use.reserve(num_frames);

  while (   (frame_idx               < num_frames)
         && (provided_timestamps_idx < num_provided_timestamps)) {
    timestamp_c timestamp_to_use;
    auto &frame = m_frames[frame_idx];

    while (   (provided_timestamps_idx < num_provided_timestamps)
           && (frame.m_position >= m_provided_timestamps[provided_timestamps_idx].second)) {
      timestamp_to_use = timestamp_c::ns(m_provided_timestamps[provided_timestamps_idx].first);
      ++provided_timestamps_idx;
    }

    if (timestamp_to_use.valid()) {
      provided_timestamps_to_use.emplace_back(timestamp_to_use.to_ns());
      frame.m_has_provided_timestamp = true;
    }

    ++frame_idx;
  }

  mxdebug_if(m_debug_timestamps,
             fmt::format("cleanup; num frames {0} num provided timestamps available {1} num provided timestamps to use {2}\n"
                         "  frames:\n{3}"
                         "  provided timestamps (available):\n{4}"
                         "  provided timestamps (to use):\n{5}",
                         num_frames, num_provided_timestamps, provided_timestamps_to_use.size(),
                         std::accumulate(m_frames.begin(), m_frames.end(), std::string{}, [](auto const &str, auto const &frame) {
                           return str + fmt::format("    pos {0} size {1} type {2}\n", frame.m_position, frame.m_data->get_size(), frame.m_type);
                         }),
                         std::accumulate(m_provided_timestamps.begin(), m_provided_timestamps.end(), std::string{}, [](auto const &str, auto const &provided_timestamp) {
                           return str + fmt::format("    pos {0} timestamp {1}\n", provided_timestamp.second, mtx::string::format_timestamp(provided_timestamp.first));
                         }),
                         std::accumulate(provided_timestamps_to_use.begin(), provided_timestamps_to_use.end(), std::string{}, [](auto const &str, auto const &provided_timestamp) {
                           return str + fmt::format("    timestamp {0}\n", mtx::string::format_timestamp(provided_timestamp));
                         })));

  m_provided_timestamps.erase(m_provided_timestamps.begin(), m_provided_timestamps.begin() + provided_timestamps_idx);

  std::sort(provided_timestamps_to_use.begin(), provided_timestamps_to_use.end());

  return provided_timestamps_to_use;
}

void
es_parser_c::cleanup() {
  auto num_frames = m_frames.size();
  if (!num_frames)
    return;

  if (m_discard_actual_frames) {
    m_stats.num_frames_discarded    += m_frames.size();
    m_stats.num_timestamps_discarded += m_provided_timestamps.size();

    m_frames.clear();
    m_provided_timestamps.clear();

    return;
  }

  calculate_frame_order();
  calculate_frame_timestamps_references_and_update_stats();

  if (m_first_cleanup && !m_frames.front().m_keyframe) {
    // Drop all frames before the first key frames as they cannot be
    // decoded anyway.
    m_stats.num_frames_discarded += m_frames.size();
    m_frames.clear();

    return;
  }

  m_first_cleanup         = false;
  m_stats.num_frames_out += m_frames.size();
  m_frames_out.insert(m_frames_out.end(), m_frames.begin(), m_frames.end());
  m_frames.clear();
}

bool
es_parser_c::has_par_been_found()
  const {
  assert(m_configuration_record_ready);
  return m_par_found;
}

mtx_mp_rational_t const &
es_parser_c::get_par()
  const {
  assert(m_configuration_record_ready && m_par_found);
  return m_par;
}

std::pair<int64_t, int64_t> const
es_parser_c::get_display_dimensions(int width,
                                    int height)
  const {
  assert(m_configuration_record_ready && m_par_found);

  if (0 >= width)
    width = get_width();
  if (0 >= height)
    height = get_height();

  return std::make_pair<int64_t, int64_t>(1 <= m_par ? mtx::to_int_rounded(width * m_par) : width,
                                          1 <= m_par ? height                             : mtx::to_int_rounded(height / m_par));
}

size_t
es_parser_c::get_num_field_slices()
  const {
  return m_stats.num_field_slices;
}

size_t
es_parser_c::get_num_frame_slices()
  const {
  return m_stats.num_frame_slices;
}

std::string
es_parser_c::get_nalu_type_name(int type) {
  init_nalu_names();

  auto name = ms_nalu_names_by_type.find(type);
  return (ms_nalu_names_by_type.end() == name) ? "unknown" : name->second;
}

}
