#ifndef __FILEREADERSTDIO_H__
#define __FILEREADERSTDIO_H__


class CFile : public CArchive
{
public:
	CFile()
	:	f(NULL)
	{}

	CFile(FILE *InFile)
	:	f(InFile)
	{
		IsLoading = true;
	}

	CFile(const char *Filename, bool loading = true)
	:	f(fopen(Filename, loading ? "rb" : "wb"))
	{
		guard(CFile::CFile);
		if (!f)
			appError("Unable to open file \"%s\"", Filename);
		IsLoading = loading;
		unguardf(("%s", Filename));
	}

	virtual ~CFile()
	{
		if (f) fclose(f);
	}

	void Close()
	{
		if (f) fclose(f);
		f = NULL;
	}

	void Setup(FILE *InFile, bool Loading)
	{
		f         = InFile;
		IsLoading = Loading;
	}

	virtual void Seek(int Pos)
	{
		fseek(f, Pos, SEEK_SET);
		ArPos = ftell(f);
		assert(Pos == ArPos);
	}

	virtual bool IsEof()
	{
		int pos  = ftell(f); fseek(f, 0, SEEK_END);
		int size = ftell(f); fseek(f, pos, SEEK_SET);
		return size == pos;
	}

	bool IsOpen()
	{
		return f != NULL;
	}

protected:
	FILE	*f;
	virtual void Serialize(void *data, int size)
	{
		guard(CFile::Serialize);
		int res;
		if (IsLoading)
			res = fread(data, size, 1, f);
		else
			res = fwrite(data, size, 1, f);
		ArPos += size;
		if (ArStopper > 0 && ArPos > ArStopper)
			appError("Serializing behind stopper");
		if (res != 1)
			appError("Unable to serialize data");
		unguard;
	}
};


#endif // __FILEREADERSTDIO_H__
