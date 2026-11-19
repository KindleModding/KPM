#ifndef KPKG3_H
#define KPKG3_H

#include "kpm/kpkg.h"
#include <stdio.h>

bool KPKG3_ValidateFile(KPKG_File *file, size_t file_size, KPMIO *kpmio);
KPKG_Result KPKG3_LoadFile(KPKG_File *kpkg_file, const char *location, FILE *in_file, KPMIO *kpmio);
char *KPKG3_GetString(KPKG3_Payload *payload, uint64_t offset);
KPKG_Result KPKG3_Unpack(KPKG_File *file, const char *out_path, const char *cmp_path, KPMIO *kpmio);
#endif