class CFileReader : public CArchive
{
public:
	CFileReader()
	:	f(NULL)
	{}

	CFileReader(FILE *InFile)
	:	f(InFile)
	{
		IsLoading = true;
	}

	CFileReader(const char *Filename)
	:	f(fopen(Filename, "rb"))
	{
		guard(CFileReader::CFileReader);
		if (!f)
			appError("Unable to open file \"%s\"", Filename);
		IsLoading = true;
		unguardf(("%s", Filename));
	}

	virtual ~CFileReader()
	{
		if (f) fclose(f);
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

/*	virtual CArchive& operator<<(FName &N)
	{
		*this << AR_INDEX(N.Index);
		return *this;
	}

	virtual CArchive& operator<<(UObject *&Obj)
	{
		int tmp;
		*this << AR_INDEX(tmp);
		printf("Object: %d\n", tmp);
		return *this;
	} */

protected:
	FILE	*f;
	virtual void Serialize(void *data, int size)
	{
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
	}
};
