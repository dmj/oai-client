/*
 * Simple OAI-PMH client.
 *
 * Copyright (c) 2018 by David Maus <dmaus@dmaus.name>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef OAI_CLIENT_H
#define OAI_CLIENT_H

void log_info (const char *msg, ...);
void log_error (const char *msg, ...);

typedef struct buffer_t buffer_t;

buffer_t * make_buffer ();
void       reset_buffer (buffer_t *buf);
void       free_buffer (buffer_t *buf);

buffer_t * buffer_from_string (const char *str);
buffer_t * buffer_append (buffer_t *dst, const char *src, size_t size);
char     * buffer_to_string (const buffer_t *buf);
  
typedef struct error_t error_t;

error_t * make_error ();
void      free_error (error_t *err);

typedef struct docinfo_t docinfo_t;

docinfo_t * make_docinfo ();
void        docinfo_add_error (docinfo_t *docinfo, error_t *error);
void        free_docinfo (docinfo_t *docinfo);

#endif
