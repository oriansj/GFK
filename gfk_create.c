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

struct sector
{
	int type;
	int block_address;
	int checksum;
	struct sector* next;
};

struct files
{
	char* name;
	int size;
	int sectors_needed;
	FILE* f;
	struct files* next;
	struct sector* content;
};

struct folders
{
	char* name;
	int sectors_needed;
	struct files* f;
	struct folders* sub;
	struct folders* next;
};

int BigByteEndian;
int BigBitEndian;
int native_block_size;
int volume_block_size;
int inode_size;
int dnode_size;
int checksum_mode;
int checksum_size;
int block_pointer_size;
int file_size_size;
char* MBR;
char* output;
struct folders* filesystem;

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

int sectors_needed_for_file(int size)
{
	int count = (size / volume_block_size);
	if(0 != (size % volume_block_size))
	{
		/* Round up */
		count = count + 1;
	}

	int max_inodes = volume_block_size / inode_size;
	if(0 == (volume_block_size % inode_size))
	{
		/* if packing too tight */
		max_inodes = max_inodes - 1;
	}

	/* Deal with the simplest case */
	if(count < max_inodes) return count + 1;

	/* One leve of indirection (plenty for the default sizes)*/
	int max_single_indirect_inodes = max_inodes * max_inodes;
	require(0 < max_single_indirect_inodes, "This tool currently doesn't support files exceeding 1GB in size\n");

	int direct_blocks = count / max_inodes;
	if(0 != (count % max_inodes))
	{
		/* make sure we have enough direct blocks */
		direct_blocks = direct_blocks + 1;
	}

	if(count < max_single_indirect_inodes) return count + direct_blocks + 1;


	/* Second level of hello */
	int max_double_indirect_inodes = max_inodes * max_single_indirect_inodes;
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

int sectors_needed_for_files(struct files* a)
{
	if(NULL == a) return 0;
	a->sectors_needed = sectors_needed_for_file(a->size);
	return a->sectors_needed + sectors_needed_for_files(a->next);
}

int sectors_needed_for_folders(struct folders* a)
{
	if(NULL == a) return 0;
	int dnodes_needed = files_count(a->f) + folders_count(a->sub);

	/* How many do we need? */
	int max_dnodes = volume_block_size / dnode_size;
	if(0 == (volume_block_size % inode_size))
	{
		/* if packing too tight */
		max_dnodes = max_dnodes - 1;
	}

	/* Welcome to indirection hell */
	int max_inodes = volume_block_size / inode_size;
	if(0 == (volume_block_size % inode_size))
	{
		/* if packing too tight */
		max_inodes = max_inodes - 1;
	}

	int max_single_indirect_dnodes = max_dnodes * max_inodes;
	require(0 < max_single_indirect_dnodes, "The folder is way bigger than it needs to be\n");


	/* Deal with simple case of single direct block */
	if(dnodes_needed < max_dnodes)
	{
		a->sectors_needed = 1;
	}
	else if(dnodes_needed < max_single_indirect_dnodes)
	{
		a->sectors_needed = (dnodes_needed / max_dnodes) + 1;
		if(0 != (dnodes_needed % max_dnodes))
		{
			a->sectors_needed = a->sectors_needed + 1;
		}
	}
	else
	{
		fputs("TODO expand dnode indirection\n", stderr);
		exit(EXIT_FAILURE);
	}

	return a->sectors_needed + sectors_needed_for_folders(a->sub) + sectors_needed_for_files(a->f);
}


struct files* insert_file(struct files* a, struct files* b)
{
	if(NULL == a) return b;
	if(NULL == b) return a;

	if(0 <= strcmp(a->name, b->name))
	{
		b->next = a;
		return b;
	}
	a->next = insert_file(a->next, b);
	return a;
}

struct folders* insert_folder(struct folders* a, struct folders* b)
{
	if(NULL == a) return b;
	if(NULL == b) return a;

	if(0 <= strcmp(a->name, b->name))
	{
		b->next = a;
		return b;
	}
	a->next = insert_folder(a->next, b);
	return a;
}

struct folders* find_folder(struct folders* walk, char* name)
{
	if(NULL == walk) return NULL;
	require(NULL != name, "A non-name is not a valid name for a directory\n");
	if(match(walk->name, name))
	{
		return walk;
	}
	return find_folder(walk->next, name);
}

void put_in_folders(struct folders* walk, struct files* f, char* PATH)
{
	if(NULL == walk) return;
	if(NULL == f) return;
	if(NULL == PATH)
	{
		walk->f = insert_file(walk->f, f);
		return;
	}

	char* hold = NULL;
	int size = strlen(PATH);

	if(0 == size)
	{
		walk->f = insert_file(walk->f, f);
	}

	int i = 0;
	while(i <= size)
	{
		if('/' == PATH[i])
		{
			/* Use for recursion */
			PATH[i] = 0;
			hold = PATH+i+1;
			i = size;
		}
		i += 1;
	}

	struct folders* d = find_folder(walk->sub, PATH);
	if(match(".", PATH))
	{
		d = walk;
	}
	else if(NULL == d)
	{
		/* lets create the folder this belongs in */
		d = calloc(1, sizeof( struct folders));
		d->name = PATH;
		walk->sub = insert_folder(walk->sub, d);
	}
	put_in_folders(d, f, hold);
}

void process_file(char* s)
{
	require(NULL != s, "got a NULL filename somehow\n");
	int i = strlen(s);
	require(0 != i, "got an empty filename somehow\n");

	/* Now lets get it */
	struct files* f = calloc(1, sizeof(struct files));
	require(NULL != f, "calloc failed in process_file\n");

	/* Lets see if we can open it */
	f->f = fopen(s, "r");
	if(0 == f->f)
	{
		fputs("The file named: ", stdout);
		fputs(s, stdout);
		fputs(" either does not exit or you don't have read permissions to it\n", stdout);
		exit(EXIT_FAILURE);
	}

	/* How big is it? */
	fseek(f->f, 0, SEEK_END);
	f->size = ftell(f->f);
	rewind(f->f);

	/* Assume everything is at root */
	f->name = s;
	char* PATH = ".";
	/* Break filename from PATH */
	while(0 <= i)
	{
		if('/' == s[i])
		{
			s[i] = 0;
			f->name = s+i+1;
			PATH = s;
			i = 0;
		}
		i = i - 1;
	}

	put_in_folders(filesystem, f, PATH);
}

void write_MBR()
{
}

int main(int argc, char** argv)
{
	char* hold;
	BigByteEndian = TRUE;
	BigBitEndian = TRUE;
	MBR = NULL;
	filesystem = calloc(1, sizeof(struct folders));
	filesystem->name = "/";

	/* Assume GFK defaults */
	checksum_mode = 1;
	checksum_size = 32;
	native_block_size = 512;
	volume_block_size = 4096;
	block_pointer_size = 4;
	file_size_size = 4;

	int option_index = 1;
	while(option_index <= argc)
	{
		if(NULL == argv[option_index])
		{
			option_index = option_index + 1;
		}
		else if(match(argv[option_index], "--big-byte-endian"))
		{
			BigByteEndian = TRUE;
			option_index = option_index + 1;
		}
		else if(match(argv[option_index], "--big-bit-endian"))
		{
			BigByteEndian = TRUE;
			option_index = option_index + 1;
		}
		else if(match(argv[option_index], "--little-byte-endian"))
		{
			BigByteEndian = FALSE;
			option_index = option_index + 1;
		}
		else if(match(argv[option_index], "--little-bit-endian"))
		{
			BigByteEndian = FALSE;
			option_index = option_index + 1;
		}
		else if(match(argv[option_index], "--file") || match(argv[option_index], "-f"))
		{
			hold = argv[option_index+1];
			require(NULL != hold, "the option --file needs to get a file name to work\n");
			process_file(hold);
			option_index = option_index + 2;
		}
		else if(match(argv[option_index], "--output") || match(argv[option_index], "-o"))
		{
			hold = argv[option_index+1];
			require(NULL != hold, "the option --output needs to get a file name to work\n");
			output = hold;
			option_index = option_index + 2;
		}
		else if(match(argv[option_index], "--master-boot-record") || match(argv[option_index], "-mbr"))
		{
			hold = argv[option_index+1];
			require(NULL != hold, "the option --master-boot-record needs to get a file name to work\n");
			MBR = hold;
			option_index = option_index + 2;
		}
		else if(match(argv[option_index], "--native-block-size") || match(argv[option_index], "-nbs"))
		{
			hold = argv[option_index+1];
			require(NULL != hold, "the option --native-block-size needs to get an integer to work\n");
			native_block_size = strtoint(hold);
			require(64 < native_block_size, "we don't support native block sizes smaller than 64bytes\n");
			option_index = option_index + 2;
		}
		else if(match(argv[option_index], "--volume-block-size") || match(argv[option_index], "-vbs"))
		{
			hold = argv[option_index+1];
			require(NULL != hold, "the option --volume-block-size needs to get an integer to work\n");
			volume_block_size = strtoint(hold);
			require(64 < volume_block_size, "we don't support volume block sizes smaller than 64bytes\n");
			option_index = option_index + 2;
		}
		else if(match(argv[option_index], "--checksum-algorithm") || match(argv[option_index], "-ca"))
		{
			hold = argv[option_index+1];
			require(NULL != hold, "the option --checksum-algorithm needs to get an integer to work\n");
			checksum_mode = strtoint(hold);
			option_index = option_index + 2;
		}
		else if(match(argv[option_index], "--checksum-block-size") || match(argv[option_index], "-cs"))
		{
			hold = argv[option_index+1];
			require(NULL != hold, "the option --checksum-block-size needs to get an integer to work\n");
			checksum_size = strtoint(hold);
			require(0 != checksum_size, "a checksum size of zero isn't valid\nIf you wish to disable checksums change the algorithm to zero\n");
			option_index = option_index + 2;
		}
		else if(match(argv[option_index], "--file-size-block-size") || match(argv[option_index], "-fsbs"))
		{
			hold = argv[option_index+1];
			require(NULL != hold, "the option --file-size-block-size needs to get an integer to work\n");
			file_size_size = strtoint(hold);
			require(0 < file_size_size, "zero bytes can't encode any file size info\nNot valid\n");
			option_index = option_index + 2;
		}
		else if(match(argv[option_index], "--block-pointer-size") || match(argv[option_index], "-bps"))
		{
			hold = argv[option_index+1];
			require(NULL != hold, "the option --checksum-size needs to get an integer to work\n");
			checksum_size = strtoint(hold);
		}
		else
		{
			fputs("Unknown option\n", stderr);
			exit(EXIT_FAILURE);
		}
	}

	/* Sanity check checksum combos */
	if(0 == checksum_mode)
	{
		fputs("the NULL algorithm is not to be generated\n", stdout);
		fputs("Disabling checksuming in output filesystem\n", stdout);
		checksum_size = 0;
	}
	else if(1 == checksum_mode)
	{
		if(16 == checksum_size)
		{
			fputs("Using BSD checksum 16bit mode\n", stdout);
		}
		else if(32 == checksum_size)
		{
			fputs("Using BSD checksum 32bit mode\n", stdout);
		}
		else if(64 == checksum_size)
		{
			fputs("You have selected BSD checksum 64bit mode\n", stdout);
			fputs("Although this mode is entirely valid\n", stdout);
			fputs("This tool doesn't actually support it\n", stdout);
			fputs("Sorry\n", stdout);
			exit(EXIT_FAILURE);
		}
		else
		{
			fputs("You have selected an invalid checksum size for the BSD checksum\n", stdout);
			fputs("The only officially valid sizes are 16, 32 or 64bits\n", stdout);
			exit(EXIT_FAILURE);
		}
	}
	else if(2 == checksum_mode)
	{
		if(128 == checksum_size)
		{
			fputs("You have selected MD5 checksum 128bit mode\n", stdout);
			fputs("Although this mode is entirely valid\n", stdout);
			fputs("This tool doesn't actually support it\n", stdout);
			fputs("Sorry\n", stdout);
			exit(EXIT_FAILURE);
		}
		else
		{
			fputs("You have selected an invalid checksum size for the MD5 checksum\n", stdout);
			fputs("The only officially valid size is 128bits\n", stdout);
			exit(EXIT_FAILURE);
		}
	}
	else if(3 == checksum_mode)
	{
		if(160 == checksum_size)
		{
			fputs("You have selected SHA-1 checksum 160bit mode\n", stdout);
			fputs("Although this mode is entirely valid\n", stdout);
			fputs("This tool doesn't actually support it\n", stdout);
			fputs("Sorry\n", stdout);
			exit(EXIT_FAILURE);
		}
		else
		{
			fputs("You have selected an invalid checksum size for the SHA-1 checksum\n", stdout);
			fputs("The only officially valid size is 160bits\n", stdout);
			exit(EXIT_FAILURE);
		}
	}
	else if(4 == checksum_mode)
	{
		if(224 == checksum_size)
		{
			fputs("You have selected SHA-2 checksum 2248bit mode\n", stdout);
		}
		else if(256 == checksum_size)
		{
			fputs("You have selected SHA-2 checksum 256bit mode\n", stdout);
			exit(EXIT_FAILURE);
		}
		else if(384 == checksum_size)
		{
			fputs("You have selected SHA-2 checksum 384bit mode\n", stdout);
			exit(EXIT_FAILURE);
		}
		else if(512 == checksum_size)
		{
			fputs("You have selected SHA-2 checksum 512bit mode\n", stdout);
			exit(EXIT_FAILURE);
		}
		else
		{
			fputs("You have selected an invalid checksum size for the MD5 checksum\n", stdout);
			fputs("The only officially valid sizes are 224, 256, 384 and 512bits\n", stdout);
			exit(EXIT_FAILURE);
		}
		fputs("Although this mode is entirely valid\n", stdout);
		fputs("This tool doesn't actually support it\n", stdout);
		fputs("Sorry\n", stdout);
		exit(EXIT_FAILURE);
	}
	else
	{
		fputs("You have selected an entirely undefined checksum id\n", stdout);
		fputs("Congrats, that isn't valid and I'm not supporting that\n", stdout);
		fputs("Goodbye\n", stdout);
		exit(EXIT_FAILURE);
	}

	inode_size = block_pointer_size + (checksum_size / 8);
	dnode_size = (inode_size << 1) + file_size_size;
	require((dnode_size << 2) <= volume_block_size, "block size is too small\n");

	/* Sanity warning message */
	if(NULL == MBR)
	{
		fputs("You didn't set an MBR\n", stderr);
		fputs("I really hope you know what you are doing\n", stderr);
		fputs("Because this probably isn't going to work\n", stderr);
	}

	int volume_blocks_needed = sectors_needed_for_folders(filesystem);
	fputs("looks like we need: ", stdout);
	fputs(int2str(volume_blocks_needed, 10, FALSE), stdout);
	fputs(" blocks to write these files\n", stdout);

	return EXIT_SUCCESS;
}
