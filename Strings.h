
inline char toLower(char c)
{
	return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c;
}


inline char toUpper(char c)
{
	return c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c;
}


void appStrncpyz(char *dst, const char *src, int count);
void appStrcatn(char *dst, int count, const char *src);

int appSprintf(char *dest, int size, const char *fmt, ...);

char *appStrdup(const char *str);
char *appStrdup(const char *str, CMemoryChain *chain);


/*-----------------------------------------------------------------------------
	TString template - template for static strings (use instead of "char str[N]")
-----------------------------------------------------------------------------*/

template<int N> class TString
{
private:
	char	str[N];
public:
	// no constructors ... anyway, 1/2 of all initializations will be done with filename()
#if 0
	inline TString()								{}
	inline TString(const char *s)					{ appStrncpyz(str, s, N); }
#endif
	inline int len() const							{ return strlen(str); }
	inline char& operator[](int idx)				{ return str[idx]; }
	inline const char& operator[](int idx) const	{ return str[idx]; }
	// copying
	inline TString<N>& operator+=(const char *s2)	{ appStrcatn(str, N, s2); return *this; }
	// assignment
	inline TString& operator=(const char *s2)
	{
		appStrncpyz(str, s2, N);
		return *this;
	}
	// formatting
	int sprintf(const char *fmt, ...)
	{
		va_list	argptr;
		va_start(argptr, fmt);
		int len = vsnprintf(str, N, fmt, argptr);
		str[N-1] = 0;											// force overflowed buffers to be null-terninated
		va_end(argptr);
		return len;
	}
	// searching
	inline char *chr(char c)						{ return strchr(str, c); }
	inline const char *chr(char c) const			{ return strchr(str, c); }
	inline char *rchr(char c)						{ return strrchr(str, c); }
	inline const char *rchr(char c) const			{ return strrchr(str, c); }
	// string comparison
	// NOTE: comparision as class members suitable for cases, when TString<> is on a left
	// side of operator; when TString on the right side, and "char*" on the left, by default,
	// will be executed "str <operator> char*(Str)", i.e. will be performed address comparision;
	// so, to correct this situation, we overloaded binary operator (char*,TString<>&) too
	inline int cmp(const char *s2) const			{ return strcmp(str, s2); }
	inline int icmp(const char *s2) const			{ return stricmp(str, s2); }
	// comparison operators
#define OP(op)										\
	inline bool operator op(const char *s2) const	\
	{ return strcmp(str, s2) op 0; };				\
	inline bool operator op(char *s2) const		\
	{ return strcmp(str, s2) op 0; };				\
	template<int M> inline bool operator op(const TString<M> &S) const \
	{ return strcmp(str, S.str) op 0; }			\
	template<int M> inline bool operator op(TString<M> &S) const \
	{ return strcmp(str, S.str) op 0; }
	OP(==) OP(!=) OP(<=) OP(<) OP(>) OP(>=)
#undef OP

	// implicit use as "const char*"
	// usage warning: use "*Str" when used in printf()-like functions (no type conversion by default ...)
	inline operator const char*() const				{ return str; }
	inline operator char*()							{ return str; }
	inline const char* operator*() const			{ return str; }
	inline char* operator*()						{ return str; }
#if 0
	// do not make "operator bool", because this will introduce conflicts with "operator char*" -- compiler
	// can not deside, which conversion to use in statements ...
	// check for empty string (true when non-empty)
	inline operator bool() const					{ return str[0] != 0; }
#endif
};


CArchive &SerializeString(CArchive &Ar, char *str, int len);

template<int N> inline CArchive& operator<<(CArchive &Ar, TString<N> &S)
{
	return SerializeString(Ar, *S, N);
}

template<int N> inline bool operator==(const char *s1, TString<N> &s2)
{
	return strcmp(s1, *s2) == 0;
}

template<int N> inline bool operator!=(const char *s1, TString<N> &s2)
{
	return strcmp(s1, *s2) != 0;
}
