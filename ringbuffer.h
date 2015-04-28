#ifndef RINGBUFFER_H_
#define RINGBUFFER_H_


// Ring Buffer context
typedef struct tRingBuffer
{
	void	*data;		//< Pointer to storage buffer.
	size_t	size;		//< Storage buffer capacity in units.
	size_t	used;		//< Units in buffer that are occupied.
	size_t	read;		//< Read position.
	size_t	write;		//< Write position.
	size_t	overflow;	//< Units overwritten by write operation.
} tRingBuffer;


extern void		ringbuffer_Init(tRingBuffer *ctx, void *buffer, size_t size);
extern void		ringbuffer_Reset(tRingBuffer *ctx);
extern size_t	ringbuffer_Write(tRingBuffer *ctx, const void *src, size_t size);
extern size_t	ringbuffer_Read(tRingBuffer *ctx, void *dest, size_t size);
extern size_t	ringbuffer_Data(tRingBuffer *ctx, void **p1, size_t *p1size, void **p2, size_t *p2size);
extern int		ringbuffer_PeekByte(tRingBuffer *ctx, size_t index);


#if __cplusplus

class RingBuffer
{
protected:
	tRingBuffer		m_Ring;


public:
	RingBuffer()															{ ringbuffer_Init(&m_Ring, NULL, 0); }
	RingBuffer(void *buffer, size_t size)									{ ringbuffer_Init(&m_Ring, buffer, size); }


	size_t	size()															{ return m_Ring.used; }
	size_t	capacity()														{ return m_Ring.size; }
	size_t	remain()														{ return m_Ring.size - m_Ring.used; }
	bool	empty()															{ return m_Ring.used == 0; }
	void	clear()															{ read(NULL, m_Ring.used); };

	void	init(void *buffer, size_t size)									{ ringbuffer_Init(&m_Ring, buffer, size); }
	void	reset()															{ ringbuffer_Reset(&m_Ring); }
	size_t	write(const void *src, size_t size)								{ return ringbuffer_Write(&m_Ring, src, size); }
	size_t	read(void *dest, size_t size)									{ return ringbuffer_Read(&m_Ring, dest, size); }
	size_t	data(void **p1, size_t *p1size, void **p2, size_t *p2size)		{ return ringbuffer_Data(&m_Ring, p1, p1size, p2, p2size); }
	int		peekByte(size_t index)											{ return ringbuffer_PeekByte(&m_Ring, index); }

	size_t	push(const char *str)											{ return write(str, strlen(str)); }
	size_t	pop(size_t size)												{ return read(NULL, size); }


	int operator[](size_t index)
	{
		return peekByte(index);
	}

	RingBuffer& operator+=(const char *lvStr)
	{
		push(lvStr);
		return *this;
	}
	RingBuffer& operator-=(size_t lvSize)
	{
		pop(lvSize);
		return *this;
	}
};


class RingBufferManaged : public RingBuffer
{
protected:
	uint8_t		*m_Buffer;


public:
	RingBufferManaged(size_t capacity)
	{
		m_Buffer = new uint8_t[capacity];
		init(m_Buffer, capacity);
	}
	virtual ~RingBufferManaged()
	{
		delete[] m_Buffer;
	}


	void	init(void *buffer, size_t size)									{ /* managed buffer, do not allow reinit */ }
};


#endif // __cplusplus

#endif // RINGBUFFER_H_
