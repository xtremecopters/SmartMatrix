/*
 * Ring Buffer implementation for C99 and C++98.
 *
 * Copyright (C) 2014-2015 The Steele Group, LLC  All Rights Reserved.
 */

#if __cplusplus
#	include <cstdint>
#	include <cstring>
#else
#	include <stdint.h>
#	include <string.h>
#endif

#include "ringbuffer.h"


/**
 * Initialize ring buffer.
 *
 * @param[in,out]	ctx				Ring buffer context.
 * @param[out]		buffer			Pointer to storage buffer.
 * @param[in]		size			Unit capacity of the storage buffer.
 */
void ringbuffer_Init(tRingBuffer *ctx, void *buffer, size_t size)
{
	ctx->data		= buffer;
	ctx->size		= size;
	ctx->used		= 0;
	ctx->read		= 0;
	ctx->write		= 0;
	ctx->overflow	= 0;
}

/**
 * Reset ring buffer to empty.
 *
 * @param[in,out]	ctx				Ring buffer context.
 */
void ringbuffer_Reset(tRingBuffer *ctx)
{
	ctx->used	= 0;
	ctx->read	= 0;
	ctx->write	= 0;
}

/**
 * Write data into ring buffer. Any writes performed after ring buffer reaches full capacity
 * the oldest units (at read position) will be replaced. Write overflows are allowed, but
 * naturally only the latest units written up to capacity will occupy the ring buffer.
 *
 * @param[in,out]	ctx				Ring buffer context.
 * @param[in]		src				Pointer to data source. May be NULL to just move write pointer.
 * @param[in]		size			Size in units to write into ring buffer.
 * @return							Number of units copied into ring buffer.
 */
size_t ringbuffer_Write(tRingBuffer *ctx, const void *src, size_t size)
{
	size_t	len;
	size_t	moved	= 0;


	while(size)
	{
		len  = size;
		size = 0;
		if((ctx->write + len) > ctx->size)
		{
			size  = ctx->write + len - ctx->size;
			len  -= size;
		}

		if(src)
			memcpy((uint8_t *)ctx->data + ctx->write, (uint8_t *)src + moved, len);

		moved	   += len;
		ctx->used  += len;
		ctx->write += len;

		if(ctx->write >= ctx->size)
			ctx->write -= ctx->size;

		// correct for overflowed write
		if(ctx->used > ctx->size)
		{
			ctx->overflow += ctx->size - ctx->used;

			ctx->read = ctx->write;
			ctx->used = ctx->size;
		}
	}

	// return number of units copied into ring buffer
	return moved;
}

/**
 * Read data out of ring buffer. For each unit of data read it is removed from the ring buffer.
 * Only up to ring buffer occupancy size will be read.
 *
 * @param[in,out]	ctx				Ring buffer context.
 * @param[out]		dest			Pointer to data destination. May be NULL to discard ring buffer data.
 * @param[in]		size			Size in units to read from ring buffer.
 * @return							Number of units moved into destination buffer.
 */
size_t ringbuffer_Read(tRingBuffer *ctx, void *dest, size_t size)
{
	size_t	len;
	size_t	moved	= 0;


	if(size > ctx->used)
		size = ctx->used;

	while(size)
	{
		len  = size;
		size = 0;
		if((ctx->read + len) > ctx->size)
		{
			size  = ctx->read + len - ctx->size;
			len  -= size;
		}

		if(dest)
			memcpy((uint8_t *)dest + moved, (uint8_t *)ctx->data + ctx->read, len);

		moved	  += len;
		ctx->used -= len;
		ctx->read += len;

		if(ctx->read >= ctx->size)
			ctx->read -= ctx->size;
	}

	// return number of units moved into destination buffer
	return moved;
}

/**
 * Retrieve pointers to the occupying data in the ring buffer.
 *
 * @param[in,out]	ctx				Ring buffer context.
 * @param[out]		p1				Reference pointer to earliest data units in ring buffer.
 * @param[out]		p1size			Number of units accessible referenced in p1.
 * @param[out]		p2				Reference pointer to latest data units in ring buffer. Not NULL when read position winds back to beginning of buffer.
 * @param[out]		p2size			Number of units accessible referenced in p2.
 * @return							Total number data units accessible with the reference pointers.
 */
size_t ringbuffer_Data(tRingBuffer *ctx, void **p1, size_t *p1size, void **p2, size_t *p2size)
{
	*p1		= (uint8_t *)ctx->data + ctx->read;
	*p1size	= ctx->used;
	*p2		= NULL;
	*p2size	= 0;

	if((ctx->read + ctx->used) > ctx->size)
	{
		*p2size  = (ctx->read + ctx->used) - ctx->size;
		*p1size -= *p2size;
		*p2		 = ctx->data;
	}

	// total accessible data units
	return *p1size + *p2size;
}

/**
* Read a byte out of the ring buffer without removing it.
* Only occupied area of ring buffer will be read. Index specified beyond this area will be wrap around.
*
* @param[in,out]	ctx				Ring buffer context.
* @param[in]		index			Byte index position to read. Value larger than occupied size will be moduloed.
* @return							Less than zero(0) when ring buffer is emptyed, else requested byte.
*/
int ringbuffer_PeekByte(tRingBuffer *ctx, size_t index)
{
	// abort on no data available
	if(!ctx->used)
		return -1;

	index  = index % ctx->used;
	index += ctx->read;
	if(index >= ctx->size)
		index -= ctx->size;

	return *((uint8_t *)((uint8_t *)ctx->data + index));
}

