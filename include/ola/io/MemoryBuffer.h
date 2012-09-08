/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * MemoryBuffer.h
 * An derrived class of InputBuffer that wraps a memory buffer.
 * Copyright (C) 2012 Simon Newton
 */

#ifndef INCLUDE_OLA_IO_MEMORYBUFFER_H_
#define INCLUDE_OLA_IO_MEMORYBUFFER_H_

#include <ola/io/InputBuffer.h>
#include <stdint.h>
#include <algorithm>

namespace ola {
namespace io {

/**
 * Wraps a block of memory and presents the InputBuffer interface. This does
 * not free the memory when the object is destroyed.
 */
class MemoryBuffer: public InputBuffer {
  public:
    explicit MemoryBuffer(const uint8_t *data, unsigned int size)
        : m_data(data),
          m_size(size),
          m_cursor(0) {
    }
    ~MemoryBuffer() {}

    void Get(uint8_t *data, unsigned int *length) {
      *length = std::min(m_size - m_cursor, *length);
      memcpy(data, m_data + m_cursor, *length);
      m_cursor += *length;
    }

  private:
    const uint8_t *m_data;
    const unsigned int m_size;
    unsigned int m_cursor;

    MemoryBuffer(const MemoryBuffer&);
    MemoryBuffer& operator=(const MemoryBuffer&);
};
}  // io
}  // ola
#endif  // INCLUDE_OLA_IO_MEMORYBUFFER_H_