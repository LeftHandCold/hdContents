//
// Created by sjw on 2018/1/18.
//

#ifndef HDCONTENTS_HDTD_BUFFER_H
#define HDCONTENTS_HDTD_BUFFER_H

#include "hdtd/system.h"
#include "hdtd/context.h"

/*
	hd_buffer is a wrapper around a dynamically allocated array of bytes.

	Buffers have a capacity (the number of bytes storage immediately
	available) and a current size.
*/
typedef struct hd_buffer_s hd_buffer;

/*
	hd_keep_buffer: Increment the reference count for a buffer.

	Returns a pointer to the buffer.
*/
hd_buffer *hd_keep_buffer(hd_context *ctx, hd_buffer *buf);

/*
	hd_drop_buffer: Decrement the reference count for a buffer.
*/
void hd_drop_buffer(hd_context *ctx, hd_buffer *buf);


/*
	hd_resize_buffer: Ensure that a buffer has a given capacity,
	truncating data if required.

	capacity: The desired capacity for the buffer. If the current size
	of the buffer contents is smaller than capacity, it is truncated.
*/
void hd_resize_buffer(hd_context *ctx, hd_buffer *buf, size_t capacity);


#endif //HDCONTENTS_HDTD_BUFFER_H
