#ifndef KPKG1_H
#define KPKG1_H

#include "kpm/kpkg.h"
#include <stdio.h>

KPKG_Result KPKG1_LoadFile(KPKG_File *kpkg_file, const char *location, FILE *in_file);

#endif