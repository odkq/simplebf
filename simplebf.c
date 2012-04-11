/* 
 * Simple BF interpreter
 * 
 * Copyright (C) 2010 Pablo Martin <pablo at odkq.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#define MSIZE 30000 /* Memory buffers
		       will be incremented as needed
		       by this amount */

/* A loop as is encountered in the source,
 * Stored both as a linked list and as
 * a "linked stack", so one can go forward
 * memory-wise and backwards stack-wise
 */
struct loop {
	unsigned int start;
	unsigned int end;
	struct loop *next_in_list;
	struct loop *previous_in_stack;
};

/* 
 * Struct with pointers to the beginning
 * of the loop list
 */
struct loops {
	struct loop *first;
	struct loop *current_in_list;
	struct loop *current_in_stack;
};

/*
 * Memory block, representing an alloc'ed
 * MSIZE array
 */
struct memory_block {
	unsigned char m[MSIZE];
	struct memory_block *next;
	struct memory_block *previous;
};


static struct loops *register_loops(const unsigned char *code);
static void free_loops(struct loops *loops);
static unsigned char *fill_program_array(const char *filename);
static int push_loop(struct loops *loops, unsigned int start_position);
static int pop_loop(struct loops *loops, unsigned int end_position);
static struct loop *find_loop_by_start(struct loops *loops, int startposition);
static struct loop *find_loop_by_end(struct loops *loops, int end_position);
static int walk_fd(unsigned char *array, int fd);

/* Fill the program input in a single buffer, skipping comments
 * In bf comments are not specially delimited, they are any
 * character not a bf command. Keep track of the position
 * (line/column) of each command in the original file
 */
static unsigned char *fill_program_array(const char *filename)
{
	int fd;
	unsigned char *program;
	struct stat buf;

	fd = open(filename, O_RDONLY);
	if (fd == -1) {
		fprintf(stderr, "Error opening %s: %s\n", 
			filename, strerror(errno));
		return NULL;
	}
	if (fstat(fd, &buf)) {
		fprintf(stderr, "stat: %s", strerror(errno));
		close(fd);
		return NULL;
	}
	if ((program = (unsigned char *)malloc(sizeof(unsigned char) * 
					       buf.st_size)) == NULL) {
		fprintf(stderr, "malloc: %s\n", strerror(errno));
		return NULL;
	}
	if (walk_fd(program, fd)) {
		fprintf(stderr, "Error parsing file\n");
		free(program);
		close(fd);
		return NULL;
	}
	close(fd);
	return program;
}

static int walk_fd(unsigned char *array, int fd)
{
	int c, i, j;
	ssize_t sread;
	unsigned char *p;
	char id[8] = "><+-.,[]";
	char buf[1024];

	p = array;
	do {
		sread = read(fd, buf, 1024);
		if (sread == (ssize_t)-1) {
			fprintf(stderr, "read(): %s", strerror(errno));
			return -1;
		}
		for (j = 0; j < (int)sread; j++) {
			c = buf[j];
			for (i = 0; i < 8; i++) {
				if (id[i] == c) {
					*p++ = c;
					break;
				}
			}
		}
	} while (sread == ((ssize_t)1024));
	return 0;
}

/*
 * Pass through the code buffer once, registering all the
 * start and end of loops, storing them in a "s linked list &
 * stack" structure
 */
static struct loops *register_loops(const unsigned char *code)
{
	struct loops *r;
	unsigned int i;

	if ((r = (struct loops *)malloc(sizeof(struct loops))) == NULL)
		return NULL;
	r->first = r->current_in_list = r->current_in_stack = NULL;

	for (i = 0; code[i] != '\0'; i++) {
		if (code[i] == '[') {
			if (push_loop(r, i)) {
				free_loops(r);
				return NULL;
			}
		} else if (code[i] == ']') {
			if (pop_loop(r, i)) {
				free_loops(r);
				return NULL;
			}
		}
	}
	return r;
}

/* Push a loop into the loops stack */
static int push_loop(struct loops *loops, unsigned int start_position)
{
	struct loop *l;
	if ((l = (struct loop *)malloc(sizeof(struct loop))) == NULL)
		return 1;
	l->start = start_position;
	l->end = 0;
	l->previous_in_stack = loops->current_in_stack;
	l->next_in_list = NULL;
	if (loops->current_in_list != NULL)
		loops->current_in_list->next_in_list = l;
	else 
		loops->first = l;
	loops->current_in_list = l;
	loops->current_in_stack = l;
	return 0;
}

/* Set the end and "pops" the element */
static int pop_loop(struct loops *loops, unsigned int end_position)
{
	loops->current_in_stack->end = end_position;
	loops->current_in_stack = loops->current_in_stack->previous_in_stack;
	return 0;
}

static struct loop *find_loop_by_start(struct loops *loops, int start_position)
{
	struct loop *l;
	l = loops->first;
	while(l != NULL) {
		if (start_position == l->start)
			return l;
		l = l->next_in_list;
	}
	return NULL;
}
static struct loop *find_loop_by_end(struct loops *loops, int end_position)
{
	struct loop *l;
	l = loops->first;
	while(l != NULL) {
		if (end_position == l->end)
			return l;
		l = l->next_in_list;
	}
	return NULL;
}

/* Free the memory used by the loops structure along with all nodes */
static void free_loops(struct loops *loops)
{
	struct loop *l, *n;

	l = loops->first;
	while (l != NULL) {
		n = l->next_in_list;
		free(l);
		l = n;
	}
	free(loops);
}
/*
 * Insert a new element in your ordinary double-linked list
 */
static struct memory_block *new_memory_block(struct memory_block *previous,
					     struct memory_block *next)
{
	struct memory_block *r;

	r = (struct memory_block *)malloc((sizeof(struct memory_block)));
	if (r == NULL)
		return NULL;
	memset(r->m, 0, MSIZE);
	r->next = next;
	r->previous = previous;
	if (previous)
		previous->next = r;
	if (next)
		next->previous = r;
	return r;
}
static void free_memory_blocks(struct memory_block *first_block)
{
	struct memory_block *current, *previous;

	current = previous = first_block;
	while (current) {
		previous = current;
		current = current->next;
		free(previous);
	}
}
/* Main loop */
static int simplebf(const unsigned char *code, struct loops *loops)
{
	struct memory_block *first_block, *current_block;
	unsigned char *mem;
	struct loop *l;
	int pc, mc;

	if ((first_block = new_memory_block(NULL, NULL)) == NULL)
		return 1;
	current_block = first_block;

	mem = current_block->m;
	for (pc = 0, mc = 0; code[pc] != '\0'; pc++) {
		switch(code[pc]) {
		case '>':
			mc++;
			if (mc == MSIZE) {
				if (current_block->next) {
					current_block = current_block->next;
				} else {
					current_block = new_memory_block(current_block, NULL);
					if (current_block == NULL) {
						free_memory_blocks(first_block);
						return 1;
					}
				}
				mc = 0;
				mem = current_block->m;
			}
			break;
		case '<':
			mc--;
			if (mc == -1) {
				current_block = current_block->previous;
				if (current_block == NULL) {
					fprintf(stderr, "Program attempted to shift left of 0\n");
					free_memory_blocks(first_block);
					return 1;
				}
				mc = MSIZE - 1;
				mem = current_block->m;
			}
			break;
		case '+':
			mem[mc] = mem[mc] + 1;
			break;
		case '-':
			mem[mc] = mem[mc] - 1;
			break;
		case '.':
			putchar(mem[mc]);
			break;
		case ',':
			mem[mc] = getchar();
			break;
		case '[':
			if (mem[mc] == 0) {
				if ((l = find_loop_by_start(loops, pc)) 
				    == NULL)
					return 2;
				pc = l->end;
			}
			break;
		case ']':
			if (mem[mc] != 0) {
				if ((l = find_loop_by_end(loops, pc)) 
				    == NULL)
					return 3;
				pc = l->start;
			}
			break;
		default:
			free(mem);
			return 4;
		}
	}
	fprintf(stderr, "\n");
	free(mem);
	return 0;
}

void usage(void)
{
	printf("\n");
	printf("Usage: simplebf programfile\n");
	printf("\n");
}

int main(int argc, char **argv)
{
	struct loops *loops;
	unsigned char *code;
	int e;

	if (argc != 2) {
		usage();
		return 1;
	}

	if ((code = fill_program_array(argv[1])) == NULL) {
		printf("Error reading %s\n", argv[1]);
		return 1;
	}

	if ((loops = register_loops(code)) == NULL) {
		fprintf(stderr, "Error in loops\n");
		return 1;
	}
	e = simplebf(code, loops);
	if (e) {
		fprintf(stderr, "Error %d\n", e);
		return 1;
	}

	free(code);
	free_loops(loops);
	return 0;
}
