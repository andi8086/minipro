/*
 * database.c - Functions for dealing with the jedec files.
 *
 * This file is a part of Minipro.
 *
 * Minipro is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Minipro is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "jedec.h"
#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* Parse uint32 value from char buffer */
static int parse_uint32(const char *buffer, uint32_t *value, char **pEnd,
                        uint8_t radix) {
  errno = 0;
  char *p_end;
  *value = strtoul(buffer, &p_end, radix);
  if (pEnd != NULL) *pEnd = p_end;
  if (p_end == buffer) return BAD_FORMAT;
  return errno;
}
/*
 This function will parse the numeric value of the given command.
 If found, will return the parsed value and a pointer after the last character
 parsed.
 */
static int parse_token(const char *buffer, const char *token_name,
                       size_t token_size, uint32_t *value, char **pEnd,
                       uint8_t radix) {
  // Compare the first 'token_size' characters ignoring case.
  if (strncasecmp(buffer, token_name, token_size)) return TOKEN_NOT_FOUND;

  // If found parse the numeric value next to it
  if (!parse_uint32(buffer + token_size, value, pEnd, radix)) return NO_ERROR;
  return BAD_FORMAT;
}

/*
 This function will parse each recognized jedec command found in the buffer.
 Supported commands: QP, QF, F, G, L, C
 It was adapted for most horrible formated jedec files i have found.
 */
static int parse_tokens(char *buffer, size_t buffer_size, jedec_t *jedec,
                        uint16_t *checksum) {
  // some state machine helpers
  uint8_t is_QP_set = 0;
  uint8_t is_QF_set = 0;
  uint8_t is_F_set = 0;
  uint8_t is_G_set = 0;
  uint8_t is_C_set = 0;
  uint8_t is_initialized = 0;
  uint8_t valid_token = 0;

  uint32_t parsed_value;
  uint16_t file_checksum;
  size_t current_offset, stx_offset, stx_count, etx_count;
  char *p_token, *p_next;

  current_offset = 0;
  stx_count = 0;
  etx_count = 0;

  // short file analysis, search for STX offset.
  while (current_offset < buffer_size) {
    if (buffer[current_offset] == STX) {
      stx_count++;
      stx_offset = current_offset;  // save the STX offset found
    }
    if (buffer[current_offset] == ETX) etx_count++;
    if (stx_count > 1 || etx_count > 1)
      return BAD_FORMAT;  // Failed, must be a binary file
    current_offset++;
  }

  current_offset = stx_offset;  // save the STX offset for latter usage
  if (!stx_count || !etx_count) return BAD_FORMAT;  // No STX or ETX found

  // Search for ETX offset, compute the file checksum.
  file_checksum = 0;
  while (current_offset < buffer_size) {
    file_checksum += buffer[current_offset];
    if (buffer[current_offset] == ETX) break;
    current_offset++;
  }

  // Parse the file checksum
  if (parse_uint32(&buffer[current_offset + 1], &parsed_value, &p_next, 16))
    return BAD_FORMAT;

  // The checksum must have 4 hex digits
  if (p_next - &buffer[current_offset + 1] != 4) return BAD_FORMAT;

  // Store both parsed and calculated file checksums.
  jedec->decl_file_checksum = parsed_value;
  jedec->calc_file_checksum = file_checksum;

  // Parse each token in the buffer starting with STX offset.
  p_token = buffer + stx_offset;
  while (p_token) {
    // Skip empty tokens
    if (!*p_token) continue;

    // Skip non printable characters but ETX
    while (!isalpha(*p_token) && *p_token != ETX) p_token++;

    // Exit the loop if the ETX character is found
    if (*p_token == ETX) break;

    // Parse QP token
    switch (parse_token(p_token, "QP", 2, &parsed_value, NULL, 10)) {
      case NO_ERROR:
        if (!is_QP_set) {
          jedec->QP = parsed_value;
          is_QP_set = 1;
          valid_token = 1;
        } else
          return BAD_FORMAT;
        break;
      case BAD_FORMAT:
        if (!valid_token) break;
        return BAD_FORMAT;
    }

    // Parse QF token
    switch (parse_token(p_token, "QF", 2, &parsed_value, NULL, 10)) {
      case NO_ERROR:
        if (!is_QF_set) {
          jedec->QF = parsed_value;
          is_QF_set = 1;
          valid_token = 1;
        } else
          return BAD_FORMAT;
        break;
      case BAD_FORMAT:
        if (!valid_token) break;
        return BAD_FORMAT;
    }

    // Parse G token
    switch (parse_token(p_token, "G", 1, &parsed_value, NULL, 10)) {
      case NO_ERROR:
        if (!is_G_set) {
          jedec->G = parsed_value;
          is_G_set = 1;
          valid_token = 1;
        } else
          return BAD_FORMAT;
        break;
      case BAD_FORMAT:
        if (!valid_token) break;
        return BAD_FORMAT;
    }

    // Parse F token
    switch (parse_token(p_token, "F", 1, &parsed_value, NULL, 10)) {
      case NO_ERROR:
        if (!is_F_set) {
          jedec->F = parsed_value;
          is_F_set = 1;
          valid_token = 1;
        } else
          return BAD_FORMAT;
        break;
      case BAD_FORMAT:
        if (!valid_token) break;
        return BAD_FORMAT;
    }

    // Parse C token
    switch (parse_token(p_token, "C", 1, &parsed_value, NULL, 16)) {
      case NO_ERROR:
        if (!is_C_set) {
          jedec->C = parsed_value;
          is_C_set = 1;
          valid_token = 1;
        } else
          return BAD_FORMAT;
        break;
      case BAD_FORMAT:
        if (!valid_token) break;
        return BAD_FORMAT;
    }

    // Parse L token
    switch (parse_token(p_token, "L", 1, &parsed_value, &p_next, 10)) {
      case NO_ERROR:
        // No 'L' allowed after C token or 'L' without a valid header
        if (is_C_set || !valid_token || !is_QF_set) return BAD_FORMAT;

        if (!is_initialized) {
          /*
           On first 'L' token found allocate buffer and clear it
           with the default 'F' value.
           If no 'F' default fuse value is set then the default will be 0.
           */
          jedec->fuses = malloc(jedec->QF);
          if (!jedec->fuses) {
            return MEMORY_ERROR;
          }
          memset(jedec->fuses, is_F_set ? (jedec->F == 0 ? 0x00 : 0x01) : 0,
                 (jedec->QF));
          is_initialized = 1;
          *checksum = 0;
        }

        /*
         Some jed files have fuses divided on several lines.
         So we need to skip line breaks characters and extract only valid
         digits. For example: L0000<CR><LF>
         1111111111111111111111111111111111111111<CR><LF>
         1111111111111111111111110111111111011101<CR><LF>
         0000000000000000000000000000000000000000*<CR><LF>
         We need to parse each line to get the entire 120 bits row.
         */
        while (*p_next != DELIMITER) {
          if (!iscntrl(*p_next) && *p_next != ' ' && *p_next != '0' &&
              *p_next != '1')
            return BAD_FORMAT;

          if (*p_next == '0' || *p_next == '1') {
            jedec->fuses[parsed_value] = 0;
            if (*p_next == '1') {
              jedec->fuses[parsed_value] = 1;
              (*checksum) += (0x01 << (parsed_value & 0x07));
            }
            parsed_value++;
          }
          p_next++;
        }
        break;
      case BAD_FORMAT:
        return BAD_FORMAT;
    }
    p_token = strchr(p_token, DELIMITER);
    if (p_token) p_token++;  // Skip the delimiter character
  }
  return NO_ERROR;
}

// JEDEC file parser
int read_jedec_file(const char *filename, jedec_t *jedec) {
  uint16_t checksum;
  // Open the jed file
  FILE *file = fopen(filename, "r");
  if (file == NULL) {
    return FILE_OPEN_ERROR;
  }

  // Get the jed file size
  struct stat st;
  if (stat(filename, &st)) return FILE_OPEN_ERROR;
  off_t size = st.st_size;

  // Check for size limits
  if (size < JED_MIN_SIZE || size > JED_MAX_SIZE) {
    fclose(file);
    return SIZE_ERROR;
  }

  // Read the jed file
  char *buffer = malloc(size + 64);
  if (!buffer) {
    fclose(file);
    return MEMORY_ERROR;
  }
  memset(buffer, 0, size + 64);

  if (fread(buffer, sizeof(char), size, file) != size) {
    fclose(file);
    free(buffer);
    return FILE_READ_ERROR;
  }
  fclose(file);

  int error = parse_tokens(buffer, size, jedec, &checksum);
  if (error != NO_ERROR) {
    free(buffer);
    return error;
  }
  free(buffer);
  jedec->fuse_checksum = checksum;

  return NO_ERROR;
}

// JEDEC file writer
int write_jedec_file(const char *filename, jedec_t *jedec) {
  uint16_t fuse_checksum = 0, file_checksum = 0;
  int16_t c;

  FILE *file = fopen(filename, "w+");
  if (file == NULL) {
    return FILE_OPEN_ERROR;
  }
  if (jedec->device_name == NULL) jedec->device_name = "Unknown";

  // Print jedec header
  fprintf(file,
          "%c\r\nDevice: %s\r\n\r\nNOTE: Written by Minipro open source"
          " software.*\r\n\r\nQP%u*\r\nQF%u*\r\nF%u*\r\nG%u*\r\n\r\n",
          STX, jedec->device_name, jedec->QP, jedec->QF, jedec->F, jedec->G);

  uint32_t i;
  // Print fuses
  for (i = 0; i < jedec->QF; i++) {
    if ((i % ROW_SIZE) == 0)
      fprintf(file, "%sL%04u ", i ? (i % ROW_SIZE ? "" : "*\r\n") : "",
              (uint32_t)i);
    fprintf(file, "%c", jedec->fuses[i] == 1 ? '1' : '0');
    fuse_checksum += jedec->fuses[i] == 1 ? (0x01 << (i & 0x07)) : 0;
  }

  // Print fuses checksum and ETX character
  fprintf(file, "*\r\nC%04X*\r\n%c", fuse_checksum, ETX);

  // Calculate the file checksum
  rewind(file);
  while ((c = fgetc(file)) != EOF) file_checksum += c;
  fprintf(file, "%04X\r\n", file_checksum);

  fclose(file);
  return NO_ERROR;
}
