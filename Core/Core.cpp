#include "Core.h"


TString<128> GRootDir;


/*-----------------------------------------------------------------------------
	Log output
-----------------------------------------------------------------------------*/

#define MAX_LOGGERS		32
#define BUFFER_LEN		16384		// allocated in stack, so - not very small, but not too large ...


/*-----------------------------------------------------------------------------
	NULL device
-----------------------------------------------------------------------------*/

class COutputDeviceNull : public COutputDevice
{
public:
	virtual void Write(const char *str)
	{ /* empty */ }
};

static COutputDeviceNull GNullDevice;


/*-----------------------------------------------------------------------------
	Log device: all Write() -> appPrintf()
-----------------------------------------------------------------------------*/

class COutputDeviceLog : public COutputDevice
{
private:
	int recurse;

public:
	virtual void Write(const char *str)
	{
		// do not allow GLog to be used in appPrintf() output device chains
		// in this case, GLog.Write -> appPrintf -> GLog.Write
		if (recurse)
			appError("GLog: recurse");
		// print data
		recurse++;
		appPrintf("%s", str);
		recurse--;
	}
};

static COutputDeviceLog GLogDevice;
COutputDevice *GLog = &GLogDevice;


static int numDevices = 0;
static COutputDevice *loggers[MAX_LOGGERS];

void COutputDevice::Printf(const char *fmt, ...)
{
	va_list	argptr;
	va_start(argptr, fmt);
	char buf[BUFFER_LEN];
	int len = vsnprintf(ARRAY_ARG(buf), fmt, argptr);
	va_end(argptr);
	if (len < 0 || len >= sizeof(buf) - 1) return;		//?? may be, write anyway

	Write(buf);
	if (FlushEveryTime) Flush();
}


void COutputDevice::Register()
{
	if (numDevices)
	{
		for (int i = 0; i < numDevices; i++)
			if (loggers[i] == this) return;		// already registered
		if (numDevices >= MAX_LOGGERS)
			appError("COutputDevice::Register: too much loggers");
		// new device will be first
		memmove(loggers+1, loggers, sizeof(COutputDevice*) * numDevices);
	}
	loggers[0] = this;
	numDevices++;
}


void COutputDevice::Unregister()
{
	for (int i = 0; i < numDevices; i++)
		if (loggers[i] == this)
		{
			// found
			numDevices--;
			if (i < numDevices)			// not last
				memcpy(loggers+i, loggers+i+1, sizeof(COutputDevice*) * (numDevices - i)); // compact list
			return;
		}
}


void appPrintf(const char *fmt, ...)
{
	guard(appPrintf);
	if (!numDevices) return;			// no loggers registered

	va_list	argptr;
	va_start(argptr, fmt);
	char buf[BUFFER_LEN];
	int len = vsnprintf(ARRAY_ARG(buf), fmt, argptr);
	va_end(argptr);
	if (len < 0 || len >= sizeof(buf) - 1) return;		//?? may be, write anyway

	for (int i = 0; i < numDevices; i++)
	{
		COutputDevice *out = loggers[i];
		out->Write(buf);
		if (out->FlushEveryTime) out->Flush();
	}
	unguard;
}

/*-----------------------------------------------------------------------------
	Simple error/notofication functions
-----------------------------------------------------------------------------*/

void appError(char *fmt, ...)
{
	va_list	argptr;
	va_start(argptr, fmt);
	char buf[4096];
	int len = vsnprintf(ARRAY_ARG(buf), fmt, argptr);
	va_end(argptr);
	if (len < 0 || len >= sizeof(buf) - 1) exit(1);

//	appNotify("ERROR: %s\n", buf);
	strcpy(GErrorHistory, buf);
	appStrcatn(ARRAY_ARG(GErrorHistory), "\n");
	THROW;
}


static char NotifyBuf[512];

void appSetNotifyHeader(const char *fmt, ...)
{
	va_list	argptr;
	va_start(argptr, fmt);
	int len = vsnprintf(ARRAY_ARG(NotifyBuf), fmt, argptr);
	va_end(argptr);
	// print to console
	if (NotifyBuf[0])
		appPrintf("******** %s ********\n", NotifyBuf);
}


void appNotify(char *fmt, ...)
{
	va_list	argptr;
	va_start(argptr, fmt);
	char buf[BUFFER_LEN];
	int len = vsnprintf(ARRAY_ARG(buf), fmt, argptr);
	va_end(argptr);
	if (len < 0 || len >= sizeof(buf) - 1) exit(1);

	// print to log file
	if (FILE *f = fopen("notify.log", "a"))
	{
		// print header
		if (NotifyBuf[0])
			fprintf(f, "\n******** %s ********\n\n", NotifyBuf);
		// print message
		fprintf(f, "%s\n", buf);
		fclose(f);
	}
	appPrintf("*** %s\n", buf);
	// clean notify header
	NotifyBuf[0] = 0;
}


char GErrorHistory[2048];
static bool WasError = false;

static void LogHistory(const char *part)
{
	appStrcatn(ARRAY_ARG(GErrorHistory), part);
}

void appUnwindPrefix(const char *fmt)
{
	char buf[512];
	appSprintf(ARRAY_ARG(buf), WasError ? " <- %s:" : "%s:", fmt);
	LogHistory(buf);
	WasError = false;
}


void appUnwindThrow(const char *fmt, ...)
{
	char buf[512];
	va_list argptr;

	va_start(argptr, fmt);
	if (WasError)
	{
		strcpy(buf, " <- ");
		vsnprintf(buf+4, sizeof(buf)-4, fmt, argptr);
	}
	else
	{
		vsnprintf(buf, sizeof(buf), fmt, argptr);
		WasError = true;
	}
	va_end(argptr);
	LogHistory(buf);

	THROW;
}


/*-----------------------------------------------------------------------------
	CArchive helpers
-----------------------------------------------------------------------------*/

CArchive& operator<<(CArchive &Ar, CCompactIndex &I)
{
	if (Ar.IsLoading)
	{
		byte b;
		Ar << b;
		int sign  = b & 0x80;	// sign bit
		int shift = 6;
		int r     = b & 0x3F;
		if (b & 0x40)			// has 2nd byte
		{
			do
			{
				Ar << b;
				r |= (b & 0x7F) << shift;
				shift += 7;
			} while (b & 0x80);	// has more bytes
		}
		I.Value = sign ? -r : r;
	}
	else
	{
		int v = I.Value;
		byte b = 0;
		if (v < 0)
		{
			v = -v;
			b |= 0x80;			// sign
		}
		b |= v & 0x3F;
		if (v <= 0x3F)
		{
			Ar << b;
		}
		else
		{
			b |= 0x40;			// has 2nd byte
			v >>= 6;
			Ar << b;
			assert(v);
			while (v)
			{
				b = v & 0x7F;
				v >>= 7;
				if (v)
					b |= 0x80;	// has more bytes
				Ar << b;
			}
		}
	}
	return Ar;
}

void SerializeChars(CArchive &Ar, char *buf, int length)
{
	for (int i = 0; i < length; i++)
		Ar << *buf++;
}

void* LoadFile(const char* filename)
{
	FILE* file = fopen(filename, "rb");
	if (!file) return NULL;

	// get file size
	fseek(file, 0, SEEK_END);
	int size = ftell(file);
	fseek(file, 0, SEEK_SET);

	byte* buf = new byte [size+1];		// 1 extra byte for null-terminated text
	buf[size] = 0;
	if (fread(buf, size, 1, file) != 1)
	{
		delete buf;
		fclose(file);
		return NULL;
	}

	fclose(file);
	return buf;
}

/*-----------------------------------------------------------------------------
	CArray implementation
-----------------------------------------------------------------------------*/

void CArray::Empty(int count, int elementSize)
{
	if (DataPtr)
		delete[] DataPtr;
	DataPtr   = NULL;
	DataCount = 0;
	MaxCount  = count;
	if (count)
	{
		DataPtr = new byte[count * elementSize];
		memset(DataPtr, 0, count * elementSize);
	}
}


void CArray::Insert(int index, int count, int elementSize)
{
	guard(CArray::Insert);
	assert(index >= 0);
	assert(index <= DataCount);
	assert(count > 0);
	// check for available space
	if (DataCount + count > MaxCount)
	{
		// not enough space, resize ...
		int prevCount = MaxCount;
		MaxCount = ((DataCount + count + 7) / 8) * 8 + 8;
		DataPtr = realloc(DataPtr, MaxCount * elementSize);
		// zero added memory
		memset(
			(byte*)DataPtr + prevCount * elementSize,
			0,
			(MaxCount - prevCount) * elementSize
		);
	}
	// move data
	memmove(
		(byte*)DataPtr + (index + count)     * elementSize,
		(byte*)DataPtr + index               * elementSize,
						 (DataCount - index) * elementSize
	);
	// last operation: advance counter
	DataCount += count;
	unguard;
}

void CArray::Remove(int index, int count, int elementSize)
{
	guard(CArray::Remove);
	assert(index >= 0);
	assert(count > 0);
	assert(index + count <= DataCount);
	// move data
	memcpy(
		(byte*)DataPtr + index                       * elementSize,
		(byte*)DataPtr + (index + count)             * elementSize,
						 (DataCount - index - count) * elementSize
	);
	// decrease counter
	DataCount -= count;
	unguard;
}
