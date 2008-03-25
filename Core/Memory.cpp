#include "Core.h"


static void OutOfMemory()
{
	appError("Out of memory");
}


void *appMalloc(int size)
{
	assert(size >= 0);
	void *data = malloc(size);
	if (!data)
		OutOfMemory();
	if (size > 0)
		memset(data, 0, size);
	return data;
}


void *appRealloc(void *ptr, int size)
{
	assert(size >= 0);
	void *data = realloc(ptr, size);
	if (!data)
		OutOfMemory();
	return data;
}


void appFree(void *ptr)
{
	free(ptr);
}


/*-----------------------------------------------------------------------------
	Memory chain
-----------------------------------------------------------------------------*/

void* CMemoryChain::operator new(size_t size, int dataSize)
{
	guardSlow(CMemoryChain::new);
	int alloc = Align(size + dataSize, MEM_CHUNK_SIZE);
	CMemoryChain *chain = (CMemoryChain *) malloc(alloc);
	if (!chain)
		OutOfMemory();
	chain->size = alloc;
	chain->next = NULL;
	chain->data = (byte*) OffsetPointer(chain, size);
	chain->end  = (byte*) OffsetPointer(chain, alloc);

	memset(chain->data, 0, chain->end - chain->data);

	return chain;
	unguardSlow;
}


void CMemoryChain::operator delete(void *ptr)
{
	guardSlow(CMemoryChain::delete);
	CMemoryChain *curr, *next;
	for (curr = (CMemoryChain *)ptr; curr; curr = next)
	{
		// free memory block
		next = curr->next;
		free(curr);
	}
	unguardSlow;
}


void *CMemoryChain::Alloc(size_t size, int alignment)
{
	guardSlow(CMemoryChain::Alloc);
	if (!size) return NULL;

	// sequence of blocks (with using "next" field): 1(==this)->5(last)->4->3->2->NULL
	CMemoryChain *b = (next) ? next : this;			// block for allocation
	byte* start = Align(b->data, alignment);		// start of new allocation
	// check block free space
	if (start + size > b->end)
	{
		//?? may be, search in other blocks ...
		// allocate in the new block
		b = new (size + alignment - 1) CMemoryChain;
		// insert new block immediately after 1st block (==this)
		b->next = next;
		next = b;
		start = Align(b->data, alignment);
	}
	// update pointer to a free space
	b->data = start + size;

	return start;
	unguardSlow;
}


int CMemoryChain::GetSize() const
{
	int n = 0;
	for (const CMemoryChain *c = this; c; c = c->next)
		n += c->size;
	return n;
}
