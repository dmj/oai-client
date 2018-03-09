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

#include "oai-client.h"

#include <stdio.h>
#include <expat.h>

static void
serialize_write_escape (FILE *file, const XML_Char *str, int size)
{
  for (int i = 0; i < size; i++) {
    switch (str[i]) {
    case '<':
      fputs("&lt;", file);
      break;
    case '&':
      fputs("&amp;", file);
      break;
    case '"':
      fputs("&quot;", file);
      break;
    case 39:
      fputs("&apos;", file);
      break;
    default:
      fputc(str[i], file);
    }
  }
}

static void XMLCALL
serialize_element_start_callback (void *userData, const XML_Char *name, const XML_Char **atts)
{
  fprintf((FILE*)userData, "<%s", name);
  if (atts[0]) {
    for (int i = 0; atts[i]; i += 2) {
      fprintf((FILE*)userData, " %s=\"", atts[i]);
      serialize_write_escape((FILE*)userData, atts[i + 1], strlen(atts[i + 1]));
      fprintf((FILE*)userData, "\"");
    }
  }
  fprintf((FILE*)userData, ">");
}

static void XMLCALL
serialize_element_end_callback (void *userData, const XML_Char *name)
{
  fprintf((FILE*)userData, "</%s>", name);
}

static void XMLCALL
serialize_cdata_callback (void *userData, const XML_Char *str, int size)
{
  serialize_write_escape((FILE*)userData, str, size);
}

void
serialize (const buffer_t *document, FILE *file)
{

  XML_Parser parser = XML_ParserCreate(NULL);

  XML_SetElementHandler(parser, serialize_element_start_callback, serialize_element_end_callback);
  XML_SetCharacterDataHandler(parser, serialize_cdata_callback);
  XML_SetUserData(parser, file);

  if (XML_Parse(parser, document->data, document->size, 1) == XML_STATUS_ERROR) {
    log_error("Error parsing XML document: %s", XML_ErrorString(XML_GetErrorCode(parser)));
    exit(EXIT_FAILURE);
  }

  XML_ParserFree(parser);

}



#define PSTATE_CAPTURE_NONE   0
#define PSTATE_CAPTURE_ERROR  1
#define PSTATE_CAPTURE_TOKEN  2

typedef struct pstate_t {
  docinfo_t *docinfo;
  buffer_t *cdata;
  error_t *error;
  int state;
} pstate_t;

pstate_t *
make_pstate ()
{
  pstate_t *pstate = malloc(sizeof(pstate_t));
  pstate->docinfo = NULL;
  pstate->cdata = NULL;
  pstate->error = NULL;
  pstate->state = PSTATE_CAPTURE_NONE;
  return pstate;
}

void
free_pstate (pstate_t *pstate)
{
  if (pstate->error) {
    free_error(pstate->error);
  }
  if (pstate->docinfo) {
    free_docinfo(pstate->docinfo);
  }
  free(pstate->cdata);
  free(pstate);
}

static void XMLCALL
parse_element_start_callback (void *userData, const XML_Char *name, const XML_Char **atts)
{
  pstate_t *pstate = userData;

  if (strcmp(name, "http://www.openarchives.org/OAI/2.0/ record") == 0) {
    pstate->docinfo->nr_of_records++;
  } else if (strcmp(name, "http://www.openarchives.org/OAI/2.0/ error") == 0) {
    pstate->error = make_error();
    for (int i = 0; atts[i]; i += 2) {
      if (strcmp(atts[i], "code") == 0) {
        pstate->error->code = strdup(atts[i + 1]);
      }
    }
    pstate->state = PSTATE_CAPTURE_ERROR;
  } else if (strcmp(name, "http://www.openarchives.org/OAI/2.0/ resumptionToken") == 0) {
    pstate->state = PSTATE_CAPTURE_TOKEN;
  }
}

static void XMLCALL
parse_element_end_callback (void *userData, const XML_Char *name)
{
  pstate_t *pstate = userData;
  if (pstate->state == PSTATE_CAPTURE_TOKEN) {
    pstate->docinfo->token = buffer_to_string(pstate->cdata);
    reset_buffer(pstate->cdata);
    pstate->state = PSTATE_CAPTURE_NONE;
  } else if (pstate->state == PSTATE_CAPTURE_ERROR) {
    pstate->error->message = buffer_to_string(pstate->cdata);
    docinfo_add_error(pstate->docinfo, pstate->error);
    reset_buffer(pstate->cdata);
    pstate->error = NULL;
    pstate->state = PSTATE_CAPTURE_NONE;
  }
}

static void XMLCALL
parse_cdata_callback (void *userData, const XML_Char *str, int size)
{
  pstate_t *pstate = userData;
  if (pstate->state) {
    pstate->cdata = buffer_append(pstate->cdata, str, size);
  }
}

docinfo_t *
parse (buffer_t *document)
{
  docinfo_t *docinfo = make_docinfo();
  pstate_t *pstate = make_pstate();

  pstate->docinfo = docinfo;
  pstate->cdata = make_buffer();

  XML_Parser parser = XML_ParserCreateNS(NULL, ' ');

  XML_SetElementHandler(parser, parse_element_start_callback, parse_element_end_callback);
  XML_SetCharacterDataHandler(parser, parse_cdata_callback);
  XML_SetUserData(parser, pstate);

  if (XML_Parse(parser, document->data, document->size, 1) == XML_STATUS_ERROR) {
    log_error("Error parsing XML document: %s", XML_ErrorString(XML_GetErrorCode(parser)));
    exit(EXIT_FAILURE);
  }

  XML_ParserFree(parser);
  pstate->docinfo = NULL;
  pstate->error = NULL;
  free_pstate(pstate);

  return docinfo;
}
