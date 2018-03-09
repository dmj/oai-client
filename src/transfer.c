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

#include <curl/curl.h>

static CURL *curl = NULL;

static size_t
curl_callback (char *str, size_t size, size_t nmemb, void *buffer)
{
  for (size_t i = 0; i < nmemb; i++) {
    buffer = buffer_append(buffer, str, size);
    str += size;
  }
  return size * nmemb;
}

buffer_t *
fetch (const char *url)
{
  buffer_t *body = make_buffer();
  if (!curl) {
    curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_callback);
  }
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, body);
  curl_easy_setopt(curl, CURLOPT_URL, url);
  log_info("GET %s", url);
  CURLcode error = curl_easy_perform(curl);
  if (error) {
    exit(EXIT_FAILURE);
  }
  return body;
}
