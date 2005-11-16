/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   class definition for the video output module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __P_VIDEO_H
#define __P_VIDEO_H

#include "os.h"

#include "common.h"
#include "mpeg4_common.h"
#include "pr_generic.h"
#include "M2VParser.h"

#define VFT_IFRAME -1
#define VFT_PFRAMEAUTOMATIC -2
#define VFT_NOBFRAME -1

class video_packetizer_c: public generic_packetizer_c {
protected:
  double fps;
  int width, height, frames_output;
  int64_t ref_timecode, duration_shift;
  bool pass_through;

public:
  video_packetizer_c(generic_reader_c *_reader, const char *_codec_id,
                     double _fps, int _width, int _height,
                     track_info_c &_ti);

  virtual int process(packet_cptr packet);
  virtual void set_headers();

  virtual void dump_debug_info();

  virtual const char *get_format_name() {
    return "video";
  }
  virtual connection_result_e can_connect_to(generic_packetizer_c *src,
                                             string &error_message);

protected:
  virtual void check_fourcc();
};

class mpeg1_2_video_packetizer_c: public video_packetizer_c {
protected:
  M2VParser parser;
  bool framed, aspect_ratio_extracted;

public:
  mpeg1_2_video_packetizer_c(generic_reader_c *_reader, int _version,
                             double _fps, int _width, int _height,
                             int _dwidth, int _dheight, bool _framed,
                             track_info_c &_ti);

  virtual int process(packet_cptr packet);
  virtual void flush();

protected:
  virtual void extract_fps(const unsigned char *buffer, int size);
  virtual void extract_aspect_ratio(const unsigned char *buffer, int size);
  virtual void create_private_data();
};

class mpeg4_p2_video_packetizer_c: public video_packetizer_c {
protected:
  deque<video_frame_t> queued_frames;
  deque<int64_t> available_timecodes, available_durations;
  int64_t timecodes_generated, last_i_p_frame, previous_timecode;
  bool aspect_ratio_extracted, input_is_native, output_is_native;
  bool size_extracted;

public:
  mpeg4_p2_video_packetizer_c(generic_reader_c *_reader,
                              double _fps, int _width, int _height,
                              bool _input_is_native, track_info_c &_ti);

  virtual int process(packet_cptr packet);
  virtual void flush();

protected:
  virtual int process_native(packet_cptr packet);
  virtual int process_non_native(packet_cptr packet);
  virtual void flush_frames_maybe(frame_type_e next_frame);
  virtual void flush_frames(bool end_of_file);
  virtual void extract_aspect_ratio(const unsigned char *buffer, int size);
  virtual void extract_size(const unsigned char *buffer, int size);
  virtual void fix_codec_string();
  virtual void handle_missing_timecodes(bool end_of_file);
};

class mpeg4_p10_video_packetizer_c: public video_packetizer_c {
public:
  mpeg4_p10_video_packetizer_c(generic_reader_c *_reader,
                               double _fps, int _width, int _height,
                               track_info_c &_ti);
  virtual int process(packet_cptr packet);

  virtual connection_result_e can_connect_to(generic_packetizer_c *src,
                                             string &error_message);

protected:
  virtual void extract_aspect_ratio();
};

#endif // __P_VIDEO_H
