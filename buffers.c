/* Copyright (C) 2022 Jeremiah Orians
 * This file is part of GFK
 *
 * GFK is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * GFK is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GFK If not, see <http://www.gnu.org/licenses/>.
 */

#include "gfk_create.h"

struct buffers* allocated;

struct buffers* allocate_buffer(int size)
{
	struct buffers* a = calloc(1, sizeof(struct buffers));
	require(NULL != a, "Buffer allocation failed\n");
	a->buffer = calloc(size+4, sizeof(char));
	require(NULL != a->buffer, "Buffer allocation failed\n");
	a->CLEANED = TRUE;
	a->IN_USE = FALSE;
	a->size = size;
	return a;
}

struct buffers* create_buffer(struct buffers* a, int size)
{
	if(NULL == a)
	{
		a = allocate_buffer(size);
	}

	if(a->CLEANED && !a->IN_USE && (a->size >= size))
	{
		a->IN_USE = TRUE;
		return a;
	}

	struct buffers* hold = create_buffer(a->next, size);

	if(NULL == a->next)
	{
		a->next = hold;
	}

	return hold;
}

void remove_buffer(struct buffers* a)
{
	/* Clean the buffer */
	memset(a->buffer, 0, a->size);
	a->CLEANED = TRUE;

	/* Reset for next use */
	a->checksum = 0;
	a->IN_USE = FALSE;
}
