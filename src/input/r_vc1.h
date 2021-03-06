/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL v2
   see the file COPYING for details
   or visit https://www.gnu.org/licenses/old-licenses/gpl-2.0.html

   class definitions for the VC1 ES demultiplexer module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#pragma once

#include "common/common_pch.h"

#include "merge/generic_reader.h"
#include "common/vc1.h"

class vc1_es_reader_c: public generic_reader_c {
private:
  memory_cptr m_raw_seqhdr;

  memory_cptr m_buffer;

  mtx::vc1::sequence_header_t m_seqhdr;

public:
  vc1_es_reader_c();

  virtual mtx::file_type_e get_format_type() const {
    return mtx::file_type_e::vc1;
  }

  virtual void read_headers();
  virtual void identify();
  virtual void create_packetizer(int64_t id);
  virtual bool is_providing_timestamps() const {
    return false;
  }

  virtual bool probe_file() override;

protected:
  virtual file_status_e read(generic_packetizer_c *packetizer, bool force = false) override;
};
