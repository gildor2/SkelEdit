#include "GlViewport.h"


#define MAX_IMG_SIZE	2048


/*-----------------------------------------------------------------------------
	Scaling image diminsions and colors
-----------------------------------------------------------------------------*/

static void ResampleTexture (unsigned* in, int inwidth, int inheight, unsigned* out, int outwidth, int outheight)
{
	int		i;
	unsigned p1[MAX_IMG_SIZE], p2[MAX_IMG_SIZE];

	unsigned fracstep = (inwidth << 16) / outwidth;
	unsigned frac = fracstep >> 2;
	for (i = 0; i < outwidth; i++)
	{
		p1[i] = 4 * (frac >> 16);
		frac += fracstep;
	}
	frac = 3 * (fracstep >> 2);
	for (i = 0; i < outwidth; i++)
	{
		p2[i] = 4 * (frac >> 16);
		frac += fracstep;
	}

	float f, f1, f2;
	f = (float)inheight / outheight;
	for (i = 0, f1 = 0.25f * f, f2 = 0.75f * f; i < outheight; i++, out += outwidth, f1 += f, f2 += f)
	{
		unsigned *inrow  = in + inwidth * appFloor (f1);
		unsigned *inrow2 = in + inwidth * appFloor (f2);
		for (int j = 0; j < outwidth; j++)
		{
			int		n, r, g, b, a;
			byte	*pix;

			n = r = g = b = a = 0;
#define PROCESS_PIXEL(row,col)	\
	pix = (byte *)row + col[j];		\
	if (pix[3])	\
	{			\
		n++;	\
		r += *pix++; g += *pix++; b += *pix++; a += *pix;	\
	}
			PROCESS_PIXEL(inrow,  p1);
			PROCESS_PIXEL(inrow,  p2);
			PROCESS_PIXEL(inrow2, p1);
			PROCESS_PIXEL(inrow2, p2);
#undef PROCESS_PIXEL

			switch (n)		// NOTE: generic version ("x /= n") is 50% slower
			{
			// case 1 - divide by 1 - do nothing
			case 2: r >>= 1; g >>= 1; b >>= 1; a >>= 1; break;
			case 3: r /= 3;  g /= 3;  b /= 3;  a /= 3;  break;
			case 4: r >>= 2; g >>= 2; b >>= 2; a >>= 2; break;
			case 0: r = g = b = 0; break;
			}

			((byte *)(out+j))[0] = r;
			((byte *)(out+j))[1] = g;
			((byte *)(out+j))[2] = b;
			((byte *)(out+j))[3] = a;
		}
	}
}


static void MipMap(byte* in, int width, int height)
{
	width *= 4;		// sizeof(rgba)
	height >>= 1;
	byte *out = in;
	for (int i = 0; i < height; i++, in += width)
	{
		for (int j = 0; j < width; j += 8, out += 4, in += 8)
		{
			int		r, g, b, a, am, n;

			r = g = b = a = am = n = 0;
#define PROCESS_PIXEL(idx)	\
	{	\
		n++;	\
		r += in[idx]; g += in[idx+1]; b += in[idx+2]; a += in[idx+3];	\
		am = max(am, in[idx+3]);	\
	}
			PROCESS_PIXEL(0);
			PROCESS_PIXEL(4);
			PROCESS_PIXEL(width);
			PROCESS_PIXEL(width+4);
#undef PROCESS_PIXEL
			//!! NOTE: currently, always n==4 here
			switch (n)
			{
			// case 1 - divide by 1 - do nothing
			case 2:
				r >>= 1; g >>= 1; b >>= 1; a >>= 1;
				break;
			case 3:
				r /= 3; g /= 3; b /= 3; a /= 3;
				break;
			case 4:
				r >>= 2; g >>= 2; b >>= 2; a >>= 2;
				break;
			case 0:
				r = g = b = 0;
				break;
			}
			out[0] = r; out[1] = g; out[2] = b;
			// generate alpha-channel for mipmaps (don't let it be transparent)
			// dest alpha = (MAX(a[0]..a[3]) + AVG(a[0]..a[3])) / 2
			// if alpha = 255 or 0 (for all 4 points) -- it will holds its value
			out[3] = (am + a) / 2;
		}
	}
}


/*-----------------------------------------------------------------------------
	Uploading textures
-----------------------------------------------------------------------------*/

static void GetImageDimensions(int width, int height, int* scaledWidth, int* scaledHeight)
{
	int sw, sh;
	for (sw = 1; sw < width;  sw <<= 1) ;
	for (sh = 1; sh < height; sh <<= 1) ;

	// scale down only when new image dimension is larger than 64 and
	// larger than 4/3 of original image dimension
	if (sw > 64 && sw > (width * 4 / 3))  sw >>= 1;
	if (sh > 64 && sh > (height * 4 / 3)) sh >>= 1;

	while (sw > MAX_IMG_SIZE) sw >>= 1;
	while (sh > MAX_IMG_SIZE) sh >>= 1;

	if (sw < 1) sw = 1;
	if (sh < 1) sh = 1;

	*scaledWidth  = sw;
	*scaledHeight = sh;
}


static void Upload(int handle, const void *pic, int width, int height, bool doMipmap, bool clampS, bool clampT)
{
	/*----- Calculate internal dimensions of the new texture --------*/
	int scaledWidth, scaledHeight;
	GetImageDimensions(width, height, &scaledWidth, &scaledHeight);

	/*---------------- Resample/lightscale texture ------------------*/
	unsigned *scaledPic = new unsigned [scaledWidth * scaledHeight];
	if (width != scaledWidth || height != scaledHeight)
		ResampleTexture((unsigned*)pic, width, height, scaledPic, scaledWidth, scaledHeight);
	else
		memcpy(scaledPic, pic, scaledWidth * scaledHeight * sizeof(unsigned));

	/*------------- Determine texture format to upload --------------*/
	int format = 4;		// RGBA

	/*------------------ Upload the image ---------------------------*/
	glBindTexture(GL_TEXTURE_2D, handle);
	glTexImage2D(GL_TEXTURE_2D, 0, format, scaledWidth, scaledHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaledPic);
	if (!doMipmap)
	{
		// setup min/max filter
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	else
	{
		int miplevel = 0;
		while (scaledWidth > 1 || scaledHeight > 1)
		{
			MipMap((byte *) scaledPic, scaledWidth, scaledHeight);
			miplevel++;
			scaledWidth  >>= 1;
			scaledHeight >>= 1;
			if (scaledWidth < 1)  scaledWidth  = 1;
			if (scaledHeight < 1) scaledHeight = 1;
			glTexImage2D(GL_TEXTURE_2D, miplevel, format, scaledWidth, scaledHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaledPic);
		}
		// setup min/max filter
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);	// trilinear filter
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}

	// setup wrap flags
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, clampS ? GL_CLAMP : GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, clampT ? GL_CLAMP : GL_REPEAT);

	delete scaledPic;
}


/*-----------------------------------------------------------------------------
	TGA loading
-----------------------------------------------------------------------------*/

#define TGA_ORIGIN_MASK		0x30
#define TGA_BOTLEFT			0x00
#define TGA_BOTRIGHT		0x10					// unused
#define TGA_TOPLEFT			0x20
#define TGA_TOPRIGHT		0x30					// unused

#if _MSC_VER
#pragma pack(push,1)
#endif

struct tgaHdr_t
{
	byte 	id_length, colormap_type, image_type;
	unsigned short colormap_index, colormap_length;
	byte	colormap_size;
	unsigned short x_origin, y_origin;				// unused
	unsigned short width, height;
	byte	pixel_size, attributes;
};

#if _MSC_VER
#pragma pack(pop)
#endif


static byte* LoadTGA(const char* name, int& width, int& height)
{
	guard(LoadTGA);

	byte*	file;
	if (!(file = (byte*) LoadFile(name))) return NULL;

	tgaHdr_t* hdr = (tgaHdr_t*)file;

	const char* errMsg = NULL;

	width  = hdr->width;
	height = hdr->height;

	bool isRle      = (hdr->image_type &  8) != 0;
	bool isPaletted = (hdr->image_type & ~8) == 1;
	bool isRGB      = (hdr->image_type & ~8) == 2;
	bool isBW       = (hdr->image_type & ~8) == 3;
	byte colorBits  = (isPaletted) ? hdr->colormap_size : hdr->pixel_size;
	byte colorBytes = colorBits >> 3;

	if (!(isPaletted || isRGB || isBW))
		errMsg = "unsupported type";
	else if (colorBits != 24 && colorBits != 32 && !isBW)
		errMsg = "unsupported color depth";
	else if ((isBW || isPaletted) && hdr->pixel_size != 8)
		errMsg = "wrong color depth for format";
	else if (width > MAX_IMG_SIZE || height > MAX_IMG_SIZE)
		errMsg = "image is too large";
	else if (isPaletted)
	{
		if (hdr->colormap_type != 1)
			errMsg = "unsupported colormap type";
		else if (hdr->colormap_index != 0)
			errMsg = "colormap_index != 0";
	}

	if (errMsg)
	{
		delete file;
		appNotify("LoadTGA(%s): %s", name, errMsg);
		return NULL;
	}

	int numPixels = width * height;

	byte *src = (byte*)(hdr + 1);
	src += hdr->id_length;							// skip image comment
	byte *pal = NULL;
	if (isPaletted)
	{
		pal = src;
		src += hdr->colormap_length * colorBytes;	// skip palette
	}

	byte* dst = new byte [numPixels * 4];
	byte* ret = dst;

	int stride;
	if ((hdr->attributes & TGA_ORIGIN_MASK) == TGA_TOPLEFT)
	{
		stride = 0;
	}
	else
	{
		stride = -width * 4 * 2;
		dst = dst + (height - 1) * width * 4;
	}

	int column = 0, num = 0;
	int copy = 0;
	while (num < numPixels)
	{
		byte	r, g, b, a;
		int		ct;

		if (copy) copy--;
		if (isRle && !copy)
		{
			byte f = *src++;
			if (f & 0x80)
				ct = f + 1 - 0x80;	// packed
			else
			{
				copy = f + 1;		// not packed
				ct = 1;
			}
		}
		else
			ct = 1;

		b = *src++;
		if (isRGB)
		{
			g = *src++;
			r = *src++;
			if (colorBytes == 4) a = *src++; else a = 255;
		}
		else if (isPaletted)
		{
			byte *s = pal + b * colorBytes;
			b = s[0];
			g = s[1];
			r = s[2];
			if (colorBytes == 4) a = s[3]; else a = 255;
		}
		else // if (isBW)  (monochrome)
		{
			r = g = b;
			a = 255;
		}

		for (int i = 0; i < ct; i++)
		{
			*dst++ = r; *dst++ = g; *dst++ = b; *dst++ = a;
			if (++column == width)
			{
				column = 0;
				dst += stride;
			}
		}
		num += ct;
	}

	delete file;
	return ret;

	unguard;
}


/*-----------------------------------------------------------------------------
	BMP loading
-----------------------------------------------------------------------------*/

#if _MSC_VER
#pragma pack(push,1)
#endif

struct bmpHdr_t
{
	word		Type;			// 'BM'
	unsigned	FileSize;
	word		Reserved1, Reserved2;
	unsigned	PicOffset;
};

struct bmpInfoHdr
{
	unsigned	StrucSize;
	int			Width, Height;
	word		Planes;
	word		BitCount;
	unsigned	Compression;
	unsigned	SizeImage;
	int			XPelsPerMeter, YPelsPerMeter;
	unsigned	ClrUsed;
	unsigned	ClrImportant;
};

struct bmpInfoHdr4 : bmpInfoHdr
{
	unsigned	RedMask, GreenMask, BlueMask, AlphaMask;
	unsigned	CSType;
	/* CIEXYZTRIPLE  Endpoints*/ long Endpoints[9];
	unsigned	GammaRed, GammaGreen, GammaBlue;
};


struct bmpInfoHdr5 : bmpInfoHdr4
{
	unsigned	Intent;
	unsigned	ProfileData;
	unsigned	ProfileSize;
	unsigned	Reserved;
};

#if _MSC_VER
#pragma pack(pop)
#endif



static byte* LoadBMP(const char* name, int& width, int& height)
{
	guard(LoadBMP);

	byte*	file;
	if (!(file = (byte*) LoadFile(name))) return NULL;

	// validate header
	bmpHdr_t   *hdr  = (bmpHdr_t*)file;
	bmpInfoHdr *hdr2 = (bmpInfoHdr*)(hdr+1);

	const char *errMsg = NULL;

	if ( hdr->Type != 'B' + ('M' << 8) ||
		(hdr2->StrucSize != sizeof(bmpInfoHdr ) &&
		 hdr2->StrucSize != sizeof(bmpInfoHdr4) &&
		 hdr2->StrucSize != sizeof(bmpInfoHdr5))
		)
		errMsg = "wrong BMP header";
	else if (hdr2->Compression != 0)
		errMsg = "compression is not supported";
	else if (hdr2->BitCount != 24 && hdr2->BitCount != 32)
		errMsg = "24 or 32-bit BMP supported only";

	if (errMsg)
	{
		delete file;
		appNotify("LoadBMP(%s): %s", name, errMsg);
		return NULL;
	}

	width  = hdr2->Width;
	height = hdr2->Height;
	bool topBottom = true;
	if (height < 0)
	{
		// Height may be <0 -- should reverse bitmap
		height    = -height;
		topBottom = false;
	}

	int numPixels = width * height;

	byte *src = file + hdr->PicOffset;
	byte* dst = new byte [numPixels * 4];
	byte* ret = dst;
	int stride;
	if (!topBottom)
	{
		stride = 0;
	}
	else
	{
		stride = -width * 4 * 2;
		dst = dst + (height - 1) * width * 4;
	}


	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			*dst++ = src[2];
			*dst++ = src[1];
			*dst++ = src[0];
			src += 3;
			if (hdr2->BitCount == 32)
				*dst++ = *src++;
			else
				*dst++ = 255;
		}
		dst += stride;
	}

	delete file;
	return ret;

	unguard;
}


/*-----------------------------------------------------------------------------
	Main texture loading function
-----------------------------------------------------------------------------*/

static int GTexHandle = 128;

int GL_LoadTexture(const char *name)
{
	guard(GL_LoadTexture);

	TString<256> Name;
	if (GRootDir[0])
		Name.sprintf("%s/%s", *GRootDir, name);
	else
		Name = name;

//	appPrintf("Loading texture %s\n", name);
	// get file extension
	const char *ext = strrchr(name, '.');
	if (!ext)
	{
		appNotify("Unable to detect texture format for %s", name);
		return 0;
	}
	ext++;

	// load texture bitmap
	byte *pic;
	int width, height;
	if (!stricmp(ext, "tga"))
	{
		pic = LoadTGA(Name, width, height);
	}
	else if (!stricmp(ext, "bmp"))
	{
		pic = LoadBMP(Name, width, height);
	}
	else
	{
		appNotify("Unsupported texture format %s", *Name);
		return 0;
	}

	if (!pic)
	{
		appNotify("File %s was not found", *Name);
		return 0;
	}

	// upload texture
	int handle = GTexHandle++;
	Upload(handle, pic, width, height, true, false, false);
	delete pic;
	return handle;

	unguardf(("%s", name));
}
