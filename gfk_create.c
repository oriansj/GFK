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
			output = fopen(hold, "w");
			require(NULL != output, "unable to open output file for writing\n");
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

	require(NULL != output, "You must set an --output file\n");

	/* Setup our first buffer */
	allocated = create_buffer(NULL, native_block_size);
	remove_buffer(allocated);

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

	int volume_blocks_needed = blocks_needed_for_folders(filesystem);
	fputs("projected block need: ", stdout);
	fputs(int2str(volume_blocks_needed, 10, FALSE), stdout);
	fputs(" blocks to write these files\n", stdout);

	/* Write the MBR which is always the first sector */
	write_MBR();

	/* Write our leadblock which is always the second sector */
	write_leadblock();

	return EXIT_SUCCESS;
}
