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

int BigByteEndian;
int BigBitEndian;
char* MBR;
struct folders* filesystem;

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

void write_file(struct files* f)
{
}

void write_folder(struct folders* f)
{
}

void write_filesystem()
{
}
