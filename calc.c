/* calc.c	01/13/1996
 * a simple calcualtor.
 */

/* Copyright (c) 1995,1996 Sascha Demetrio
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *    If you modify any part of HEXER and resitribute it, you must add
 *    a notice to the `README' file and the modified source files containing
 *    information about the  changes you made.  I do not want to take
 *    credit or be blamed for your modifications.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *    If you modify any part of HEXER and resitribute it in binary form,
 *    you must supply a `README' file containing information about the
 *    changes you made.
 * 3. The name of the developer may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * HEXER WAS DEVELOPED BY SASCHA DEMETRIO.
 * THIS SOFTWARE SHOULD NOT BE CONSIDERED TO BE A COMMERCIAL PRODUCT.
 * THE DEVELOPER URGES THAT USERS WHO REQUIRE A COMMERCIAL PRODUCT
 * NOT MAKE USE OF THIS WORK.
 *
 * DISCLAIMER:
 * THIS SOFTWARE IS PROVIDED BY THE DEVELOPER ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE DEVELOPER BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "defs.h"

#define NDEBUG

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <math.h>
#if HAVE_FLOAT_H
#include <float.h>
#endif
#include <limits.h>
#if USE_STDARG
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#if HAVE_ALLOCA
#if NEED_ALLOCA_H
#include <alloca.h>
#endif
#else
char *alloca();
#endif

#include "calc.h"
#include "readline.h"
#include "tio.h"

#define CALC_STACKSIZE 1024

#ifndef DBL_EPSILON
#define DBL_EPSILON (1e-9)
#endif

#ifndef LONG_BITS
#define LONG_BITS 32
#endif

#ifndef DEBUG
#define DEBUG 0
#endif

static int calc_stack_initialized_f = 0;
static int calc_command_counter = 0;

enum calc_object_e {
  CO_NONE,                           /* uninitialized */
  CO_INTEGER, CO_FLOAT, CO_BOOLEAN,  /* numeric values */
  CO_UNARY_OPERATOR,                 /* unary operator sign */
  CO_BINARY_OPERATOR,                /* binary operator sign */
  CO_VARIABLE,                       /* variable name */
  CO_ERROR                           /* error message */
};

#define CO_VALUE(x) (x.type == CO_INTEGER                                     \
                     || x.type == CO_FLOAT                                    \
                     || x.type == CO_BOOLEAN)

static char *calc_object_names[] = {
  "none", "integer", "float", "boolean", "unary operator", "binary operator",
  "variable", "error", 0
};

enum calc_unary_operator_e {
  OP_NEG, OP_NOT, OP_COMP,
  OP_PAROPEN, OP_PARCLOSE,
  OP_EXEC,
  OP_max_unary_operator
};

static char *calc_unary_operator_names[OP_max_unary_operator] = {
  "-  neg", "!  not", "~  comp", "(  paropen", ")  parclose", ":  exec"
};

enum calc_binary_operator_e {
  OP_POW,
  OP_MUL, OP_DIV, OP_MOD,
  OP_ADD, OP_SUB,
  OP_SHL, OP_SHR,
  OP_LES, OP_LEQ, OP_GRT, OP_GEQ,
  OP_EQU, OP_NEQ,
  OP_AND, OP_XOR, OP_OR,
  OP_LAND, OP_LOR,
  OP_ASSIGN,
  OP_max_binary_operator
};

static int prec[OP_max_binary_operator] = {
  0, 1, 1, 1, 2, 2, 3, 3, 4, 4, 4, 4, 5, 5, 6, 6, 6, 7, 7, 8
};

static char *calc_binary_operator_names[OP_max_binary_operator] = {
  "** pow",
  "*  mul", "/  div", "%  mod", "+  add", "-  sub", "<< shl", ">> shr",
  "<  les", "<= leq", ">  grt", ">= geq", "== equ", "!= neq",
  "&  and", "^  xor", "|  or", "&& land", "|| lor", "=  assign"
};

struct calc_object_s {
  enum calc_object_e type;
  union {
    long i;                            /* `CO_INTEGER', `CO_BOOLEAN' */
    double f;                          /* `CO_FLOAT' */
    char *s;                           /* `CO_VARIABLE', `CO_ERROR' */
    enum calc_unary_operator_e uo;     /* `CO_UNARY_OPERATOR' */
    enum calc_binary_operator_e bo;    /* `CO_BINARY_OPERATOR' */
  } u;
};

/* a simple list of variables.
 */
struct calc_variable_s {
  char *name;
  struct calc_object_s object;
  struct calc_variable_s *next;
} *calc_variable_list = 0;

#ifdef CALC_COMPILE
enum calc_code_e {
  CC_VALUE,          /* immediate value */
  CC_INSTRUCTION     /* instruction */
};

static char *calc_code_names[] = {
  "VALUE", "INSTRUCTION", 0
};

#if 0
enum calc_instruction_e {
  CI_ADD,      /* add the top two elements on the stack */
  CI_ADDI,     /* add an immediate value to the top element */
  CI_SUB,      /* substact the top two elements */
  CI_SUBI,     /* substact immediate value from the top element */
  CI_MUL,      /* multiply the top two elements */
  CI_MULI,     /* multiply an immediate value with the top element */
  CI_POW,      /* compute s[-2] ** s[-1] */
  CI_POWI,     /* compute s[-1] ** i (immediate value) */
  CI_IDIV,     /* integer divide the top two elements */
  CI_IDIVI,    /* integer divide the top element by an immediate value */
  CI_DIV,      /* divide the top two elements */
  CI_DIVI,     /* divide the top element by an immediate value */
  CI_SHL,      /* shift left s[-2] by s[-1] bits */
  CI_SHLI,     /* shift left the top element by an immediate value */
  CI_SHR,      /* shift right s[-2] by s[-1] bits */
  CI_SHRI,     /* shift right the top element by an immediate value */
  CI_LES,      /* check if the top element is less than zero */
  CI_LEQ,      /* check if the top element is less or equal than zero */
  CI_GRT,      /* check if the top element is greater than zero */
  CI_GEQ,      /* check if the top element is greater or equal than zero */
  CI_EQU,      /* check if the top element is equal to zero */
  CI_NEQ,      /* check if the top element is not equal to zero */
  CI_PUSH,     /* push an immediate value onto the execution stack */
  CI_POP,      /* remove an element from the execution stack */
  CI_VALUE,    /* push the value of a variable onto the execution stack */
  CI_ASSIGN,   /* assign the top element to a variable */
#if 0
  CI_BEQ,      /* branch on equal */
  CI_BNE,      /* branch on not equal */
#endif
  CI_MAX       /* number of instructions */
};

static char *calc_instruction_names[] = {
  "ADD", "ADDI", "SUB", "SUBI", "MUL", "MULI", "POW", "POWI", "IDIV", "IDIVI",
  "DIV", "DIVI", "SHL", "SHLI", "SHR", "SHRI", "LES", "LEQ", "EQU", "NEQ",
  "PUSH", "POP", "VALUE", "ASSIGN",
  "BEQ", "BNE", 0
};
#endif

struct calc_code_s {
  enum calc_code_e type;
  union {
    enum calc_instruction_e instruction;
    struct calc_object_s object;
  } u;
};
#endif

static struct calc_object_s stack[CALC_STACKSIZE];
static int sp;

  static int
calc_initialize_stack()
{
  int i;

  if (calc_stack_initialized_f) return 0;
  ++calc_stack_initialized_f;
  for (i = 0; i < CALC_STACKSIZE; ++i) stack[i].type = CO_NONE;
  return 0;
}
/* calc_initialize_stack */

  static int
calc_clear_stack(count)
  int count;
  /* remove the trailing `count' stack elements from the stack.
   */
{
  for (--sp; count; --count, --sp) {
    switch (stack[sp].type) {
    case CO_VARIABLE:
    case CO_ERROR:
      free(stack[sp].u.s);
      stack[sp].u.s = 0;
      break;
    default:
      break;
    }
    stack[sp].type = CO_NONE;
  }
  ++sp;
  return 0;
}
/* calc_clear_stack */

  static int
calc_set_stack(position, object)
  int position;
  struct calc_object_s object;
{
  if (stack[position].type == CO_ERROR || stack[position].type == CO_VARIABLE)
    free(stack[position].u.s);
  stack[position] = object;
  return 0;
}
/* calc_set_stack */

  static int
calc_print(object, verbose)
  struct calc_object_s object;
  int verbose;
{
  if (verbose) printf("(%s) ", calc_object_names[(int)object.type]);
  switch(object.type) {
  case CO_BINARY_OPERATOR:
    printf("%s", calc_binary_operator_names[(int)object.u.bo] + 3);
    break;
  case CO_UNARY_OPERATOR:
    printf("%s", calc_unary_operator_names[(int)object.u.uo] + 3);
    break;
  case CO_INTEGER:
    printf("%li 0%lo 0x%lx", object.u.i, object.u.i, object.u.i);
    break;
  case CO_FLOAT:
    printf("%g", object.u.f);
    break;
  case CO_BOOLEAN:
    printf("%s", object.u.i ? "true" : "false");
    break;
  case CO_ERROR:
  case CO_VARIABLE:
    printf("%s", object.u.s);
    break;
  default:
    printf("INVALID OBJECT");
    return -1;
  }
  return 0;
}
/* calc_print */

  static char *
calc_sprint(s, object)
  char *s;
  struct calc_object_s object;
{
  switch(object.type) {
  case CO_BINARY_OPERATOR:
    sprintf(s, "%s", calc_binary_operator_names[(int)object.u.bo] + 3);
    break;
  case CO_UNARY_OPERATOR:
    sprintf(s, "%s", calc_unary_operator_names[(int)object.u.uo] + 3);
    break;
  case CO_INTEGER:
    sprintf(s, "%li 0%lo 0x%lx", object.u.i, object.u.i, object.u.i);
    break;
  case CO_FLOAT:
    sprintf(s, "%g", object.u.f);
    break;
  case CO_BOOLEAN:
    sprintf(s, "%s", object.u.i ? "true" : "false");
    break;
  case CO_ERROR:
  case CO_VARIABLE:
    sprintf(s, "%s", object.u.s);
    break;
  default:
    sprintf(s, "INVALID OBJECT");
  }
  return s;
}
/* calc_print */

  static int
calc_stack()
{
  int i;

  for (i = 0; i < sp; ++i) {
    printf("%i: ", i);
    calc_print(stack[i], 1);
    putchar('\n');
  }
  return 0;
}
/* calc_stack */

  static int
#if USE_STDARG
calc_error(struct calc_object_s *x, const char *fmt, ...)
#else
calc_error(x, fmt, va_alist)
  struct calc_object_s *x;
  char *fmt;
  va_dcl
#endif
{
  va_list ap;
  char buf[256];

#if USE_STDARG
  va_start(ap, fmt);
#else
  va_start(ap);
#endif
  vsprintf(buf, fmt, ap);
  if (x->type != CO_ERROR) {
    x->type = CO_ERROR;
    x->u.s = strdup(buf);
  }
  va_end(ap);
  return 0;
}
/* calc_error */

  static int
calc_evaluate(object)
  struct calc_object_s *object;
{
  struct calc_variable_s *i;

  assert(object->type == CO_VARIABLE);
  for (i = calc_variable_list; i; i = i->next)
    if (!strcmp(object->u.s, i->name)) {
      *object = i->object;
      return 0;
    }
  calc_error(object, "undefined variable `%s'", object->u.s);
  return -1;
}
/* calc_evaluate */

  static int
calc_assign(x, y)
  struct calc_object_s x, y;
{
  struct calc_variable_s *i;

  assert(x.type == CO_VARIABLE);
  assert(y.type != CO_VARIABLE);
  assert(y.type != CO_ERROR);
  for (i = calc_variable_list; i; i = i->next)
    if (!strcmp(x.u.s, i->name)) {
      i->object = y;
      return 0;
    }
  i = (struct calc_variable_s *)malloc(sizeof *i);
  i->object = y;
  i->name = strdup(x.u.s);
  i->next = calc_variable_list;
  calc_variable_list = i;
  return 0;
}
/* calc_assign */

  static int
calc_operation(position, binary)
  int position;
  int binary;
  /* evaluate the simple expression on stack position `position' and
   * update the stack.  if `binary == 0', a unary expression is expected,
   * else a binary expression is expected.  the function returns 0 on
   * success and -1 on error.
   */
{
  struct calc_object_s x, y, op;
  int int_f;
  int error_f = 0;
  int remove;  /* number of elements to remove from the stack */
  int i;

  if (binary) {  /* binary operator */
    remove = 2;
    assert(position + 2 < sp);
    x = stack[position];
    op = stack[position + 1];
    y = stack[position + 2];
    if (x.type == CO_VARIABLE && op.u.bo != OP_ASSIGN) calc_evaluate(&x);
    if (y.type == CO_VARIABLE) calc_evaluate(&y);
    if (op.type != CO_BINARY_OPERATOR || !CO_VALUE(y)) {
      if (y.type == CO_ERROR) x = y; else calc_error(&x, "syntax error");
      ++error_f;
      goto exit;
    }
    switch (op.u.bo) {
    case OP_POW:
      int_f = 0;
      switch (x.type) {
      case CO_BOOLEAN:
      case CO_INTEGER:
        int_f = 1;
        x.type = CO_FLOAT;
        x.u.f = (double)x.u.i;
      case CO_FLOAT:
        if (y.type != CO_FLOAT) {
          y.type = CO_FLOAT;
          y.u.f = (double)y.u.i;
        } else
          int_f = 0;
        x.u.f = pow(x.u.f, y.u.f);
        if (int_f && x.u.f < (unsigned)(1 << (LONG_BITS - 1)) - 1) {
          x.type = CO_INTEGER;
          x.u.i = (int)x.u.f;
        }
        break;
      default:
        calc_error(&x, "syntax error");
        ++error_f;
        break;
      }
      break;
    case OP_MUL:
      switch (x.type) {
      case CO_BOOLEAN:
        if (y.type == CO_INTEGER) x.type = CO_INTEGER;
      case CO_INTEGER:
        if (y.type != CO_FLOAT) {
          x.u.i *= y.u.i;
          break;
        } else {
          x.u.f = (double)x.u.i;
          x.type = CO_FLOAT;
        }
      case CO_FLOAT:
        x.u.f *= y.type == CO_FLOAT ? y.u.f : (double)y.u.i;
        break;
      default:
        calc_error(&x, "syntax error");
        ++error_f;
        break;
      }
      break;
    case OP_DIV:
      if (y.type == CO_FLOAT ? fabs(y.u.f) < DBL_EPSILON : !y.u.i) {
        calc_error(&x, "division by zero");
        ++error_f;
        break;
      }
      switch (x.type) {
      case CO_BOOLEAN:
        if (y.type == CO_INTEGER) x.type = CO_INTEGER;
      case CO_INTEGER:
        if (y.type != CO_FLOAT) {
          x.u.i /= y.u.i;
          break;
        } else {
          x.u.f = (double)x.u.i;
          x.type = CO_FLOAT;
        }
      case CO_FLOAT:
        x.u.f /= y.type == CO_FLOAT ? y.u.f : (double)y.u.i;
        break;
      default:
        calc_error(&x, "syntax error");
        ++error_f;
        break;
      }
      break;
    case OP_MOD:
      if (x.type == CO_FLOAT || y.type == CO_FLOAT) {
        calc_error(&x, "invalid type in binary operator MOD (%%)");
        ++error_f;
        break;
      }
      switch (x.type) {
      case CO_BOOLEAN:
        if (y.type == CO_INTEGER) x.type = CO_INTEGER;
      case CO_INTEGER:
        x.u.i %= y.u.i;
        break;
      case CO_FLOAT:
      default:
        calc_error(&x, "syntax error");
        ++error_f;
        break;
      }
      break;
    case OP_ADD:
      switch (x.type) {
      case CO_BOOLEAN:
        if (y.type == CO_INTEGER) x.type = CO_INTEGER;
      case CO_INTEGER:
        if (y.type != CO_FLOAT) {
          x.u.i += y.u.i;
          break;
        } else {
          x.u.f = (double)x.u.i;
          x.type = CO_FLOAT;
        }
      case CO_FLOAT:
        x.u.f += y.type == CO_FLOAT ? y.u.f : (double)y.u.i;
        break;
      default:
        calc_error(&x, "syntax error");
        ++error_f;
        break;
      }
      break;
    case OP_SUB:
      switch (x.type) {
      case CO_BOOLEAN:
        if (y.type == CO_INTEGER) x.type = CO_INTEGER;
      case CO_INTEGER:
        if (y.type != CO_FLOAT) {
          x.u.i -= y.u.i;
          break;
        } else {
          x.u.f = (double)x.u.i;
          x.type = CO_FLOAT;
        }
      case CO_FLOAT:
        x.u.f -= y.type == CO_FLOAT ? y.u.f : (double)y.u.i;
        break;
      default:
        calc_error(&x, "syntax error");
        ++error_f;
        break;
      }
      break;
    case OP_SHL:
      if (x.type == CO_FLOAT || y.type == CO_FLOAT) {
        calc_error(&x, "invalid type in binary operator SHL (<<)");
        ++error_f;
        break;
      }
      switch (x.type) {
      case CO_BOOLEAN:
        if (y.type == CO_INTEGER) x.type = CO_INTEGER;
      case CO_INTEGER:
        if (y.u.i >= LONG_BITS) y.u.i = LONG_BITS - 1;
        x.u.i <<= y.u.i;
        break;
      case CO_FLOAT:
      default:
        calc_error(&x, "syntax error");
        ++error_f;
        break;
      }
      break;
    case OP_SHR:
      if (x.type == CO_FLOAT || y.type == CO_FLOAT) {
        calc_error(&x, "invalid type in binary operator SHR (>>)");
        ++error_f;
        break;
      }
      switch (x.type) {
      case CO_BOOLEAN:
        if (y.type == CO_INTEGER) x.type = CO_INTEGER;
      case CO_INTEGER:
        if (y.u.i >= LONG_BITS) y.u.i = LONG_BITS - 1;
        x.u.i >>= y.u.i;
        break;
      case CO_FLOAT:
      default:
        calc_error(&x, "syntax error");
        ++error_f;
        break;
      }
      break;
    case OP_LES:
      switch (x.type) {
      case CO_BOOLEAN:
      case CO_INTEGER:
        if (y.type != CO_FLOAT) {
          x.u.i = x.u.i < y.u.i;
          break;
        } else
          x.u.f = (double)x.u.i;
      case CO_FLOAT:
        x.u.i = x.u.f < (y.type == CO_FLOAT ? y.u.f : (double)y.u.i);
        break;
      default:
        calc_error(&x, "syntax error");
        ++error_f;
        break;
      }
      x.type = CO_BOOLEAN;
      break;
    case OP_LEQ:
      switch (x.type) {
      case CO_BOOLEAN:
      case CO_INTEGER:
        if (y.type != CO_FLOAT) {
          x.u.i = x.u.i <= y.u.i;
          break;
        } else
          x.u.f = (double)x.u.i;
      case CO_FLOAT:
        x.u.i = x.u.f <= (y.type == CO_FLOAT ? y.u.f : (double)y.u.i);
        break;
      default:
        calc_error(&x, "syntax error");
        ++error_f;
        break;
      }
      x.type = CO_BOOLEAN;
      break;
    case OP_GRT:
      switch (x.type) {
      case CO_BOOLEAN:
      case CO_INTEGER:
        if (y.type != CO_FLOAT) {
          x.u.i = x.u.i > y.u.i;
          break;
        } else
          x.u.f = (double)x.u.i;
      case CO_FLOAT:
        x.u.i = x.u.f > (y.type == CO_FLOAT ? y.u.f : (double)y.u.i);
        break;
      default:
        calc_error(&x, "syntax error");
        ++error_f;
        break;
      }
      x.type = CO_BOOLEAN;
      break;
    case OP_GEQ:
      switch (x.type) {
      case CO_BOOLEAN:
      case CO_INTEGER:
        if (y.type != CO_FLOAT) {
          x.u.i = x.u.i >= y.u.i;
          break;
        } else
          x.u.f = (double)x.u.i;
      case CO_FLOAT:
        x.u.i = x.u.f >= (y.type == CO_FLOAT ? y.u.f : (double)y.u.i);
        break;
      default:
        calc_error(&x, "syntax error");
        ++error_f;
        break;
      }
      x.type = CO_BOOLEAN;
      break;
    case OP_EQU:
      switch (x.type) {
      case CO_BOOLEAN:
      case CO_INTEGER:
        if (y.type != CO_FLOAT) {
          x.u.i = x.u.i == y.u.i;
          break;
        } else
          x.u.f = (double)x.u.i;
      case CO_FLOAT:
        if (y.type == CO_INTEGER) y.u.f = (double)y.u.i;
        x.u.i = fabs(x.u.f - y.u.f) < DBL_EPSILON;
        break;
      default:
        calc_error(&x, "syntax error");
        ++error_f;
        break;
      }
      x.type = CO_BOOLEAN;
      break;
    case OP_NEQ:
      switch (x.type) {
      case CO_BOOLEAN:
      case CO_INTEGER:
        if (y.type != CO_FLOAT) {
          x.u.i = x.u.i != y.u.i;
          break;
        } else
          x.u.f = (double)x.u.i;
      case CO_FLOAT:
        if (y.type == CO_INTEGER) y.u.f = (double)y.u.i;
        x.u.i = fabs(x.u.f - y.u.f) >= DBL_EPSILON;
        break;
      default:
        calc_error(&x, "syntax error");
        ++error_f;
        break;
      }
      x.type = CO_BOOLEAN;
      break;
    case OP_AND:
      if (x.type == CO_FLOAT || y.type == CO_FLOAT) {
        calc_error(&x, "invalid type in binary operator AND (&)");
        ++error_f;
        break;
      }
      switch (x.type) {
      case CO_BOOLEAN:
        if (y.type == CO_INTEGER) x.type = CO_INTEGER;
      case CO_INTEGER:
        if (y.u.i >= LONG_BITS) y.u.i = LONG_BITS - 1;
        x.u.i &= y.u.i;
        break;
      case CO_FLOAT:
      default:
        calc_error(&x, "syntax error");
        ++error_f;
        break;
      }
      break;
    case OP_XOR:
      if (x.type == CO_FLOAT || y.type == CO_FLOAT) {
        calc_error(&x, "invalid type in binary operator XOR (^)");
        ++error_f;
        break;
      }
      switch (x.type) {
      case CO_BOOLEAN:
        if (y.type == CO_INTEGER) x.type = CO_INTEGER;
      case CO_INTEGER:
        if (y.u.i >= LONG_BITS) y.u.i = LONG_BITS - 1;
        x.u.i ^= y.u.i;
        break;
      case CO_FLOAT:
      default:
        calc_error(&x, "syntax error");
        ++error_f;
        break;
      }
      break;
    case OP_OR:
      if (x.type == CO_FLOAT || y.type == CO_FLOAT) {
        calc_error(&x, "invalid type in binary operator OR (|)");
        ++error_f;
        break;
      }
      switch (x.type) {
      case CO_BOOLEAN:
        if (y.type == CO_INTEGER) x.type = CO_INTEGER;
      case CO_INTEGER:
        if (y.u.i >= LONG_BITS) y.u.i = LONG_BITS - 1;
        x.u.i |= y.u.i;
        break;
      case CO_FLOAT:
      default:
        calc_error(&x, "syntax error");
        ++error_f;
        break;
      }
      break;
    case OP_LAND:  /* logical AND */
      switch (x.type) {
      case CO_BOOLEAN:
      case CO_INTEGER:
        x.u.i = x.u.i
          && (y.type == CO_FLOAT ? fabs(y.u.f) >= DBL_EPSILON : y.u.i);
        break;
      case CO_FLOAT:
        x.u.i = fabs(x.u.f) >= DBL_EPSILON
          && (y.type == CO_FLOAT ? fabs(y.u.f) >= DBL_EPSILON : y.u.i);
        break;
      default:
        calc_error(&x, "syntax error");
        ++error_f;
        break;
      }
      x.type = CO_BOOLEAN;
      break;
    case OP_LOR:  /* logical OR */
      switch (x.type) {
      case CO_BOOLEAN:
      case CO_INTEGER:
        x.u.i = x.u.i
          || (y.type == CO_FLOAT ? fabs(y.u.f) >= DBL_EPSILON : y.u.i);
        break;
      case CO_FLOAT:
        x.u.i = fabs(x.u.f) >= DBL_EPSILON
          || (y.type == CO_FLOAT ? fabs(y.u.f) >= DBL_EPSILON : y.u.i);
        break;
      default:
        calc_error(&x, "syntax error");
        ++error_f;
        break;
      }
      x.type = CO_BOOLEAN;
      break;
    case OP_ASSIGN:
      if (x.type != CO_VARIABLE) {
        calc_error(&x, "invalid l-value in assignment");
        ++error_f;
        break;
      }
      calc_assign(x, y);
      x = y;
      break;
    default:
      calc_error(&x, "syntax error");
      ++error_f;
      break;
    }
  } else {  /* unary operator */
    remove = 1;
    assert(position + 1 < sp);
    op = stack[position];
    x = stack[position + 1];
    assert(op.type == CO_UNARY_OPERATOR);
    if (x.type == CO_VARIABLE) calc_evaluate(&x);
    switch (op.u.uo) {
    case OP_NEG:
      switch (x.type) {
      case CO_BOOLEAN:
      case CO_INTEGER:
        x.u.i = -x.u.i;
        break;
      case CO_FLOAT:
        x.u.f = -x.u.f;
        break;
      default:
        calc_error(&x, "syntax error");
        ++error_f;
        break;
      }
      break;
    case OP_NOT:  /* logical NOT */
      switch (x.type) {
      case CO_INTEGER:
        x.type = CO_BOOLEAN;
      case CO_BOOLEAN:
        x.u.i = !x.u.i;
        break;
      case CO_FLOAT:
        x.u.i = !x.u.f;
        x.type = CO_BOOLEAN;
        break;
      default:
        calc_error(&x, "syntax error");
        ++error_f;
        break;
      }
      break;
    case OP_COMP:  /* bit complement */
      if (x.type == CO_FLOAT) {
        calc_error(&x, "invalid type in unary operator COMP (~)");
        ++error_f;
        break;
      }
      switch (x.type) {
      case CO_BOOLEAN:
        x.type = CO_INTEGER;
      case CO_INTEGER:
        x.u.i = ~x.u.i;
        break;
      default:
        calc_error(&x, "syntax error");
        ++error_f;
        break;
      }
      break;
    case OP_PAROPEN:
      break;
    case OP_PARCLOSE:
    case OP_EXEC:
      /* `calc_operation()' should never be called for `OP_EXEC' or
       * `OP_PARCLOSE'. */
    default:
      calc_error(&x, "syntax error");
      ++error_f;
      break;
    }
  }
exit:
  for (i = position + 1; i < sp; ++i) calc_set_stack(i, stack[i + remove]);
  calc_clear_stack(remove);
  calc_set_stack(position, x);
  return error_f;
}
/* calc_operation */

  static int
calc_reduce()
  /* try to reduce to top of the stack.  return values:
   * -1:  reduction failed due to an error.  an error element has been put
   *      onto the stack.
   * 0:   no reduction.
   * 1:   reduction succeeded.
   */
{
  struct calc_object_s x, y, z;
  int error_f = 0;
  int reduce_f = 0;

  if (sp < 2) goto exit_calc_reduce;
  x = stack[sp - 1];
  y = stack[sp - 2];
  switch (x.type) {
  case CO_INTEGER: case CO_FLOAT: case CO_BOOLEAN: case CO_VARIABLE:
    switch (y.type) {
    case CO_UNARY_OPERATOR:
      if (y.u.uo != OP_PAROPEN) {
unary:  error_f = calc_operation(sp - 2, 0);
        ++reduce_f;
      }
      break;
    case CO_BINARY_OPERATOR:
      /* check if `y' is an unary minus */
      if (y.u.bo == OP_SUB && (sp == 2 ? 1 : !CO_VALUE(stack[sp - 3]))) {
        stack[sp - 2].type = CO_UNARY_OPERATOR;
        stack[sp - 2].u.uo = OP_NEG;
        goto unary;
      }
      break;
    default:
      calc_error(stack + sp++, "syntax error");
      ++error_f;
      break;
    }
    break;
  case CO_UNARY_OPERATOR:
    if (x.u.uo == OP_PARCLOSE)
      goto eval;
    else if (x.u.uo == OP_EXEC && sp > 3)
      goto exec;
    break;
  case CO_BINARY_OPERATOR:
    if (sp > 3) {
exec: z = stack[sp - 3];
      if (z.type == CO_BINARY_OPERATOR) {
        if (!CO_VALUE(y) && y.type != CO_VARIABLE) {
          calc_error(stack + sp++, "syntax error");
          ++error_f;
          break;
        } else if (prec[(int)z.u.bo] <= prec[(int)x.u.bo]
                   || x.type == CO_UNARY_OPERATOR) {
          error_f = calc_operation(sp - 4, 1);
          ++reduce_f;
          break;
        }
      } else {
        if (z.type != CO_UNARY_OPERATOR || z.u.uo != OP_PAROPEN) {
          calc_error(stack + sp++, "syntax error");
          ++error_f;
          break;
        }
      }
    }
    break;
  default: abort();
  }
exit_calc_reduce:
  return error_f ? -1 : reduce_f;

eval: /* evaluate a parenthesed expression */
  if (sp > 1 && y.type == CO_UNARY_OPERATOR ? y.u.uo == OP_PAROPEN : 0) {
    calc_clear_stack(2);
    calc_error(stack + sp++, "empty parentheses");
    ++error_f;
  } else if (sp > 2) {
    z = stack[sp - 3];
    if (z.type == CO_BINARY_OPERATOR && sp > 3)
      goto exec;
    else if (z.type == CO_UNARY_OPERATOR) {
      assert(z.u.uo == OP_PAROPEN);
      if (!CO_VALUE(y)) {
        calc_error(&y, "invalid object (%s) in parentheses",
                   calc_object_names[(int)y.type]);
        ++error_f;
      }
      stack[sp - 3] = y;
      calc_clear_stack(2);
      ++reduce_f;
    }
  } else {
    calc_clear_stack(1);
    calc_error(stack + sp++, "unmatched ')'");
    ++error_f;
  }
  return error_f ? -1 : reduce_f;
}
/* calc_reduce */

  static int
istrue(s, endptr)
  char *s;
  char **endptr;
{
  static char *true_strings[] = { "true", "t", "TRUE", "True", "T",
    "yes", "y", "YES", "Yes", "on", "ON", "On", 0 };
  int i, l;

  for (i = 0; l = (true_strings[i] ? strlen(true_strings[i]) : 0); ++i)
    if (!strncmp(true_strings[i], s, l)) {
      *endptr = s + l;
      return 1;
    }
  *endptr = s;
  return 0;
}
/* istrue */

  static int
isfalse(s, endptr)
  char *s;
  char **endptr;
{
  static char *false_strings[] = { "false", "f", "FALSE", "False", "F",
    "no", "n", "NO", "No", "off", "OFF", "Off", 0 };
  int i, l;

  for (i = 0; l = (false_strings[i] ? strlen(false_strings[i]) : 0); ++i)
    if (!strncmp(false_strings[i], s, l)) {
      *endptr = s + l;
      return 1;
    }
  *endptr = s;
  return 0;
}
/* isfalse */

#ifndef XDIGIT
#define XDIGIT(x) (isdigit(x) ? x - '0' : tolower(x) - 'a' + 0xa)
#endif

  static int
calc_scan(exp)
  char *exp;
{
  char *p, *q, *r;
  long val;
  double fval;
  struct calc_object_s x;
  int float_f = 0;
  int base;
  int i;

  calc_clear_stack(sp);
  for (p = exp; p;) {
    while (isspace(*p)) ++p;
    if (!*p) break;
    if (isdigit(*p) || *p == '.') {
      if (*p == '0') {
        if (tolower(p[1]) == 'x') /* hex */
          base = 0x10, r = p + 2;
        else if (isdigit(p[1])) /* oct */
          base = 010, r = p + 1;
        else
          goto dec;
      } else { /* dec */
dec:    base = 10, r = p;
        for (q = p, val = 0; isdigit(*q); ++q);
        if (*q == '.' || *q == 'e' || *q == 'E') ++float_f;
      }
      if (float_f) {
        fval = strtod(r, &p);
        x.type = CO_FLOAT;
        x.u.f = fval;
      } else {
        val = strtol(r, &p, base);
        x.type = CO_INTEGER;
        x.u.i = val;
      }
    } else if (istrue(p, &p)) {
      x.type = CO_BOOLEAN;
      x.u.i = 1;
    } else if (isfalse(p, &p)) {
      x.type = CO_BOOLEAN;
      x.u.i = 0;
    } else if (isalpha(*p) || *p == '_' || *p == '$') {
      for (q = p + 1; isalnum(*q) || *q == '_' || *q == '$'; ++q);
      x.type = CO_VARIABLE;
      x.u.s = strdup(p);
      x.u.s[q - p] = 0;
      p = q;
    } else {
      switch (*p) {
      case '*':
        ++p;
        x.type = CO_BINARY_OPERATOR;
        if (*p == '*') {
          ++p;
          x.u.bo = OP_POW;
        } else
          x.u.bo = OP_MUL;
        break;
      case '/':
        ++p;
        x.type = CO_BINARY_OPERATOR;
        x.u.bo = OP_DIV;
        break;
      case '%':
        ++p;
        x.type = CO_BINARY_OPERATOR;
        x.u.bo = OP_MOD;
        break;
      case '+':
        ++p;
        x.type = CO_BINARY_OPERATOR;
        x.u.bo = OP_ADD;
        break;
      case '-':
        ++p;
        x.type = CO_BINARY_OPERATOR;
        x.u.bo = OP_SUB;
        break;
      case '<':
        ++p;
        switch (*p) {
        case '<':
          ++p;
          x.type = CO_BINARY_OPERATOR;
          x.u.bo = OP_SHL;
          break;
        case '=':
          ++p;
          x.type = CO_BINARY_OPERATOR;
          x.u.bo = OP_LEQ;
          break;
        default:
          x.type = CO_BINARY_OPERATOR;
          x.u.bo = OP_LES;
        }
        break;
      case '>':
        ++p;
        switch (*p) {
        case '>':
          ++p;
          x.type = CO_BINARY_OPERATOR;
          x.u.bo = OP_SHR;
          break;
        case '=':
          ++p;
          x.type = CO_BINARY_OPERATOR;
          x.u.bo = OP_GEQ;
          break;
        default:
          x.type = CO_BINARY_OPERATOR;
          x.u.bo = OP_GRT;
        }
        break;
      case '=':
        ++p;
        switch (*p) {
        case '=':
          ++p;
          x.type = CO_BINARY_OPERATOR;
          x.u.bo = OP_EQU;
          break;
        default:
          x.type = CO_BINARY_OPERATOR;
          x.u.bo = OP_ASSIGN;
        }
        break;
      case '!':
        ++p;
        switch (*p) {
        case '=':
          ++p;
          x.type = CO_BINARY_OPERATOR;
          x.u.bo = OP_NEQ;
          break;
        default:
          x.type = CO_UNARY_OPERATOR;
          x.u.uo = OP_NOT;
        }
        break;
      case '&':
        ++p;
        switch (*p) {
        case '&':
          ++p;
          x.type = CO_BINARY_OPERATOR;
          x.u.bo = OP_LAND;
          break;
        default:
          x.type = CO_BINARY_OPERATOR;
          x.u.bo = OP_AND;
        }
        break;
      case '|':
        ++p;
        switch (*p) {
        case '|':
          ++p;
          x.type = CO_BINARY_OPERATOR;
          x.u.bo = OP_LOR;
          break;
        default:
          x.type = CO_BINARY_OPERATOR;
          x.u.bo = OP_OR;
        }
        break;
      case '^':
        ++p;
        x.type = CO_BINARY_OPERATOR;
        x.u.bo = OP_XOR;
        break;
      case '~':
        ++p;
        x.type = CO_UNARY_OPERATOR;
        x.u.uo = OP_COMP;
        break;
      case '(':
        ++p;
        x.type = CO_UNARY_OPERATOR;
        x.u.uo = OP_PAROPEN;
        break;
      case ')':
        ++p;
        x.type = CO_UNARY_OPERATOR;
        x.u.uo = OP_PARCLOSE;
        break;
      default:
        calc_error(stack + sp++, "unexpected caracter %c (0x%x)",
                   isprint(*p) ? *p : ' ', (int)(unsigned char)*p);
        return -1;
      }
    }
    stack[sp++] = x;
#   if DEBUG
    calc_stack();
    while ((i = calc_reduce()) > 0) calc_stack();
    if (i < 0) {
      calc_stack();
      return -1;
    }
#   else
    while ((i = calc_reduce()) > 0);
    if (i < 0) return -1;
#   endif
  }
  x.type = CO_UNARY_OPERATOR;
  x.u.uo = OP_EXEC;
  stack[sp++] = x;
# if DEBUG
  calc_stack();
  while ((i = calc_reduce()) > 0) calc_stack();
  if (i < 0) calc_stack();
# else
  while ((i = calc_reduce()) > 0);
# endif
  return i;
}
/* calc_scan */

#ifdef MYCALC
  int
main(argc, argv)
  int argc;
  char **argv;
{
  char buf[256];
  int error_f = 0;
  int i;
  struct calc_object_s x;

  calc_initialize_stack();
  for (;;) {
    *buf = 0;
    if (argc > 1)
      for (i = 1; i < argc; ++i) strcat(buf, argv[i]);
    else
      fgets(buf, 256, stdin);
    if (!*buf) break;
    if (buf[strlen(buf) - 1] == '\n') buf[strlen(buf) - 1] = 0;
    if (calc_scan(buf)) {
      /* search for error objects on the stack */
      for (i = sp - 1; i >= 0; --i)
        if (stack[i].type == CO_ERROR) {
          calc_print(stack[i], 0);
          ++error_f;
          putchar('\n');
        }
      if (!error_f) {
        printf("unknown error\nstack:\n");
        calc_stack();
      }
    } else {
      if (stack[1].type != CO_UNARY_OPERATOR || stack[1].u.uo != OP_EXEC)
        printf("syntax error");
      else {
        if (stack[0].type == CO_VARIABLE) calc_evaluate(stack);
        if (stack[0].type == CO_ERROR)
          calc_print(stack[0], 0);
        else if (!CO_VALUE(stack[0]))
          printf("syntax error");
        else {
          if (argc == 1) printf("$%i = ", ++calc_command_counter);
          calc_print(stack[0], 0);
          x.type = CO_VARIABLE;
          x.u.s = (char *)alloca(256);
          sprintf(x.u.s, "$%i", calc_command_counter);
          calc_assign(x, stack[0]);
          sprintf(x.u.s, "$$");
          calc_assign(x, stack[0]);
        }
        putchar('\n');
      }
    }
    calc_clear_stack(sp);
    if (argc > 1) break;
  }
  return 0;
}
/* main */
#else
  char *
calculator(exp)
  char *exp;
{
  int i;
  int error_f = 0;
  static char result[256];
  struct calc_object_s x;

  calc_initialize_stack();
  if (calc_scan(exp)) {
    /* search for error objects on the stack */
    for (i = sp - 1; i >= 0; --i)
      if (stack[i].type == CO_ERROR) {
        calc_sprint(result, stack[i]);
        ++error_f;
        break;
      }
    if (!error_f)
      sprintf(result, "unknown error");
  } else {
    if (stack[1].type != CO_UNARY_OPERATOR || stack[1].u.uo != OP_EXEC)
      sprintf(result, "syntax error");
    else {
      if (stack[0].type == CO_VARIABLE) calc_evaluate(stack);
      if (stack[0].type == CO_ERROR)
        calc_sprint(result, stack[0]);
      else if (!CO_VALUE(stack[0]))
        sprintf(result, "syntax error");
      else {
        sprintf(result, "$%i = ", ++calc_command_counter);
        calc_sprint(result + strlen(result), stack[0]);
        x.type = CO_VARIABLE;
        x.u.s = (char *)alloca(256);
        sprintf(x.u.s, "$%i", calc_command_counter);
        calc_assign(x, stack[0]);
        sprintf(x.u.s, "$$");
        calc_assign(x, stack[0]);
      }
    }
  }
  calc_clear_stack(sp);
  return result;
}
/* calculator */
#endif

/* end of calc.c */

/* VIM configuration: (do not delete this line)
 *
 * vim:aw:bk:ch=2:nodg:efm=%f\:%l\:%m:et:hid:icon:
 * vim:sw=2:sc:sm:si:textwidth=79:to:ul=1024:wh=12:wrap:wb:
 */

