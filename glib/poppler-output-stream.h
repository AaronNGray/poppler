/* poppler-output-stream.h: glib interface to poppler
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.
 */

#pragma once

#include <gio/gio.h>

#ifndef __GI_SCANNER__
#    include <Object.h>
#    include <Stream.h>

class GLibOutStream : public OutStream
{
public:
    explicit GLibOutStream(GOutputStream *stream_, GCancellable *cancellable, GError **error);

    ~GLibOutStream() override;

    void close() override;

    Goffset getPos() override;
    void put(char c) override;

    size_t write(std::span<unsigned char> data) override;

    void printf(const char *format, ...) override GCC_PRINTF_FORMAT(2, 3);

    void flush();

private:
    GOutputStream *stream;
    Goffset pos;
    unsigned char buffer[1024];
    size_t inbuf;
    GCancellable *cancellable;
    GError **error;
};

#endif