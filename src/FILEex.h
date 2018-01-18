#pragma once
#include <string>
#include "dataType.hh"

/**
Adapter to enhance file I/O operations - e.g. access files in archives.
*/
class FILEex
{
public:
	FILEex() {}
	virtual ~FILEex() {}
	virtual bool open(const char * filename, const char * mode) = 0;
	virtual int seek(long offset, int whence) = 0;
	virtual size_t read(void * ptr, size_t size, size_t count) = 0;
	virtual bool read_line(std::string& line) = 0;
};

FILEex * fopenEx(const char * filename, const char * mode);
inline int fseek(FILEex *stream, long offset, int whence) { return stream->seek(offset, whence); }
inline int fclose(FILEex *& stream) { delete stream; stream = NULL; return 0; }
inline size_t fread(void * ptr, size_t size, size_t count, FILEex * stream) { return stream->read(ptr, size, count); }

inline bool getLineFromFile(FILEex* f, std::string& line) { return f->read_line(line); }
bool getFirstAndSecondElementInLine(FILEex* f, std::string& _line, ITYPE& _freq);
