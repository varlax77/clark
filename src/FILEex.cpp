#include "FILEex.h"

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <vector>

#include "file.hh"

#include "../7zC/CpuArch.h"
#include "../7zC/7z.h"
#include "../7zC/7zAlloc.h"
#include "../7zC/7zBuf.h"
#include "../7zC/7zCrc.h"
#include "../7zC/7zFile.h"
#include "../7zC/7zVersion.h"

using namespace std;

class PlainFile: public FILEex
{
public:
	PlainFile() : FILEex() {
		
	}
	bool open(const char * filename, const char * mode) { 
		file = fopen(filename, mode);
		return file != NULL; 
	}
	~PlainFile() { fclose(file); file = NULL; }
	int seek(long offset, int whence) { return fseek(file, offset, whence); }
	size_t read(void * ptr, size_t size, size_t count) { return fread(ptr, size, count, file); }
	bool read_line(string& line) { return getLineFromFile(file, line); }
private:
	FILE *file;
};

/**
Based on 7z ANSI-C Decoder from http://www.7-zip.org/sdk.html 
*/
class ArchivedLZMAFile : public FILEex
{
public:
	ArchivedLZMAFile(); 
	~ArchivedLZMAFile();
	bool open(const char * filename, const char * mode);
	int seek(long offset, int whence);
	size_t read(void * ptr, size_t size, size_t count);
	bool read_line(string& line);
private:
	bool init(const char* aname, const char *entry);

private:

	ISzAlloc allocImp;
	ISzAlloc allocTempImp;

	CFileInStream archiveStream;
	CLookToRead2 lookStream;
	CSzArEx db;
	SRes res;

	UInt32 entry_idx;

	/*
	if you need cache, use these 3 variables.
	*/
	UInt32 blockIndex = 0xFFFFFFFF; /* it can have any value before first call (if outBuffer = 0) */
	Byte *outBuffer = 0; /* it must be 0 before first call for each new archive. */
	size_t outBufferSize = 0;  /* it can have any value before first call (if outBuffer = 0) */

	size_t offset = 0;
	size_t outSizeProcessed = 0;

	Byte *outBufferProcessed = 0;
};


const char* LZMA_URL = "7z:";

FILEex * fopenEx(const char * filename, const char * mode) {
	FILEex *f = NULL;
	if (strncmp(filename, LZMA_URL, strlen(LZMA_URL)) == 0) {
		filename = filename + strlen(LZMA_URL);
		f = new ArchivedLZMAFile();
	}
	else {
		f = new PlainFile();
	}
	if (f && f->open(filename, mode)) {
		return f;
	}
	delete f;
	return NULL;
}

bool getFirstAndSecondElementInLine(FILEex* f, string& _line, ITYPE& _freq) {
	string tmp;
	if (f->read_line(tmp)) {
		size_t len = tmp.size();
		{
			// Take first element and put it into _line
			// Take second element and put it into _freq
			vector<string> ele;
			getElementsFromLine(tmp.c_str(), len, 2, ele);
			_line = ele[0];
			_freq = atoi(ele[1].c_str());
			return true;
		}
	}
	return false;
}

ArchivedLZMAFile::ArchivedLZMAFile() : FILEex(), entry_idx(UINT32_MAX), outBuffer(NULL), outBufferProcessed(NULL) {
}

ArchivedLZMAFile::~ArchivedLZMAFile() { 
	ISzAlloc_Free(&allocImp, outBuffer);
	SzArEx_Free(&db, &allocImp);
	ISzAlloc_Free(&allocImp, lookStream.buf);
	File_Close(&archiveStream.file);
}

bool ArchivedLZMAFile::open(const char * filename, const char * mode) {
	if (strcmp(mode, "r") == 0) {
		const char* sep = strchr(filename, '!');
		if (sep && sep[1] == '/') {
			string aname(filename, sep - filename);
			return init(aname.c_str(), sep + 2);
		}
		else {
			cerr << "Malformed archive entry url: " << filename << "\n";
			return false;
		}
	}
	else {
		cerr << "Unsupported file open mode for 7z archive: " << mode << "\n";
		return false;
	}
}


#define kInputBufSize ((size_t)1 << 18)

static const ISzAlloc g_Alloc = { SzAlloc, SzFree };

static int Buf_EnsureSize(CBuf *dest, size_t size)
{
	if (dest->size >= size)
		return 1;
	Buf_Free(dest, &g_Alloc);
	return Buf_Create(dest, size, &g_Alloc);
}

static void UInt64ToStr(UInt64 value, char *s, int numDigits)
{
	char temp[32];
	int pos = 0;
	do
	{
		temp[pos++] = (char)('0' + (unsigned)(value % 10));
		value /= 10;
	} while (value != 0);

	for (numDigits -= pos; numDigits > 0; numDigits--)
		*s++ = ' ';

	do
		*s++ = temp[--pos];
	while (pos);
	*s = '\0';
}
#ifndef _WIN32
#define _USE_UTF8
#endif

#ifdef _WIN32
static UINT g_FileCodePage = CP_ACP;
#endif


/* #define _USE_UTF8 */

#ifdef _USE_UTF8

#define _UTF8_START(n) (0x100 - (1 << (7 - (n))))

#define _UTF8_RANGE(n) (((UInt32)1) << ((n) * 5 + 6))

#define _UTF8_HEAD(n, val) ((Byte)(_UTF8_START(n) + (val >> (6 * (n)))))
#define _UTF8_CHAR(n, val) ((Byte)(0x80 + (((val) >> (6 * (n))) & 0x3F)))

static size_t Utf16_To_Utf8_Calc(const UInt16 *src, const UInt16 *srcLim)
{
	size_t size = 0;
	for (;;)
	{
		UInt32 val;
		if (src == srcLim)
			return size;

		size++;
		val = *src++;

		if (val < 0x80)
			continue;

		if (val < _UTF8_RANGE(1))
		{
			size++;
			continue;
		}

		if (val >= 0xD800 && val < 0xDC00 && src != srcLim)
		{
			UInt32 c2 = *src;
			if (c2 >= 0xDC00 && c2 < 0xE000)
			{
				src++;
				size += 3;
				continue;
			}
		}

		size += 2;
	}
}

static Byte *Utf16_To_Utf8(Byte *dest, const UInt16 *src, const UInt16 *srcLim)
{
	for (;;)
	{
		UInt32 val;
		if (src == srcLim)
			return dest;

		val = *src++;

		if (val < 0x80)
		{
			*dest++ = (char)val;
			continue;
		}

		if (val < _UTF8_RANGE(1))
		{
			dest[0] = _UTF8_HEAD(1, val);
			dest[1] = _UTF8_CHAR(0, val);
			dest += 2;
			continue;
		}

		if (val >= 0xD800 && val < 0xDC00 && src != srcLim)
		{
			UInt32 c2 = *src;
			if (c2 >= 0xDC00 && c2 < 0xE000)
			{
				src++;
				val = (((val - 0xD800) << 10) | (c2 - 0xDC00)) + 0x10000;
				dest[0] = _UTF8_HEAD(3, val);
				dest[1] = _UTF8_CHAR(2, val);
				dest[2] = _UTF8_CHAR(1, val);
				dest[3] = _UTF8_CHAR(0, val);
				dest += 4;
				continue;
			}
		}

		dest[0] = _UTF8_HEAD(2, val);
		dest[1] = _UTF8_CHAR(1, val);
		dest[2] = _UTF8_CHAR(0, val);
		dest += 3;
	}
}

static SRes Utf16_To_Utf8Buf(CBuf *dest, const UInt16 *src, size_t srcLen)
{
	size_t destLen = Utf16_To_Utf8_Calc(src, src + srcLen);
	destLen += 1;
	if (!Buf_EnsureSize(dest, destLen))
		return SZ_ERROR_MEM;
	*Utf16_To_Utf8(dest->data, src, src + srcLen) = 0;
	return SZ_OK;
}

#endif

static SRes Utf16_To_Char(CBuf *buf, const UInt16 *s
#ifndef _USE_UTF8
	, UINT codePage = g_FileCodePage
#endif
	)
{
	unsigned len = 0;
	for (len = 0; s[len] != 0; len++);

#ifndef _USE_UTF8
	{
		unsigned size = len * 3 + 100;
		if (!Buf_EnsureSize(buf, size))
			return SZ_ERROR_MEM;
		{
			buf->data[0] = 0;
			if (len != 0)
			{
				char defaultChar = '_';
				BOOL defUsed;
				unsigned numChars = 0;
				numChars = WideCharToMultiByte(codePage, 0, (LPCWCH)s, len, (char *)buf->data, size, &defaultChar, &defUsed);
				if (numChars == 0 || numChars >= size)
					return SZ_ERROR_FAIL;
				buf->data[numChars] = 0;
			}
			return SZ_OK;
		}
	}
#else
	return Utf16_To_Utf8Buf(buf, s, len);
#endif
}

int ArchivedLZMAFile::seek(long offset, int whence) {
/*	if (offset || whence) {
		fprintf(stderr, "Failed to seek 7z archive to arbitrary pos, only reset to 0 is supported!!!");
		exit(-1);
	}*/
	fprintf(stderr, "seeking 7z archive is not supported yet!!!");
	exit(-1);
	return -1;
}

size_t ArchivedLZMAFile::read(void * ptr, size_t size, size_t count) {

	size_t el = size;
	size = count * size;
	Byte *dest = (Byte*)ptr;
	while (res == SZ_OK) {
		if (outBufferProcessed && outSizeProcessed) {
			// use leftover from prev decompression first
			if (outSizeProcessed >= size) {
				memcpy(dest, outBufferProcessed, size);
				outSizeProcessed -= size;
				outBufferProcessed += size;
				return count;
			}
			else {
				memcpy(dest, outBufferProcessed, outSizeProcessed);
				dest += outSizeProcessed;
				size -= outSizeProcessed;
				outBufferProcessed = 0;
				return outSizeProcessed / size;
			}
		}
		if (outBufferProcessed) {
			return 0; // EOF ???
		}
		res = SzArEx_Extract(&db, &lookStream.vt, entry_idx,
			&blockIndex, &outBuffer, &outBufferSize,
			&offset, &outSizeProcessed,
			&allocImp, &allocTempImp);
		outBufferProcessed = outBuffer + offset;
		if (outSizeProcessed == 0) {
			//end of archive?
			break;
		}
	}
	return 0;
}


bool ArchivedLZMAFile::read_line(string& line) {

	char delims[] = "\n\r";
	char *sep = NULL;
	Byte terminator = '\0';

	while (res == SZ_OK) {
		if (outBufferProcessed && outSizeProcessed) {
			// use leftover from prev decompression first

			// ensure proper line end
			Byte* the_end = outBufferProcessed + outSizeProcessed;
			if (outBuffer + outBufferSize > the_end) {
				*the_end = '\0';
			}
			else if (*(--the_end) != '\0') {
				terminator = *the_end;
				*the_end = '\0';
			}
			// look for line sep
			sep = strtok((char*)outBufferProcessed, delims);
			if (sep != NULL)
			{
				line.append(sep);
				outBufferProcessed += line.length();
				outSizeProcessed -= line.length();
				if (terminator != '\0') {
					*the_end = terminator;
					if (outSizeProcessed == 1) {
						line.append(1, (char)terminator);
						outSizeProcessed = 0;
					}
				}
				return true;
			}
			else {
				if (terminator != '\0') {
					*the_end = terminator;
					if (outSizeProcessed == 1) {
						line.append(1, (char)terminator);
						outSizeProcessed = 0;
						return true;
					}
				}
				// no more lines
				outSizeProcessed = 0;
			}
		}
		if (outBufferProcessed) {
			return 0; // EOF ???
		}
		res = SzArEx_Extract(&db, &lookStream.vt, entry_idx,
			&blockIndex, &outBuffer, &outBufferSize,
			&offset, &outSizeProcessed,
			&allocImp, &allocTempImp);
		outBufferProcessed = outBuffer + offset;
		if (outSizeProcessed == 0) {
			//end of archive?
			break;
		}
	}

	return false;
}


static int strcmp16(const char *p, const UInt16 *name)
{
	CBuf buf;
	int res = 1;
	Buf_Init(&buf);
	if (Utf16_To_Char(&buf, name) == SZ_OK) {
		//fprintf(stderr, "Found entry: %s", (const char *)buf.data);
		res = strcmp(p, (const char *)buf.data);
		Buf_Free(&buf, &g_Alloc);
	}
	return res;
}

bool ArchivedLZMAFile::init(const char* fname, const char* entry) {
	allocImp = g_Alloc;
	allocTempImp = g_Alloc;

	if (InFile_Open(&archiveStream.file, fname))
	{
		fprintf(stderr, "can not open 7z file: %s\n", fname);
		return false;
	}

	FileInStream_CreateVTable(&archiveStream);
	LookToRead2_CreateVTable(&lookStream, False);
	lookStream.buf = NULL;

	res = SZ_OK;

	{
		lookStream.buf = (Byte*)ISzAlloc_Alloc(&allocImp, kInputBufSize);
		if (!lookStream.buf)
			res = SZ_ERROR_MEM;
		else
		{
			lookStream.bufSize = kInputBufSize;
			lookStream.realStream = &archiveStream.vt;
			LookToRead2_Init(&lookStream);
		}
	}

	CrcGenerateTable();

	SzArEx_Init(&db);

	if (res == SZ_OK)
	{
		UInt16 *temp = NULL;
		size_t tempSize = 0;

		res = SzArEx_Open(&db, &lookStream.vt, &allocImp, &allocTempImp);


		 for (UInt32 i = 0; i < db.NumFiles; i++)
		{
			// const CSzFileItem *f = db.Files + i;
			size_t len;
			unsigned isDir = SzArEx_IsDir(&db, i);
			if (isDir)
				continue;
			len = SzArEx_GetFileNameUtf16(&db, i, NULL);
			// len = SzArEx_GetFullNameLen(&db, i);

			if (len > tempSize)
			{
				SzFree(NULL, temp);
				tempSize = len;
				temp = (UInt16 *)SzAlloc(NULL, tempSize * sizeof(temp[0]));
				if (!temp)
				{
					res = SZ_ERROR_MEM;
					break;
				}
			}

			SzArEx_GetFileNameUtf16(&db, i, temp);
			if (0 == strcmp16(entry, temp)) {
				entry_idx = i;
				break;
			}

			/*
			if (SzArEx_GetFullNameUtf16_Back(&db, i, temp + len) != temp)
			{
			res = SZ_ERROR_FAIL;
			break;
			}
			*/
		}
		 SzFree(NULL, temp);
	}

	if (res == SZ_OK)
	{
		if (entry_idx == UINT32_MAX) {
			fprintf(stderr, "Bad target configuration, 7z archive entry not found: %s", entry);
			res = SZ_ERROR_DATA;
		}
		else {
			return true;
		}
	}

	if (res == SZ_ERROR_UNSUPPORTED)
		fprintf(stderr, "decoder doesn't support this archive\n");
	else if (res == SZ_ERROR_MEM)
		fprintf(stderr, "can not allocate memory\n");
	else if (res == SZ_ERROR_CRC)
		fprintf(stderr, "CRC error\n");
	else
	{
		char s[32];
		UInt64ToStr(res, s, 0);
		fprintf(stderr, "Error decoding 7z archive: %s\n", s);
	}
	return false;
}
