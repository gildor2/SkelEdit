#ifndef __OUTPUTDEVICEMEM_H__
#define __OUTPUTDEVICEMEM_H__


class COutputDeviceMem : public COutputDevice
{
protected:
	char	*buffer;
	int		used;
	int		size;

public:
	COutputDeviceMem(int bufSize = 4096)
	:	size(bufSize)
	,	used(0)
	{
		buffer = new char [bufSize];
//		buffer[0] = 0;
	}
	~COutputDeviceMem()
	{
		Unregister();
		delete buffer;
	}
	inline const char *GetText() const
	{
		return buffer;
	}
	virtual void Write(const char *str)
	{
		int len = strlen(str);
		if (used + len + 1 >= size)
		{
			// shrink len
			len = size - used - 1;
		}
		if (len <= 0) return;
		// add text to buffer
		memcpy(buffer + used, str, len);
		used += len;
		// add trailing zero
		buffer[used] = 0;
	}
};


#endif // __OUTPUTDEVICEMEM_H__
