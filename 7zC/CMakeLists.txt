cmake_minimum_required(VERSION 2.8)

project(7zlib)

set(7z_SRC 7zAlloc.c 7zArcIn.c 7zBuf.c 7zBuf2.c 7zCrc.c 7zCrcOpt.c 7zDec.c CpuArch.c Delta.c LzmaDec.c Lzma2Dec.c Bra.c Bra86.c BraIA64.c Bcj2.c Ppmd7.c Ppmd7Dec.c 7zFile.c 7zStream.c)
add_library(7z ${7z_SRC})

