#include "Core.h"


void appStrncpyz(char *dst, const char *src, int count)
{
	if (count <= 0) return;	// zero-length string

	char c;
	do
	{
		if (!--count)
		{
			// out of dst space -- add zero to the string end
			*dst = 0;
			return;
		}
		c = *src++;
		*dst++ = c;
	} while (c);
}


// NOTE: not same, as strncat(dst, src, count): strncat's "count" is a maximum length of "src", but
//	here "count" is maximal result size of "dst" ...
void appStrcatn(char *dst, int count, const char *src)
{
	char *p = strchr(dst, 0);
	int maxLen = count - (p - dst);
	if (maxLen > 1)
		appStrncpyz(p, src, maxLen);
}


int appSprintf(char *dest, int size, const char *fmt, ...)
{
	guardSlow(appSprintf);

	va_list	argptr;
	va_start(argptr, fmt);
	int len = vsnprintf(dest, size, fmt, argptr);
	va_end(argptr);
//	if (len < 0 || len >= size - 1)
//		appWPrintf("appSprintf: overflow of %d (called by \"%s\")\n", size, appSymbolName(GET_RETADDR(dest)));

	return len;

	unguardSlow;
}


char *appStrdup(const char *str)
{
//	MEM_ALLOCATOR(str);
	int size = strlen(str) + 1;
	char *out = (char*)appMalloc(size);
	memcpy(out, str, size);
	return out;
}

char *appStrdup(const char *str, CMemoryChain *chain)
{
	int size = strlen(str) + 1;
	char *out = (char*)chain->Alloc(size);
	memcpy(out, str, size);
	return out;
}


CArchive& SerializeString(CArchive &Ar, char *str, int len)
{
	guard(SerializeString);
	if (Ar.IsLoading)
	{
		// load string
		char *limit = str + len;
		while (str < limit)
		{
			char c;
			Ar << c;
			*str++ = c;
			if (!c) return Ar;
		}
		// here: no more space in string, but have data in a file
		appError("Reading TString<%d> of a greater len");
	}
	else
	{
		// store string
		while (true)
		{
			char c = *str++;
			Ar << c;
			if (!c) break;
		}
	}
	return Ar;
	unguard;
}
