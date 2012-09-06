/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Convert a keyboard program to bytecodes.
 *
 * A keyboard program is written in femto-lisp syntax.  For instance:
 *
 * (repeat 2 (key 1 4) (pause 3))
 *
 * Full manual:
 *
 * <program> = one or more <instruction>s
 * <instruction> = (key <row> <col>)
 *                      // toggle key at <row>, <col>
 *                 (repeat <times> <one or more instructions>)
 *                      // repeat <times> times (between 2 and 5 included)
 *                 (pause <time-index>)
 *                      // wait for the number of milliseconds specified
 *                      // by the <time-index>-th element of this table:
 *                      // { 0, 1, 2, 5, 10, 20, 50
 *                      //   100, 200, 500, 1000, 2000, 5000,
 *                      //   10000, 20000, 50000 }
 *
 *
 * Femto-lisp: smallest Lisp reader and data structures in the world.
 * - everything is an Object
 * - nil is also an Object.  Null pointer is not used.
 *
 * @x is translated into the (unique) address of symbol x (&x_symbol) by
 * preprocessing.  Like in Lisp, symbols are not declared.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

enum Type {
	NUMBER,
	CONS,
	SYMBOL,
};

struct object {
	enum Type type;				/* the type tag */
	union {
		struct {
			struct object *ar;	/* for the CAR */
			struct object *dr;	/* for the CDR */
		} c;
		char *name;			/* for SYMBOL type */
		int value;			/* for NUMBER type */
	} u;
	void *attrs;				/* other data on this object */
};

/* This is not allowed by the style guide:
 *   typedef struct object *Object;
 */
#define Object struct object *	/* barf */

#define assert(exp) do {				\
	if (!(exp)) {					\
		printf("assert failed: " #exp "\n");	\
		exit(1);				\
	} } while (0)

#define car(x) (x->u.c.ar)
#define cdr(x) (x->u.c.dr)

#define first(x) car(x)
#define second(x) car(cdr(x))
#define third(x) car(cdr(cdr(x)))

#define numberval(n) (n->u.value)
#define stringval(n) (n->u.name)

#define iswhite(c) (isblank(c) || c == '\n' || c == '\r')

#include "kbprog.h"
#include "symbols.h"

Object new_object(enum Type tag)
{
	Object x = malloc(sizeof(struct object));
	x->type = tag;
	x->attrs = @nil;
}

Object cons(Object kar, Object kdr)
{
	Object x = new_object(CONS);
	car(x) = kar;
	cdr(x) = kdr;
	return x;
}

inline int consp(Object x)
{
	return x->type == CONS;
}

inline int numberp(Object x)
{
	return x->type == NUMBER;
}

#define push(object, location) do {	\
	Object *_l = &(location);	\
	*_l = cons(object, *_l);	\
} while (0)

#define dolist(var, list) do {		\
	Object _c = list;		\
	while (_c != @nil) {		\
		Object var = car(_c);

#define odlist	_c = cdr(_c);		\
	}				\
} while (0)

int length(Object l)
{
	int i = 0;
	assert(consp(l));
	dolist(x, l) {
		i++;
	} odlist;
	return i;
}

Object nreverse_(Object old, Object new)
{
	Object t;
	if (old == @nil)
		return new;
	t = cdr(old);
	cdr(old) = new;
	return nreverse_(t, old);
}

/* Destructive list reversal */
Object nreverse(Object l)
{
	return nreverse_(l, @nil);
}

void print_list_no_open(Object x)
{
	Object y = cdr(x);
	print(car(x));
	if (consp(y)) {
		printf(" ");
		print_list_no_open(y);
	} else if (y == @nil) {
		printf(")");
	} else {
		printf(" . ");
		print(y);
	}
}

void print(Object x)
{
	switch (x->type) {
	case NUMBER:
		printf("%d", numberval(x));
		break;
	case SYMBOL:
		printf("%s", stringval(x));
		break;
	case CONS:
		printf("(");
		print_list_no_open(x);
		break;
	default:
		assert(0);
	}
}

Object new_symbol(const char *p, const char *end)
{
	Object x = new_object(SYMBOL);
	int l = end - p;
	char *s = malloc(l + 1);  /* null-terminated */
	strncpy(s, p, l);
	s[l] = '\0';
	x->u.name = s;
	return x;
}

Object find_symbol(Object list, const char *p, const char *end)
{

	dolist(s, list) {
		if (strlen(s->u.name) == end - p &&
		    strncmp(s->u.name, p, end - p) == 0) {
			return s;
		}
	} odlist;
	return @nil;
}

Object *symbol_table;       /* hash table of symbols, hashed on their name */
int symbol_table_length;    /* size of hash table */

int hash(const char *p, const char *end)
{
	int x = 0;
	while (p < end) {
		x += *p * 11777;
		p++;
	}
	return x;
}

/* Internalizes a builtin symbol */

void intern_builtin(Object s)
{
	const char *n = s->u.name;
	int h = hash(n, n + strlen(n)) % symbol_table_length;
	push(s, symbol_table[h]);
}

/* Finds a symbol, or makes a new one. */

Object intern(const char *p, const char *end)
{
	int h = hash(p, end) % symbol_table_length;
	Object *entry = symbol_table + h;
	Object symbol = find_symbol(*entry, p, end);
	if (symbol == @nil) {
		symbol = new_symbol(p, end);
		push(symbol, *entry);
	}
	return symbol;
}

void read_fatal(const char *message, const char *string)
{
	printf("%s: %s", message, string);
	exit(1);
}

Object read_number(const char **s)
{
	int x = 0;
	Object n = new_object(NUMBER);
	while (isdigit(**s)) {
		x = x * 10 + (**s - '0');
		(*s)++;
	}
	numberval(n) = x;
	return n;
}

Object read_symbol(const char **s)
{
	const char *p = *s;
	do {
		(*s)++;
	} while (**s == '_' || isalpha(**s) || **s == '-' || isdigit(**s));
	return intern(p, *s);
}

inline void skip_white(const char **s)
{
	while (iswhite(**s) && (**s != '#'))
		(*s)++;
	if (**s == '#') {
		/* skip comment, then start over */
		do {
			(*s)++;
		} while ((**s != '\n') && (**s != '\0'));
		skip_white(s);
	}
}

Object read_list(const char **s)
{
	(*s)++;  /* skip '(' */
	Object l = @nil;
	Object e;
	for (;;) {
		skip_white(s);
		if (**s == '\0')
			read_fatal("end of input reading list", *s);
		e = read_object(s);
		if (e == @closed_parens)
			return nreverse(l);
		push(e, l);
	}
}

Object read_object(const char **s)
{
	if (**s == '(')
		return read_list(s);
	else if (**s == ')') {
		(*s)++;
		return @closed_parens;
	} else if (isdigit(**s))
		return read_number(s);
	else
		return read_symbol(s);
}

void femtolisp_init(void)
{
	int i;
	symbol_table_length = 1000;
	symbol_table = malloc(sizeof(*symbol_table) * symbol_table_length);
	for (i = 0; i < symbol_table_length; i++)
		symbol_table[i] = @nil;
	intern_builtin_symbols();
}

/* --- end of femtolisp system */


Object read_program(const char **s)
{
	Object p = @nil;
	Object e;
	for (;;) {
		skip_white(s);
		if (**s == '\0')
			return nreverse(p);
		e = read_object(s);
		if (e == @closed_parens) {
			read_fatal("unexpected closed parens", *s);
			return NULL;
		}
		push(e, p);
	}
}

void error_unless(int condition, const char *message, Object x)
{
	printf("Error: %s.\nat: ", message);
	print(x);
	printf("\n");
	exit(1);
}

/* Translates a statement into bytecodes.  Modifies *p and returns
 * the number of output bytes.
 */
int translate_stat(Object stat, unsigned char *p, int max_length)
{
	/* Statement takes at least one byte. */
	error_unless(max_length >= 1, "program too long", stat);
	error_unless(consp(stat), "invalid statement", stat);
	if (first(stat) == @key) {
		unsigned int row, column;
		error_unless(length(stat) == 3,
			     "bad 'key' statement (usage: (key <row> <col>))",
			     stat);
		error_unless(numberp(second(stat)),
			     "<row> is not a number", stat);
		error_unless(numberp(third(stat)),
			     "<col> is not a number", stat);
		row = numberval(second(stat));
		column = numberval(third(stat));
		error_unless(row < 8, "invalid <row>", stat);
		error_unless(column < 16, "invalid <column>", stat);
		*p = column + (row << 4);
		return 1;
	}
	if (first(stat) == @repeat) {
		unsigned int n_written, count, nstats;
		error_unless(length(stat) >= 3,
			     "bad 'repeat' statement "
			     "(usage: (repeat <times> <stat> [..]))",
			     stat);
		error_unless(numberp(second(stat)),
			     "first arg to 'repeat' must be int",
			     stat);
		count = numberval(second(stat));
		error_unless(2 <= count && count <= 5,
			     "can only repeat between 2 and 5 times",
			     stat);
		nstats = length(stat) - 2;
		error_unless(1 <= nstats && nstats <= 4,
			     "can only repeat between 1 and 4 statements",
			     stat),
			*p++ = 0x90 | ((nstats - 1) << 2) | (count - 2);
		max_length--;
		n_written = 1;
		dolist(s, cdr(cdr(stat))) {
			int l = translate_stat(s, p, max_length);
			max_length -= l;
			n_written += l;
			p += l;
		} odlist;
		return n_written;
	}
	if (first(stat) == @pause) {
		unsigned int x;
		error_unless(length(stat) == 2,
			     "bad 'pause' statement "
			     "(usage: (pause <time-index>))\n",
			     stat);
		x = numberval(second(stat));
		*p = 0x80 | x;
		return 1;
	}
	error_unless(0, "unknown statement type", stat);
	return 0;  /* not reached */
}

int translate_program(Object program, unsigned char *output, int max_length)
{
	int n_written = 0;

	dolist(stat, program) {
		int l = translate_stat(stat, output, max_length);
		max_length -= l;
		n_written += l;
		output += l;
	} odlist;

	return n_written;
}

int main(int ac, char **av)
{
	char input[10000];
	Object program;
	const char *s;
	unsigned char output[100];
	int olength;
	int i;

	int l = fread(input, 1, sizeof(input) - 1, stdin);
	if (l == sizeof(input) - 1) {
		printf("program too long\n");
		exit(1);
	}
	input[l] = '\0';

	femtolisp_init();

	s = (const char *) input;
	program = read_program(&s);
	olength = translate_program(program, output, sizeof(output));

	for (i = 0; i < olength; i++)
		printf("%02x", output[i]);
	printf("\n");

	return 0;
}
