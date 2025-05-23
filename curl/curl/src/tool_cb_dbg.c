/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * Copyright (C) Daniel Stenberg, <daniel@haxx.se>, et al.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution. The terms
 * are also available at https://curl.se/docs/copyright.html.
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so, under the terms of the COPYING file.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 * SPDX-License-Identifier: curl
 *
 ***************************************************************************/
#include "tool_setup.h"

#define ENABLE_CURLX_PRINTF
/* use our own printf() functions */
#include "curlx.h"

#include "tool_cfgable.h"
#include "tool_msgs.h"
#include "tool_cb_dbg.h"
#include "tool_util.h"
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE || TARGET_OS_WATCH || TARGET_OS_TV || TARGET_OS_MACCATALYST
#include "ios_error.h"
#undef stderr
#define stderr thread_stderr
#undef stdout
#define stdout thread_stdout
#endif

#include "memdebug.h" /* keep this as LAST include */

static void dump(const char *timebuf, const char *text,
                 FILE *stream, const unsigned char *ptr, size_t size,
                 trace tracetype, curl_infotype infotype);

/*
 * Return the formatted HH:MM:SS for the tv_sec given.
 * NOT thread safe.
 */
static const char *hms_for_sec(time_t tv_sec)
{
  static time_t cached_tv_sec;
  static char hms_buf[12];
  static time_t epoch_offset;
  static int known_epoch;

  if(tv_sec != cached_tv_sec) {
    struct tm *now;
    time_t secs;
    /* recalculate */
    if(!known_epoch) {
      epoch_offset = time(NULL) - tv_sec;
      known_epoch = 1;
    }
    secs = epoch_offset + tv_sec;
    /* !checksrc! disable BANNEDFUNC 1 */
    now = localtime(&secs);  /* not thread safe but we don't care */
    msnprintf(hms_buf, sizeof(hms_buf), "%02d:%02d:%02d",
              now->tm_hour, now->tm_min, now->tm_sec);
    cached_tv_sec = tv_sec;
  }
  return hms_buf;
}

static void log_line_start(FILE *log, const char *intro, curl_infotype type)
{
  /*
   * This is the trace look that is similar to what libcurl makes on its
   * own.
   */
  static const char * const s_infotype[] = {
    "* ", "< ", "> ", "{ ", "} ", "{ ", "} "
  };
  if(intro && *intro)
    fprintf(log, "%s%s", intro, s_infotype[type]);
  else
    fputs(s_infotype[type], log);
}

/*
** callback for CURLOPT_DEBUGFUNCTION
*/
int tool_debug_cb(CURL *handle, curl_infotype type,
                  char *data, size_t size,
                  void *userdata)
{
  struct OperationConfig *operation = userdata;
  struct GlobalConfig *config = operation->global;
  FILE *output = stderr;
  const char *text;
  struct timeval tv;
  char timebuf[20];

  (void)handle; /* not used */

  if(config->tracetime) {
    tv = tvnow();
    msnprintf(timebuf, sizeof(timebuf), "%s.%06ld ",
              hms_for_sec(tv.tv_sec), (long)tv.tv_usec);
  }
  else
    timebuf[0] = 0;

  if(!config->trace_stream) {
    /* open for append */
    if(!strcmp("-", config->trace_dump))
      config->trace_stream = stdout;
    else if(!strcmp("%", config->trace_dump))
      /* Ok, this is somewhat hackish but we do it undocumented for now */
      config->trace_stream = stderr;
    else {
      config->trace_stream = fopen(config->trace_dump, FOPEN_WRITETEXT);
      config->trace_fopened = TRUE;
    }
  }

  if(config->trace_stream)
    output = config->trace_stream;

  if(!output) {
    warnf(config, "Failed to create/open output");
    return 0;
  }

  if(config->tracetype == TRACE_PLAIN) {
    static bool newl = FALSE;
    static bool traced_data = FALSE;

    switch(type) {
    case CURLINFO_HEADER_OUT:
      if(size > 0) {
        size_t st = 0;
        size_t i;
        for(i = 0; i < size - 1; i++) {
          if(data[i] == '\n') { /* LF */
            if(!newl) {
              log_line_start(output, timebuf, type);
            }
            (void)fwrite(data + st, i - st + 1, 1, output);
            st = i + 1;
            newl = FALSE;
          }
        }
        if(!newl)
          log_line_start(output, timebuf, type);
        (void)fwrite(data + st, i - st + 1, 1, output);
      }
      newl = (size && (data[size - 1] != '\n')) ? TRUE : FALSE;
      traced_data = FALSE;
      break;
    case CURLINFO_TEXT:
    case CURLINFO_HEADER_IN:
      if(!newl)
        log_line_start(output, timebuf, type);
      (void)fwrite(data, size, 1, output);
      newl = (size && (data[size - 1] != '\n')) ? TRUE : FALSE;
      traced_data = FALSE;
      break;
    case CURLINFO_DATA_OUT:
    case CURLINFO_DATA_IN:
    case CURLINFO_SSL_DATA_IN:
    case CURLINFO_SSL_DATA_OUT:
      if(!traced_data) {
        /* if the data is output to a tty and we're sending this debug trace
           to stderr or stdout, we don't display the alert about the data not
           being shown as the data _is_ shown then just not via this
           function */
        if(!config->isatty || ((output != stderr) && (output != stdout))) {
          if(!newl)
            log_line_start(output, timebuf, type);
          fprintf(output, "[%zu bytes data]\n", size);
          newl = FALSE;
          traced_data = TRUE;
        }
      }
      break;
    default: /* nada */
      newl = FALSE;
      traced_data = FALSE;
      break;
    }

    return 0;
  }

  switch(type) {
  case CURLINFO_TEXT:
    fprintf(output, "%s== Info: %.*s", timebuf, (int)size, data);
    /* FALLTHROUGH */
  default: /* in case a new one is introduced to shock us */
    return 0;

  case CURLINFO_HEADER_OUT:
    text = "=> Send header";
    break;
  case CURLINFO_DATA_OUT:
    text = "=> Send data";
    break;
  case CURLINFO_HEADER_IN:
    text = "<= Recv header";
    break;
  case CURLINFO_DATA_IN:
    text = "<= Recv data";
    break;
  case CURLINFO_SSL_DATA_IN:
    text = "<= Recv SSL data";
    break;
  case CURLINFO_SSL_DATA_OUT:
    text = "=> Send SSL data";
    break;
  }

  dump(timebuf, text, output, (unsigned char *) data, size, config->tracetype,
       type);
  return 0;
}

static void dump(const char *timebuf, const char *text,
                 FILE *stream, const unsigned char *ptr, size_t size,
                 trace tracetype, curl_infotype infotype)
{
  size_t i;
  size_t c;

  unsigned int width = 0x10;

  if(tracetype == TRACE_ASCII)
    /* without the hex output, we can fit more on screen */
    width = 0x40;

  fprintf(stream, "%s%s, %zu bytes (0x%zx)\n", timebuf, text, size, size);

  for(i = 0; i < size; i += width) {

    fprintf(stream, "%04zx: ", i);

    if(tracetype == TRACE_BIN) {
      /* hex not disabled, show it */
      for(c = 0; c < width; c++)
        if(i + c < size)
          fprintf(stream, "%02x ", ptr[i + c]);
        else
          fputs("   ", stream);
    }

    for(c = 0; (c < width) && (i + c < size); c++) {
      /* check for 0D0A; if found, skip past and start a new line of output */
      if((tracetype == TRACE_ASCII) &&
         (i + c + 1 < size) && (ptr[i + c] == 0x0D) &&
         (ptr[i + c + 1] == 0x0A)) {
        i += (c + 2 - width);
        break;
      }
      (void)infotype;
      fprintf(stream, "%c", ((ptr[i + c] >= 0x20) && (ptr[i + c] < 0x7F)) ?
              ptr[i + c] : UNPRINTABLE_CHAR);
      /* check again for 0D0A, to avoid an extra \n if it's at width */
      if((tracetype == TRACE_ASCII) &&
         (i + c + 2 < size) && (ptr[i + c + 1] == 0x0D) &&
         (ptr[i + c + 2] == 0x0A)) {
        i += (c + 3 - width);
        break;
      }
    }
    fputc('\n', stream); /* newline */
  }
  fflush(stream);
}
