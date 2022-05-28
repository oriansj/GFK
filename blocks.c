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

int disk_block_count;
int native_block_size;
int volume_block_size;
int inode_size;
int dnode_size;
int checksum_mode;
int checksum_size;
int block_pointer_size;
int file_size_size;
FILE* output;

int _volume_block_id;
int get_free_block()
{
	int r = _volume_block_id;
	_volume_block_id = _volume_block_id + 1;
	return r;
}

int folders_count(struct folders* a)
{
	if(NULL == a) return 0;
	return 1 + folders_count(a->next);
}

int files_count(struct files* a)
{
	if(NULL == a) return 0;
	return 1 + files_count(a->next);
}

int max_inodes()
{
	int max = volume_block_size / inode_size;
	if(0 == (volume_block_size % inode_size))
	{
		/* if packing too tight */
		max = max - 1;
	}
	return max;
}

int max_dnodes()
{
	int max = volume_block_size / dnode_size;
	if(0 == (volume_block_size % inode_size))
	{
		/* if packing too tight */
		max = max - 1;
	}
	return max;
}

int blocks_needed_for_file_data(int size)
{
	int count = (size / volume_block_size);
	if(0 != (size % volume_block_size))
	{
		/* Round up */
		count = count + 1;
	}
	return count;
}

int blocks_needed_for_file(int size)
{
	int count = blocks_needed_for_file_data(size);

	/* Deal with the simplest case */
	if(count < max_inodes()) return count + 1;

	/* One leve of indirection (plenty for the default sizes)*/
	int max_single_indirect_inodes = max_inodes() * max_inodes();
	require(0 < max_single_indirect_inodes, "This tool currently doesn't support files exceeding 1GB in size\n");

	int direct_blocks = count / max_inodes();
	if(0 != (count % max_inodes()))
	{
		/* make sure we have enough direct blocks */
		direct_blocks = direct_blocks + 1;
	}

	if(count < max_single_indirect_inodes) return count + direct_blocks + 1;


	/* Second level of hello */
	int max_double_indirect_inodes = max_inodes() * max_single_indirect_inodes;
	require(0 < max_double_indirect_inodes, "This tool currently doesn't support files exceeding 1GB in size\n");

	int level_zero_indirect_blocks = count / max_single_indirect_inodes;
	if(0 != (count % max_single_indirect_inodes))
	{
		/* make sure we have enough level zero indirect blocks */
		level_zero_indirect_blocks = level_zero_indirect_blocks + 1;
	}

	if(count < max_double_indirect_inodes) return count + direct_blocks + level_zero_indirect_blocks + 1;

	fputs("TODO inode indirection expansion\n", stderr);
	exit(EXIT_FAILURE);
}

int blocks_needed_for_files(struct files* a)
{
	if(NULL == a) return 0;
	a->blocks_needed = blocks_needed_for_file(a->size);
	return a->blocks_needed + blocks_needed_for_files(a->next);
}

int blocks_needed_for_folders(struct folders* a)
{
	if(NULL == a) return 0;
	int dnodes_needed = files_count(a->f) + folders_count(a->sub);

	/* How many do we need? */
	int max_single_indirect_dnodes = max_dnodes() * max_inodes();
	require(0 < max_single_indirect_dnodes, "The folder is way bigger than it needs to be\n");


	/* Deal with simple case of single direct block */
	if(dnodes_needed < max_dnodes())
	{
		a->blocks_needed = 1;
	}
	else if(dnodes_needed < max_single_indirect_dnodes)
	{
		a->blocks_needed = (dnodes_needed / max_dnodes()) + 1;
		if(0 != (dnodes_needed % max_dnodes()))
		{
			a->blocks_needed = a->blocks_needed + 1;
		}
	}
	else
	{
		fputs("TODO expand dnode indirection\n", stderr);
		exit(EXIT_FAILURE);
	}

	return a->blocks_needed + blocks_needed_for_folders(a->sub) + blocks_needed_for_files(a->f);
}

void write_sector(char* s, int size)
{
	fwrite(s, sizeof(char), size, output);
	fflush(output);
}

void write_block(char* s, int size)
{
	int needed = (size / native_block_size);
	if(0 != (volume_block_size / native_block_size))
	{
		needed = needed + 1;
	}

	struct buffers* a;
	int i = 0;
	while( i <= needed)
	{
		a = create_buffer(allocated, native_block_size);
		memcpy(a->buffer, s + (i * native_block_size), native_block_size);
		write_sector(a->buffer, native_block_size);
		remove_buffer(a);
		i = i + 1;
	}
}

void insert_block_address(char* a, int address)
{
	if(block_pointer_size > 1)
}

void insert_inode(struct buffers* a, int index, int address, int checksum)
{
}

void read_write_direct_file_blocks(FILE* f, struct buffers* b)
{
}

void read_write_indirect_file_blocks(struct files* f, struct buffers* b)
{
	if(f->blocks_needed <= (max_inodes() * max_inodes()))
	{
		/* only a single indirection block needed */
	}
	else
	{
		fputs("read_write_indirect_file_blocks doesn't support this yet\n", stderr);
		exit(EXIT_FAILURE);
	}
}

int read_write_file_blocks(struct files* f)
{
	if(f->blocks_needed <= max_inodes())
	{
		/* First write the data to disk */
		read_write_direct_file_blocks(f->f);
	}
	else
	{
		read_write_indirect_file_blocks(f);
	}
	return get_free_block();
}

void write_MBR()
{
	struct buffers* a = create_buffer(allocated, native_block_size);
	if(NULL != MBR)
	{
		FILE* f = fopen(MBR, "r");
		require(NULL != f, "Unable to open MBR file for reading\n");
		int read = fread(a->buffer, sizeof(char) ,native_block_size, f);
		require(read > 0, "empty MBRs are not supported\n");
		write_sector(a->buffer, native_block_size);
		remove_buffer(a);

		a = create_buffer(allocated, native_block_size);
		read = fread(a->buffer, sizeof(char) ,native_block_size, f);
		require(0 == read, "MBR is not allowed to be bigger than a single sector\n");
	}
	else
	{
		/* Deal with NULL case */
		write_sector(a->buffer, native_block_size);
	}
	remove_buffer(a);
}

void write_slice(char* buffer, int value)
{
	/* Currently does the wrong thing for little bit endian but oh well */
	if(BigByteEndian)
	{
		buffer[4] = (value & 0xFF000000) >> 24;
		buffer[5] = (value & 0xFF0000) >> 16;
		buffer[6] = (value & 0xFF00) >> 8;
		buffer[7] = value & 0xFF;
	}
	else
	{
		buffer[0] = value & 0xFF;
		buffer[1] = (value & 0xFF00) >> 8;
		buffer[2] = (value & 0xFF0000) >> 16;
		buffer[3] = (value & 0xFF000000) >> 24;
	}
}

void write_leadblock()
{
	struct buffers* a = create_buffer(allocated, native_block_size);
	char encoding_flag = 0;
	if(!BigByteEndian) encoding_flag = encoding_flag | 0b10000000;
	if(!BigBitEndian)  encoding_flag = encoding_flag |  0b1000000;

	/* We only support version zero thus far so rest of encoding flag must be zero */
	a->buffer[0] = encoding_flag;

	/* store size of blocks */
	write_slice(a->buffer+64, volume_block_size);

	/* store size of block pointer */
	write_slice(a->buffer+128, block_pointer_size);

	/* store size of file size */
	write_slice(a->buffer+192, file_size_size);

	/* store superblock address */
	int volume_blocks_needed = blocks_needed_for_folders(filesystem);
	if(native_block_size > volume_block_size)
	{
		_volume_block_id = ((native_block_size << 1) / volume_block_size);
		/* Figure out which virtual block address physical block address is */
		volume_blocks_needed = volume_blocks_needed + _volume_block_id;
		if(0 != ((native_block_size << 1) % volume_block_size))
		{
			/* fudge the extra block */
			volume_blocks_needed = volume_blocks_needed + 1;
			_volume_block_id = _volume_block_id + 1;
		}
		/* the block after the leadblock is the one we start allocating in */
		_volume_block_id = _volume_block_id + 1;
	}
	else
	{
		/* Deal with the case the native block size is equal or smaller than virtual block size */
		if((native_block_size < 1) <= volume_block_size)
		{
			volume_blocks_needed = volume_blocks_needed + 1;
			/* Looks like the block after the leadblock is volume address 2 */
			_volume_block_id = 2;
		}
		else
		{
			volume_blocks_needed = volume_blocks_needed + 2;
			/* Looks like the block after the leadblock is volume address 3 */
			_volume_block_id = 3;
		}
	}
	write_slice(a->buffer+256, volume_blocks_needed);

	/* store native block size */
	write_slice(a->buffer+320, native_block_size);

	/* The result needs to be nulls*/

	/* Now write it out to disk */
	write_sector(a->buffer, native_block_size);

	/* clean up after ourselves */
	remove_buffer(a);
}
