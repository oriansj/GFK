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

#include "M2libc/bootstrappable.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FILE_TAG 0b100
#define FILE_INDIRECT_TAG 0b101
#define FOLDER_TAGE 0b10
#define FOLDER_INDIRECT_TAG 0b11

struct files
{
	char* name;
	int size;
	int blocks_needed;
	FILE* f;
	struct files* next;
};

struct folders
{
	char* name;
	int blocks_needed;
	struct files* f;
	struct folders* sub;
	struct folders* next;
};

struct buffers
{
	int size;
	int IN_USE;
	int CLEANED;
	int checksum;
	char* buffer;
	struct buffers* next;
};

extern int disk_block_count;
extern struct buffers* allocated;
extern int native_block_size;
extern int volume_block_size;
extern int inode_size;
extern int dnode_size;
extern int checksum_mode;
extern int checksum_size;
extern int block_pointer_size;
extern int file_size_size;
extern int BigByteEndian;
extern int BigBitEndian;
extern char* MBR;
extern struct folders* filesystem;
extern FILE* output;

struct buffers* create_buffer(struct buffers* a, int size);
void remove_buffer(struct buffers* a);
void process_file(char* s);
int blocks_needed_for_folders(struct folders* a);
void write_MBR();
void write_leadblock();
