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

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
FILE *fdopen(int fd, const char *mode);

#include <string.h>
char *strdup(const char *s);

#include <unistd.h>
int getopt(int argc, const char **argv, const char *optstring);
extern char *optarg;
extern int opterr, optind, optopt;

#include <time.h>

typedef struct buffer_t {
  size_t  size;
  char   *data;
} buffer_t;

buffer_t *
make_buffer ()
{
  buffer_t *buffer = malloc(sizeof(buffer_t));
  buffer->size = 0;
  buffer->data = NULL;
  return buffer;
}

buffer_t *
buffer_from_string (const char *str)
{
  buffer_t *buffer = make_buffer();
  buffer->size = strlen(str);
  buffer->data = malloc(buffer->size);
  memcpy(buffer->data, str, buffer->size);
  return buffer;
}

buffer_t *
buffer_append (buffer_t *dst, const char *src, size_t size)
{
  dst->data = realloc(dst->data, dst->size + size);
  memcpy(dst->data + dst->size, src, size);
  dst->size += size;
  return dst;
}

void
reset_buffer (buffer_t *buf)
{
  buf->size = 0;
}

char *
buffer_to_string (const buffer_t *buf)
{
  char *str = malloc(1 + buf->size);
  memcpy(str, buf->data, buf->size);
  str[buf->size] = 0;
  return str;
}

void
free_buffer (buffer_t *buf)
{
  free(buf->data);
  free(buf);
}

#define _free_buffer(_buf_) \
  free_buffer(_buf_); \
  _buf_ = NULL;

typedef struct error_t {
  char *code;
  char *message;
} error_t;

typedef struct docinfo_t {
  int nr_of_errors;
  int nr_of_records;
  error_t **errors;
  char *token;
} docinfo_t;

error_t *
make_error ()
{
  error_t *error = malloc(sizeof(error_t));
  error->code = NULL;
  error->message = NULL;
  return error;
}

void
free_error (error_t *err)
{
  free(err->code);
  free(err->message);
  free(err);
}

#define _free_error(_err_) \
  free_error(_err_); \
  _err_ = NULL;

docinfo_t *
make_docinfo ()
{
  docinfo_t *docinfo = malloc(sizeof(docinfo_t));
  docinfo->token = NULL;
  docinfo->errors = NULL;
  docinfo->nr_of_errors = 0;
  docinfo->nr_of_records = 0;
  return docinfo;
}

void
docinfo_add_error (docinfo_t *docinfo, error_t *error)
{
  docinfo->nr_of_errors += 1;
  docinfo->errors = realloc(docinfo->errors, docinfo->nr_of_errors * sizeof(error_t*));
  docinfo->errors[docinfo->nr_of_errors - 1] = error;
}

void
free_docinfo (docinfo_t *doc)
{
  for (int i = 0; i < doc->nr_of_errors; i++) {
    _free_error(doc->errors[i]);
  }
  free(doc->token);
  free(doc);
}

#define _free_docinfo(_doc_) \
  free_docinfo(_doc_); \
  _doc_ = NULL;



#include "transfer.c"
#include "xmlparse.c"

static int verbose = 0;

static void
log_message (const char *tag, const char *msg, va_list args)
{
  fprintf(stderr, "%ju [%s] ", time(NULL), tag);
  vfprintf(stderr, msg, args);
  fputc('\n', stderr);
}

void
log_info (const char *msg, ...)
{
  if (verbose) {
    va_list args;
    va_start(args, msg);
    log_message("info", msg, args);
    va_end(args);
  }
}

void
log_error (const char *msg, ...)
{
  va_list args;
  va_start(args, msg);
  log_message("error", msg, args);
  va_end(args);
}

const static char *USAGE = "Usage: oai-client -b <baseUrl> -m <metadataPrefix> [-f <from>] [-o <outfile>] [-s <set>] [-u <until>\n\0";

static char *baseUrl = NULL;
static char *metadataPrefix = NULL;
static char *until = NULL;
static char *from = NULL;
static char *set = NULL;

static char *outfile = NULL;
static FILE *handle = NULL;
static char *url = NULL;

static void
parse_commandline (int argc, const char **argv)
{
  opterr = 0;
  int c;
  while ( (c = getopt(argc, argv, "b:m:s:f:u:o:v")) != -1) {
    switch (c) {
    case 'b':
      baseUrl = optarg;
      break;
    case 'm':
      metadataPrefix = optarg;
      break;
    case 's':
      set = optarg;
      break;
    case 'f':
      from = optarg;
      break;
    case 'u':
      until = optarg;
      break;
    case 'o':
      outfile = optarg;
      break;
    case 'v':
      verbose = 1;
      break;
    case '?':
      fprintf(stderr, USAGE);
      exit(EXIT_FAILURE);
    }
  }
}

static void
validate_arguments ()
{
  if (!baseUrl || !metadataPrefix) {
    fprintf(stderr, USAGE);
    exit(EXIT_FAILURE);
  }
}

#define _free(_dst_) \
  free(_dst_); \
  _dst_ = NULL;

#define _strcat(_dst_,_src_) \
  _dst_ = realloc(_dst_, 1 + strlen(_dst_) + strlen(_src_)); \
  _dst_ = strcat(_dst_, _src_);

static char *
build_resume_url (const char *baseUrl, const char *resumptionToken)
{
  char *url = strdup(baseUrl);
  _strcat(url, "?verb=ListRecords&resumptionToken=");
  _strcat(url, resumptionToken);
  return url;
}

static char *
build_request_url (const char *baseUrl, const char *metadataPrefix, const char *from, const char *until, const char *set)
{
  char *url = strdup(baseUrl);
  _strcat(url, "?verb=ListRecords&metadataPrefix=");
  _strcat(url, metadataPrefix);

  if (until) {
    _strcat(url, "&until=");
    _strcat(url, until);
  }
  if (from) {
    _strcat(url, "&from=");
    _strcat(url, from);
  }
  if (set) {
    _strcat(url, "&set=");
    _strcat(url, set);
  }
  return url;
}

static char *
fetch_records (const char *url, FILE *handle)
{
  buffer_t *body = fetch(url);
  docinfo_t *docinfo = parse(body);

  if (docinfo->nr_of_errors > 0) {
    for (int i = 0; i < docinfo->nr_of_errors; i++) {
      error_t *err = docinfo->errors[i];
      log_error("Protocol error: %s -- %s", err->code, err->message);
    }
  }
  if (docinfo->nr_of_records > 0) {
    log_info("Found %d records", docinfo->nr_of_records);
    serialize(body, handle);
  }

  char *token = NULL;
  if (docinfo->token) {
    token = strdup(docinfo->token);
    log_info("Found resumption token %s", token);
  }

  _free_docinfo(docinfo);
  _free_buffer(body);

  return token;
}

int main (int argc, const char **argv)
{
  parse_commandline(argc, argv);
  validate_arguments();

  if (outfile) {
    handle = fopen(outfile, "w");
  } else {
    handle = fdopen(1, "w");
  }
  fprintf(handle, "<records xmlns='tag:dmaus@dmaus.name,2018:oai-client'>");

  url = build_request_url(baseUrl, metadataPrefix, from, until, set);
  do {
    char *token = fetch_records(url, handle);
    _free(url);
    if (token) {
      url = build_resume_url(baseUrl, token);
      _free(token);
    }
  } while (url);

  fprintf(handle, "</records>");
  if (outfile) {
    fclose(handle);
  }

  exit(EXIT_SUCCESS);
}
