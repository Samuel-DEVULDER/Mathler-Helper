#if 0
gcc -DxSLOW_RAT -DxDEBUG -DxNUMBLE -DxEASY -DxNORMAL -DHARD -DxTHENUMBLE -Wall $0 -o a.out CBack-1.0/SRC/CBack.c -fshort-enums -O3 -fopenmp && time ./a.out
exit $?
#endif

/*
 * mathler.c - solves mathler-like games (https://www.mathler.com/,
 *             https://www.thenumble.app/) using the same technique as
 *             for the Mastermind(tm) (https://youtu.be/FR_71HyBytE).
 *
 * (c) 2022 by Samuel Devulder
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <time.h>
#include <sys/time.h>
#include <limits.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <locale.h>

#ifdef _OPENMP
#include <omp.h>
#endif

#include "CBack-1.0/SRC/CBack.h"

#ifdef EASY
#define SIZE    5
#define MAX_OP  1
#define URL     "https://easy.mathler.com/"

#elif defined(NORMAL)
#define SIZE    6
#define MAX_OP  2
#define URL     "https://mathler.com/"

#elif defined(HARD)
#define SIZE    8
#define MAX_OP  3
#define URL     "https://hard.mathler.com/"

#elif defined(THENUMBLE)
#define SIZE    7
#define URL     "https://www.thenumble.app/"

#elif defined(NUMBLE)
#define SIZE    8
#define URL     "https://www.mathix.org/numble/"

#else
#error Please define one of EASY, NORMAL, HARD, NUMBLE, THENUMBLE.
#define SIZE    1
#define URL     ""
#endif

#ifndef MAX_OP
#define MAX_OP  SIZE
#endif

#define DO_SHUFFLE          0 //defined(_OPENMP)
#define DO_SORT             1
#define FASTER_RAND         1
#define USE_IMPOSSIBLE      0

#define MAX_FORMULAE_EXACT  (15000)

/*****************************************************************************/

#define ALLOW_PARENTHESIS   (((SIZE)>=8) && !defined(NUMBLE))
#define PRIVATE             static

typedef int integer;

PRIVATE const char *A_BOLD="", *A_NORM=""; /* ansi escape sequence */

/*****************************************************************************/

PRIVATE int popcount(uint32_t _x) {
#ifdef __GNUC__
    return __builtin_popcount(_x);
#else
    uint32_t x = _x;
    x -= (x >> 1) & 0x55555555;
    x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
    x = (x + (x >> 4)) & 0x0f0f0f0f;
    x = (x * 0x01010101) >> 24;
    return x;
#endif
}

/*****************************************************************************/

PRIVATE void gettime(struct timeval *tv) {
    if(gettimeofday(tv, NULL)<0) {
        perror("gettimeofday");
        exit(EXIT_FAILURE);
    }
}

/*****************************************************************************/

#ifdef FASTER_RAND

#define XORSHIFT
// #define XOR128
// #define PARK_MILLER

#ifdef XORSHIFT
PRIVATE uint32_t _rnd_seed = 0xDEADBEEF;
PRIVATE void _srnd(int seed) {
    _rnd_seed = seed == 0 ? 0xABADCAFE : seed;
}
PRIVATE int _rnd(void) {
    uint32_t x = _rnd_seed;
    x ^= x<<13;
    x ^= x>>17;
    x ^= x<<5;
    _rnd_seed = x;
#if RAND_MAX==INT_MAX
    return x>>1;
#else
    return x&RAND_MAX;
#endif
}

#elif defined(PARK_MILLER)
PRIVATE uint32_t _rnd_seed = 0xDEADBEEF;
PRIVATE void _srnd(int seed) {
    _rnd_seed = seed == 0 || seed == 0x7fffffff ? 0xABADCAFE : seed;
}
PRIVATE int _rnd(void) {
    return _rnd_seed = (_rnd_seed * (uint64_t)48271) % 0x7fffffff;
}

#elif defined(XOR128)
PRIVATE void _srnd(int seed) {
    (void)seed;
}
PRIVATE int _rnd(void) {
  static uint32_t x = 123456789;
  static uint32_t y = 362436069;
  static uint32_t z = 521288629;
  static uint32_t w = 88675123;
  uint32_t t;
  t = x ^ (x << 11);
  x = y; y = z; z = w;
  return (w ^= (w >> 19) ^ (t ^ (t >> 8))) >> 1;
}
#endif

#define rand    _rnd
#define srand   _srnd
#endif

/*****************************************************************************/

PRIVATE int progress(int count) {
    static int cpt, cpt_sec, last;
    static struct timeval start;
    static long long total;

    if(count==0) { // done
        struct timeval curr, temp;
        int i;
        for(i=0; i<last; ++i) putchar(' ');
        for(i=0; i<last; ++i) putchar('\b');
        fflush(stdout);
        gettime(&curr);
        timersub(&curr, &start, &temp);
        return temp.tv_sec>INT_MAX ? INT_MAX : (int)temp.tv_sec;
    } else if(count<0) { // set
        total   = -count;
        cpt_sec = -30;
        last    = 0;
        cpt     = 0;
        gettime(&start);
    } else if(count == INT_MAX) {
        static char *mill = "-\\|/";
        if(((++cpt)&63)==0) {
            int i;
            last = printf(" (%c)", mill[(cpt/64)&3]);
            for(i=0; i<last; ++i) putchar('\b');
            fflush(stdout);
        }
    } else { // step
        struct timeval curr, temp; ++cpt;
        if(cpt_sec<0 && 0<=cpt_sec+cpt) {
            cpt_sec -= 30;
            gettime(&curr);
            timersub(&curr, &start, &temp);
            if(temp.tv_sec>=15) {
                cpt_sec = cpt/temp.tv_sec;
            }
        }
        if(cpt_sec>=0 && cpt>=cpt_sec) {
            long long t; const int base=100; cpt = 0;
            gettime(&curr);
            timersub(&curr, &start, &temp);
            t = (100*base*count)/total;
            t = printf(" %d.%02d%% (%ds, rem. %ds)",
                (int)t/base, (int)t%base,
                (int)temp.tv_sec,
                (int)(temp.tv_sec*(total-count))/count);
            while(t<last) {
                putchar(' ');
                ++t;
            }
            last = t;
            while(t--) putchar('\b');
            fflush(stdout);
        }
    }
    return -1;
}
// #define progress(X) (void)(X);

/*****************************************************************************/

#define ARRAY(TYPE)                                         \
struct {                                                    \
    size_t        capa;                                     \
    const size_t  cell;                                     \
    size_t        len;                                      \
    TYPE         *tab;                                      \
}

PRIVATE void _ARRAY_DISPOSE(void *_array) {
    ARRAY(void) *array = _array;
    if(array!=NULL) {
        if(array->tab!=NULL) free(array->tab);
        array->tab  = NULL;
        array->capa = 0;
        array->len  = 0;
    }
}

PRIVATE size_t _ARRAY_ENSURE_CAPA(void *_array, size_t n) {
    ARRAY(void) *array = _array;
    if(array!=NULL && array->capa<=n) {
        array->capa = n + 128;
        array->tab = realloc(array->tab, array->capa * array->cell);
        assert(array->tab != NULL);
    }
    return n;
}

#define ARRAY_DECL(TYPE, NAME) ARRAY(TYPE) NAME = {         \
    .capa = 0,                                              \
    .cell = sizeof(TYPE),                                   \
    .len  = 0,                                              \
    .tab  = NULL}

#define ARRAY_DONE(ARRAY)    _ARRAY_DISPOSE(&(ARRAY))

#define ARRAY_REM(ARRAY, INDEX)                             \
    (ARRAY).tab[(INDEX)] = (ARRAY).tab[--(ARRAY).len]

#define ARRAY_ADD(ARRAY, VAL) do {                          \
    _ARRAY_ENSURE_CAPA(&(ARRAY), ++(ARRAY).len);            \
    (ARRAY).tab[(ARRAY).len-1] = (VAL);                     \
} while(0)

#define ARRAY_CPY(DST, SRC) do {                            \
    _ARRAY_ENSURE_CAPA(&(DST), (DST).len = (SRC).len);      \
    /* if only whe could do typeof(x)==typeof(y)... */      \
    if((DST).cell == (SRC).cell) {                          \
        memcpy((DST).tab, (SRC).tab, (SRC).len*(SRC).cell); \
    } else {                                                \
        int i, len = (SRC).len;                             \
        for(i = 0; i<len; ++i) (DST).tab[i] = (SRC).tab[i]; \
    }                                                       \
} while(0)

/*****************************************************************************/

typedef struct {
    integer p;
    integer q;
} rat;

#ifndef NUMBLE
/* converts a double to a rationnal */
PRIVATE void rat_double(rat *r, double f) {
    // https://rosettacode.org/wiki/Convert_decimal_number_to_rational#C

    /*  a: continued fraction coefficients. */
    int64_t a, h[3] = { 0, 1, 0 }, k[3] = { 1, 0, 0 };
    int64_t x, d, n = 1, md = 32767; // md = max denominator
    bool neg = f<0;
    int i;

    if(neg) f = -f;
    while (f != floor(f)) { n <<= 1; f *= 2; }
    d = f;

    /* continued fraction and check denominator each step */
    for (i = 0; i < 64; i++) {
        a = n ? d / n : 0;
        if (i && !a) break;

        x = d; d = n; n = x % n;

        x = a;
        if (k[1] * a + k[0] >= md) {
            x = (md - k[0]) / k[1];
            if (x * 2 >= a || k[1] >= md)
                i = 65;
            else
                break;
        }

        h[2] = x * h[1] + h[0]; h[0] = h[1]; h[1] = h[2];
        k[2] = x * k[1] + k[0]; k[0] = k[1]; k[1] = k[2];
    }
    r->q = k[1];
    r->p = neg ? -h[1] : h[1];
}

PRIVATE bool rat_whole(rat *p) {
    return p->q == 1;
}
#endif

PRIVATE bool rat_integer(rat *r, integer n) {
    r->p = n;
    r->q = 1;
    return true;
}

//* https://rosettacode.org/wiki/Greatest_common_divisor#C */
PRIVATE integer gcd(integer a, integer b) {
    if(a < 0) a = -a;
    if(b < 0) b = -b;
    if(b) while ((a %= b) && (b %= a));
    return a+b;
}

PRIVATE void rat_norm(rat *r, integer p, integer q) {
    integer t = gcd(p,q);
    r->p = p/t;
    r->q = q/t;
}

PRIVATE void rat_add(rat *r, rat *u, rat *v) {
/* define SLOW_RAT if normalisation of rationnal is slow
  (not the case on modern cpus). */
#ifdef SLOW_RAT
    if(rat_whole(u) && rat_whole(v)) rat_integer(r, u->p + v->p); else
#endif
    rat_norm(r, u->p*v->q + v->p*u->q, u->q*v->q);
}

PRIVATE void rat_sub(rat *r, rat *u, rat *v) {
#ifdef SLOW_RAT
    if(rat_whole(u) && rat_whole(v)) rat_integer(r, u->p - v->p); else
#endif
    rat_norm(r, u->p*v->q - v->p*u->q, u->q*v->q);
}

PRIVATE void rat_mul(rat *r, rat *u, rat *v) {
#ifdef SLOW_RAT
    if(rat_whole(u) && rat_whole(v)) rat_integer(r, u->p * v->p); else
#endif
    rat_norm(r, u->p * v->p, u->q * v->q);
}

PRIVATE void rat_div(rat *r, rat *u, rat *v) {
#ifdef SLOW_RAT
    if(rat_whole(u) && rat_whole(v)) rat_norm(r, u->p, v->p); else
#endif
    rat_norm(r, u->p * v->q, u->q * v->p);
}

/*****************************************************************************/

typedef enum {
    MSK0=1,
    MSK1=2,
    MSK2=4,
    MSK3=8,
    MSK4=16,
    MSK5=32,
    MSK6=64,
    MSK7=128,
    MSK8=256,
    MSK9=512,
    MSKadd=1024,
    MSKsub=2048,
    MSKmul=4096,
    MSKdiv=8192,
#if ALLOW_PARENTHESIS
    MSKbra=16384,
    MSKket=32768,
#elif defined(NUMBLE)
    MSKequ=16384,
#endif
    MSKnone=0
} mask;

PRIVATE const mask MSKall = MSKnone
    | MSK0
    | MSK1
    | MSK2
    | MSK3
    | MSK4
    | MSK5
    | MSK6
    | MSK7
    | MSK8
    | MSK9
    | MSKadd
    | MSKsub
    | MSKmul
    | MSKdiv
#if ALLOW_PARENTHESIS
    | MSKbra
    | MSKket
#elif defined(NUMBLE)
    | MSKequ
#endif
    | MSKnone;

PRIVATE mask char_to_mask(char symbol) {
    switch(symbol) {
        case '0': return MSK0;
        case '1': return MSK1;
        case '2': return MSK2;
        case '3': return MSK3;
        case '4': return MSK4;
        case '5': return MSK5;
        case '6': return MSK6;
        case '7': return MSK7;
        case '8': return MSK8;
        case '9': return MSK9;
        case '+': return MSKadd;
        case '-': return MSKsub;
        case '*': return MSKmul;
        case '/': return MSKdiv;
#if ALLOW_PARENTHESIS
        case '(': return MSKbra;
        case ')': return MSKket;
#elif defined(NUMBLE)
        case '=': return MSKequ;
#endif
        default:  return MSKnone;
    }
}

PRIVATE char mask_to_char(mask mask) {
    switch(mask) {
        case MSK0:   return '0';
        case MSK1:   return '1';
        case MSK2:   return '2';
        case MSK3:   return '3';
        case MSK4:   return '4';
        case MSK5:   return '5';
        case MSK6:   return '6';
        case MSK7:   return '7';
        case MSK8:   return '8';
        case MSK9:   return '9';
        case MSKadd: return '+';
        case MSKsub: return '-';
        case MSKmul: return '*';
        case MSKdiv: return '/';
#if ALLOW_PARENTHESIS
        case MSKbra: return '(';
        case MSKket: return ')';
#elif defined(NUMBLE)
        case MSKequ: return '=';
#endif
        default:     return '\0';
    }
}

#ifdef DEBUG
PRIVATE void mask_print(mask msk) {
    while(msk) {
        mask m = msk & -msk;
        putchar(mask_to_char(m));
        msk -= m;
    }
}
#endif

/*****************************************************************************/

// converted from prolog (https://pastebin.com/YV7xRsdg) to C

typedef struct {
    bool set;
    rat val;
} opt_rat;

PRIVATE opt_rat *expression(opt_rat *T, int from, int to);
PRIVATE opt_rat *term(opt_rat *T, int from, int to);
PRIVATE opt_rat *factor(opt_rat *T, int from, int to);
PRIVATE opt_rat *number(opt_rat *T, int from, int to);

PRIVATE char buffer[SIZE];

typedef opt_rat *(*goal)(opt_rat *T, int from, int to);

/*
 * solves T = U op V
 *
 * if T is set, compute U to satisy equation;
 * else (U is set), compute W.
 * Note: V is always set.
 */
PRIVATE opt_rat *solve(opt_rat *T, goal goal, opt_rat *U, int from, int to, char op, opt_rat *V) {
    assert(V->set);

    buffer[to] = op;
    switch(op) {
        case '+':
        if(T->set) {
            rat_sub(&U->val, &T->val, &V->val);
            U->set = true;  goal(U, from, to);
        } else {
            U->set = false; goal(U, from, to);
            rat_add(&T->val, &U->val, &V->val);
            T->set = true;
        }
        break;

        case '=':
        case '-':
        if(T->set) {
            rat_add(&U->val, &T->val, &V->val);
            U->set = true;  goal(U, from, to);
        } else {
            U->set = false; goal(U, from, to);
            rat_sub(&T->val, &U->val, &V->val);
            T->set = true;
        }
        break;

        case '/':
        if(V->val.p == 0) Backtrack();
        if(T->set) {
            rat_mul(&U->val, &T->val, &V->val);
            U->set = true;  goal(U, from, to);
        } else {
            U->set = false; goal(U, from, to);
            rat_div(&T->val, &U->val, &V->val);
            T->set = true;
        }
        break;

        case '*':
        if(!T->set || 0 == (T->val.p | V->val.p)) {
            U->set = false; goal(U, from, to);
            rat_mul(&T->val, &U->val, &V->val);
            T->set = true;
        } else {
            if(V->val.p == 0) Backtrack();
            rat_div(&U->val, &T->val, &V->val);
            U->set = true;  goal(U, from, to);
        }
        break;

        default:
        assert(false);
    }
    return T;
}

PRIVATE opt_rat *expression(opt_rat *T, int from, int to) {
    char op;

    switch(Choice(3)) {
        case 1: // expression ::= term
        return term(T, from, to);

        case 2: // expression ::= expression + term
        op = '+'; break;

        case 3: // expression ::= expression - term
        op = '-'; break;

        default: assert(false); return T;
    }
    {
        int split = from + Choice(to - from - 2);
        opt_rat U, V;  V.set = false;
        return solve(T, expression, &U, from, split, op, term(&V, split+1, to));
    }
}

PRIVATE opt_rat *term(opt_rat *T, int from, int to) {
    char op;
    switch(Choice(3)) {
        case 1: // term ::= factor
        return factor(T, from, to);

        case 2: // term ::= term * factor
        op = '*'; break;

        case 3: // term ::= term / factor
        op = '/' ; break;

        default: assert(false); return T;
    }
    {
        int split = from + Choice(to - from - 2);
        opt_rat U, V;  V.set = false;
        return solve(T, term, &U, from, split, op, factor(&V, split+1, to));
    }
}

PRIVATE opt_rat *factor(opt_rat *T, int from, int to) {
    switch(Choice(1
#if ALLOW_PARENTHESIS
                    +1
#endif
    )) {
        case 1: // factor ::= number
        return number(T, from, to);

        case 2: // factor ::= ( expression )
        if(to-from<3) Backtrack();
        buffer[from] = '(';
        buffer[to-1] = ')';
        return expression(T, from+1, to-1);
    }
    assert(false);
    return T;
}

PRIVATE bool num(int n, int from, int to) {
    int j = to;
    do {
        buffer[--j] = '0' + (n%10);
        n /= 10;
    } while(n && from<j);
    return n==0 && from==j;
}

PRIVATE integer ipow(integer a, int b) {
    integer x = 1, y = a, z = b;
    while(z>0) {
        if(z & 1) x *= y;
        y *= y;
        z >>= 1;
    }
    return x;
}

PRIVATE opt_rat *number(opt_rat *T, int from, int to) {
    if(T->set) {
        if(T->val.q!=1
        || T->val.p<0
        || !num(T->val.p, from, to)) Backtrack();
    } else {
        integer i = ipow(10, to - from - 1);
        rat_integer(&T->val, Choice(i==1 ? 10 : (9*i)) + i - (i==1 ? 2 : 1));
        T->set = true;
        (void)num(T->val.p, from, to);
    }
    return T;
}

/*****************************************************************************/

typedef struct formula {
    mask            used;
    mask            mask[SIZE];
    short int       used_count;
} formula;

PRIVATE ARRAY_DECL(formula *, formulae);

#ifdef _OPENMP
PRIVATE int nthreads = 1;
#endif

PRIVATE void findall(rat *num) {
    opt_rat T;  T.set = true;
    T.val = *num;
#ifdef NUMBLE
    {
        int split = Choice(SIZE - 2);
        opt_rat U, V;  V.set = false;
        solve(&T, expression, &U, 0, split, '=', term(&V, split+1, SIZE));
    }
#else
    expression(&T, 0, SIZE);
#endif

    {
        int i, op = 0;
        for(i=0; i<SIZE; ++i) {
            switch(buffer[i]) {
                case '+': case '-': case '*': case '/': ++op; break;
                default: break;
            }
        }
        if(op > MAX_OP) Backtrack();
    }

    {
        formula *f = calloc(1, sizeof(formula));
        int i;

        assert(f!=NULL);

        for(i=0; i<SIZE; ++i) {
            mask m = char_to_mask(buffer[i]);
            f->mask[i] = m;
            f->used   |= m;
        }
        f->used_count  = popcount(f->used);
        ARRAY_ADD(formulae, f);
    }

#ifdef DEBUG
{
    static int num = 0; int i;
    for(i=0; i<SIZE; ++i) putchar(buffer[i]);
    printf("\t#%d\n", ++num);
}
#else
    progress(INT_MAX);
#endif

    Backtrack();
}

/*****************************************************************************/

typedef struct state {
    mask    mandatory;
#if USE_IMPOSSIBLE
    mask    impossible;
#endif
    mask    possible[SIZE];
} state;

#ifdef DEBUG
PRIVATE void state_print(state *state) {
    int i;
    printf("mandatory: "); mask_print(state->mandatory);
#if USE_IMPOSSIBLE
    printf("\nimpossible: "); mask_print(state->impossible);
#endif
    printf("\npossible:");
    for(i=0; i<SIZE; ++i) {
        printf(" ");
        mask_print(state->possible[i]);
    }
    printf("\n");
}
#endif

PRIVATE void state_relax(state *s) {
    mask possible = MSKnone;
    int i;

    for(i=0; i<SIZE; ++i) possible |= s->possible[i];
    for(i=0; i<SIZE; ++i) {
        mask m = s->possible[i];
        if((m & -m)==m) s->possible[i] = possible;
    }
#ifdef DEBUG
    printf("relaxed state:\n");
    state_print(s);
#endif
}

#define GREEN   0   /* must be 0 */
#define YELLOW  1
#define BLACK   2

PRIVATE void state_init(state *s) {
    int i;

    for(i=0; i<SIZE; ++i) s->possible[i] = MSKall;
    s->mandatory  = MSKnone;
#if USE_IMPOSSIBLE
    s->impossible = MSKnone;
#endif
}

PRIVATE bool state_update(state *st, mask *formula, int colors) {
    mask yellow_ones = MSKnone;
    mask impossible  = MSKnone;
    int i; div_t r;

    // update yellow
    for(r.quot=colors, i=0; i<SIZE; ++i) {
        r = div(r.quot, 3);
        switch(r.rem) {
            case YELLOW: {
                mask m = formula[i];
                st->possible[i] &= ~m;
                st->mandatory   |=  m;
                yellow_ones     |=  m;
            }
        }
    }

    // update green & find impossible ones
    for(r.quot=colors, i=0; i<SIZE; ++i) {
        mask m = formula[i];
        r = div(r.quot, 3);
        switch(r.rem) {
            case GREEN:
                if(st->possible[i] & m) {
                    st->possible[i] = m;
                    st->mandatory |=  m;
#ifdef NUMBLE
                    if(m==MSKequ) impossible |= m;
#endif
                } else return false; // non coherent
            break;
            case BLACK:
                if(MSKnone == (yellow_ones & m)) {
                    impossible |= m;
                }
            break;
        }
    }

    // remove impossible ones
    impossible = ~impossible;
    for(i=0; i<SIZE; ++i) {
        mask m = st->possible[i];
        if((m & -m) != m) {
            st->possible[i] &= impossible;
#if USE_IMPOSSIBLE
        } else {
            st->impossible &= impossible;
#endif
        }
    }

    return true;
}

#if 1
PRIVATE bool state_compatible(state *state, formula *formula) {
    //if((state->mandatory - (state->mandatory & formula->used))
    if(((state->mandatory & formula->used) - state->mandatory)
#if USE_IMPOSSIBLE
        | (state->impossible & formula->used)
#endif
    ) {
        return false;
    } else {
        mask *a = formula->mask, *b = state->possible;
        int i = SIZE;
        while(i && (*a++ & *b++)) --i;
        return i==0;
    }
}
#elif 1
PRIVATE bool state_compatible(state *state, formula *formula) {
    int i;

    if((state->mandatory & formula->used) != state->mandatory) {
        return false;
    }
    for(i=0; i<SIZE; ++i) {
        if(MSKnone == (formula->mask[i] & state->possible[i])) {
            return false;
        }
    }

    return true;
}
#else
PRIVATE bool state_compatible(state *state, formula *formula) {
    int i=SIZE;
    static mask *old;

    if((state->impossible & formula->used) == MSKnone
    && (state->mandatory  & formula->used) == state->mandatory) {
        if(old) {while(--i>=0 && formula->mask[i]==old[i]); ++i;}
        while(--i>=0 && (formula->mask[i] & state->possible[i]));
    }
    if(i<0) {
        old = formula->mask;
        return true;
    } else {
        return false;
    }
}
#endif

#if 1
PRIVATE int state_compatible_count(
    state * const state, const int threshold,
    formula ** const tab, const size_t len) {
    int n = 0, i;
    for(i=len; --i>=0;) {
        if(state_compatible(state, tab[i])) {
            if(++n>threshold) break;
        }
    }
    return n;
}
#else
PRIVATE int state_compatible_count(state *state, int threshold, formula **tab, size_t len) {
    int n = 0, i;
    for(i=len; i; ) switch(i) {
        default:
#define CODE if(state_compatible(state, tab[--i])) ++n
        case 8: CODE; case 7: CODE; case 6: CODE; case 5: CODE;
        case 4: CODE; case 3: CODE; case 2: CODE; case 1: CODE;
        if(n>threshold) goto done;
#undef CODE
    }
    done:
    return n;
}
#endif

/* find the worst number of incompatible states for the
   current candidate */
#ifdef _OPENMP
PRIVATE int find_worst_openmp(state *state, formula *candidate,
    int all_colors, formula **tab, int len, int least_c) {
    int worst = 0;

    #pragma omp parallel shared(worst)
    {
        int color = all_colors + omp_get_thread_num();
        while((color-=nthreads)>=0 && worst<least_c) {
            struct state state2 = *state;
            state_update(&state2, candidate->mask, color);
            int count = state_compatible_count(&state2, least_c, tab, len);
            if(count>worst) {
                #pragma omp critical
                {
                    if(count>worst) worst = count;
                }
            }
        }
    }

    return worst;
}

#endif
PRIVATE int find_worst(state *state, formula *candidate,
    int all_colors, formula **tab, int len, int least_c) {
    int colors;
    int worst;

#ifdef _OPENMP
    if(nthreads>1) return find_worst_openmp(state, candidate,
        all_colors, tab, len, least_c);
#endif

    for(worst=0, colors=all_colors; --colors>=0;) {
        struct state state2 = *state;
        int count;

        state_update(&state2, candidate->mask, colors);

        count = state_compatible_count(&state2, least_c, tab, len);

        if(count > worst) {
            worst = count;
            if(worst > least_c) break;
        }
    }

    return worst;
}

PRIVATE bool least_worst(state *state) {
    const long long use_sampling_threshold =
            MAX_FORMULAE_EXACT*(long long)MAX_FORMULAE_EXACT;
    const long long all_colors = ipow(3,SIZE);
    int             least_c = formulae.len+1;
    formula         *least_f = formulae.tab[0];
    int             rnd_thr = -1, i;

    ARRAY_DECL(formula *, candidates);
    ARRAY_DECL(formula *, samples);

    if(formulae.len == 0) return false;

    if(formulae.len == 1) {
        printf("Only one possible equation.\n");
        for(i=0; i<SIZE; ++i) {
            buffer[i] = mask_to_char(formulae.tab[0]->mask[i]);
        }
        return true;
    }

    printf("Finding least worst equation..."); fflush(stdout);
    ARRAY_CPY(candidates, formulae);
    ARRAY_CPY(samples,    formulae);

    if(candidates.len >= MAX_FORMULAE_EXACT) {
        printf("simpl");
        for(i=0; i<candidates.len;) {
            if(candidates.tab[i]->used_count==SIZE)
                ++i;
            else ARRAY_REM(candidates, i);
        }
        if(candidates.len >= MAX_FORMULAE_EXACT) {
            for(i=0; i<candidates.len;) {
                if((candidates.tab[i]->used & MSK0)==MSKnone)
                    ++i;
                else ARRAY_REM(candidates, i);
            }
        }
        printf("..."); fflush(stdout);
    }

    if(formulae.len*(long long)candidates.len >= use_sampling_threshold) {
        long long t = use_sampling_threshold * RAND_MAX;
        rnd_thr = t/formulae.len/candidates.len;
        t = (rnd_thr*(100*100ll))/RAND_MAX;
        printf("%d.%02d%% sampl...", (int)(t/100), (int)(t%100));
        fflush(stdout);
    }
    progress(-candidates.len);

// #pragma omp parallel for
    for(i=0; i<candidates.len; ++i) {
        formula *candidate = candidates.tab[i];
        int worst;

        /* refesh our sample list from time to time */
        if(rnd_thr>=0 && 0==(i & 7)) {
            int j;
            while(samples.len>0) ARRAY_REM(samples, 0);
            for(j=0; j<formulae.len; ++j) if(j==i || rand()<=rnd_thr) {
                ARRAY_ADD(samples, formulae.tab[j]);
            }
        }

        worst = find_worst(state, candidate,
            all_colors, samples.tab, samples.len, least_c);
        progress(i);

        /* keep the least-worse candidate */
        if(worst<least_c) {
            least_c = worst;
            least_f = candidate;
#ifdef DEBUG
            int  j;
            printf("\n%5d [", worst); fflush(stdout);
            for(j=0; j<SIZE; ++j) putchar(mask_to_char(least_f->mask[j]));
            putchar(']');
            fflush(stdout);
#endif
        }
    }
    ARRAY_DONE(samples);
    ARRAY_DONE(candidates);
    printf("done");
    if((i=progress(0))>1) printf(" (%s%d%s secs)", A_BOLD, i, A_NORM);
    printf("\n");
    for(i=0; i<SIZE; ++i) {
        buffer[i] = mask_to_char(least_f->mask[i]);
    }
#ifdef DEBUG
    printf("least=");
    for(i=0; i<SIZE; ++i) putchar(buffer[i]);
    printf(" (%d / %d)\n", least_c, formulae.len);
#endif
    return least_c>0;
}

/*****************************************************************************/

PRIVATE jmp_buf _env;
PRIVATE void _return(void) {
    longjmp(_env, 1);
}
#define _Backtracking(S) do {                       \
    void (*_Fiasco)(void) = Fiasco;                 \
    Fiasco = _return;                               \
    if(!setjmp(_env)) do Backtracking(S) while(0);  \
    else Fiasco = _Fiasco;                          \
} while(0)

/*****************************************************************************/

PRIVATE void remove_impossible(state *s) {
#ifdef DEBUG
    size_t before = formulae.len;
#endif
    int i;
    for(i = 0; i<formulae.len;) {
        if(state_compatible(s, formulae.tab[i]))
            ++i;
        else ARRAY_REM(formulae, i);
    }
#ifdef DEBUG
    printf("Removed: %d\n", before - formulae.len);
#endif
}

PRIVATE bool play_round(state *state, bool relaxed) {
    int colors;
    while(true) {
        int i, index;
        mask symbs[SIZE];
        struct state back = *state;

        printf(formulae.len>1 ? "Try: %s" : "Sol: %s", A_BOLD);
        for(i=0; i<SIZE; ++i) {
            symbs[i] = char_to_mask(buffer[i]);
            putchar(buffer[i]);
        }
        printf("%s\n", A_NORM);

#if !defined(NUMBLE) || !defined(DEBUG)
        if(formulae.len<=1) {
            fflush(stdout);
            return false;
        }
#endif
        printf("Ans: ");
        fflush(stdout);

        for(colors = i = 0, index = 1; i<SIZE; ) {
            int c; int code = -1;
            switch((c = getchar())) {
                case EOF: exit(0); break;

                case ' ': case '\r': case '\n': break;

                case '!': code = GREEN;  break;
                case '+': code = YELLOW; break;
                case '-': code = BLACK;  break;

                default:
                printf("ERROR, invalid char: %c\ntry:  ", (char)c);
                fflush(stdout); colors = i = 0; index = 1; break;
            }
            if(code>=0) {
                colors += code*index;
                ++i; index *= 3;
            }
        }

        if(0 == colors) return false;

        state_update(state, symbs, colors);
#ifdef DEBUG
        state_print(state);
#endif
        if(0 == state_compatible_count(
                state,        INT_MAX,
                formulae.tab, formulae.len)) {
            printf("ERROR, invalid colors: ");
            for(i=0; i<SIZE; ++i, colors /= 3) {
                switch(colors % 3) {
                    case GREEN:  putchar('!'); break;
                    case YELLOW: putchar('+'); break;
                    case BLACK:  putchar('-'); break;
                }
                buffer[i] = mask_to_char(symbs[i]);
            }
            printf("\n");
            *state = back;
        } else {
            bool ok;
            if(relaxed) {
                state_relax(state);
                for(i = 0; i<formulae.len; ++i) {
                    formula *f = formulae.tab[i];
                    int j;
                    for(j=0; j<SIZE && f->mask[j]==symbs[j]; ++j);
                    if(j == SIZE) {
                        ARRAY_REM(formulae, i);
                        break;
                    }
                }
            } else {
                remove_impossible(state);
            }
            ok = least_worst(state);
            if(relaxed) {
                *state = back;
                state_update(state, symbs, colors);
            }
            if(ok) break;
        }
    }

    return colors!=0;
}

/*****************************************************************************/

#if DO_SORT
PRIVATE int cmp_formula(const void *_a, const void *_b) {
    formula * const *x = _a, * const *y = _b;
    const formula *a = *x, *b = *y;
    int d=0;
    if(d==0) d = b->used_count - a->used_count;
    int i = SIZE; while(d==0 && --i>=0) d =
#if 1
        b->mask[i] - a->mask[i];
#else
        a->mask[i] - b->mask[i];
#endif
    // if((a->used & MSKbra) && !(b->used & MSKbra)) d =  1;
    // if(!(a->used & MSKbra) && (b->used & MSKbra)) d = -1;
    // if(d==0) d = a->used - b->used;
    // int i; for(i=0; d==0 && i<SIZE; ++i) d = (*b)->mask[i] - (*a)->mask[i];
    return d;
}

PRIVATE void sort_formulae(void) {
    // int i;

    qsort(formulae.tab, formulae.len, sizeof(*formulae.tab), cmp_formula);

    // for(i=0; i<formlae.len; ++i) {
        // int j;
        // for(j=0; j<SIZE; ++j) putchar(mask_to_char(formulaetab[i]->mask[j]));
        // putchar('\n');
    // }
}
#endif

#if DO_SHUFFLE
PRIVATE void shuffle_formulae(void) {
    int i = formulae.len;

    while(i>1) {
        int j = rand() % i--;
        formula *t = formulae.tab[i];
        formulae.tab[i] = formulae.tab[j];
        formulae.tab[j] = t;
    }

    // for(i = formulae.len; i--;) {
        // int j;
        // for(j=0; j<SIZE; ++j) putchar(mask_to_char(formulae.tab[i]->mask[j]));
        // putchar('\n');
    // }
}
#endif

/*****************************************************************************/

PRIVATE void title(void) {
    char *TITLE1 = "Helper for ";
    char *TITLE2 = " by Samuel Devulder";
    const int len = strlen(TITLE1) + strlen(URL) + strlen(TITLE2);
    int i;
    putchar('\r');
    for(i=len; --i>=0; putchar('~')){} putchar('\n');
    printf("%s%s%s%s%s\n", TITLE1, A_BOLD, URL, A_NORM, TITLE2);
    for(i=len; --i>=0; putchar('~')){} putchar('\n');
}

/*****************************************************************************/

int main(int argc, char **argv) {
    ARRAY_DECL(formula *, found);
    state state;
    rat target;
    int i = 0;

    srand(time(0));
    setlocale(LC_ALL, "");

    if(isatty(fileno(stdout))) {
        A_BOLD = "\033[1m";
        A_NORM = "\033[0m";
    }

    title();

    if(argc>1) {
#ifdef NUMBLE
        rat_integer(&target, 0);
#else
        rat_double(&target, atof(argv[1]));
#endif
    } else {
        int ignored;
#if defined(_WIN32) || defined(__CYGWIN__)
        ignored = system("cmd /c start " URL);
#elif defined(__linux__) || defined(__unix__)
        ignored = system("xdg-open " URL);
#elif defined(__APPLE__)
        ignored = system("open " URL);
#endif
        (void)ignored;
#ifdef NUMBLE
        rat_integer(&target, 0);
#else
        double x = 0;
        printf("Num? ");
        fflush(stdout);
        while(scanf("%lf", &x)!=1);
        rat_double(&target, x);
#endif
    }

#ifdef _OPENMP
#pragma omp parallel
    {
        #pragma omp single
        nthreads = omp_get_num_threads();
    }
    if(nthreads>1) printf("Using %s%d%s threads.\n", A_BOLD, nthreads, A_NORM);
#endif

#ifdef NUMBLE
    do {
        printf("Finding equations..."); fflush(stdout);
#else
        if(rat_whole(&target))
                printf("Finding equations for %s%d%s...",
                    A_BOLD, target.p, A_NORM);
        else    printf("Finding equations for %s%d/%d%s...",
                    A_BOLD, target.p, target.q, A_NORM);
        fflush(stdout);
#endif

        formulae.len = 0;
        progress(-1); _Backtracking(findall(&target)); i = progress(0);
        printf("done ("); if(i>1) printf("%s%d%s secs, ", A_BOLD, i, A_NORM);
        printf("%s%'u%s found)\n", A_BOLD, formulae.len, A_NORM);
        ARRAY_CPY(found, formulae);

#if DO_SHUFFLE
        shuffle_formulae();
#endif
#if DO_SORT
        sort_formulae();
#endif
        state_init(&state);
#if NUMBLE
        memcpy(buffer, "9*42=378", SIZE);
#else
        least_worst(&state);
#endif
        for(i=1; play_round(&state, i==1); ++i);
        printf("Solved in %s%d%s round%s.\n", A_BOLD, i, A_NORM, i>1?"s":"");
#ifdef NUMBLE
    } while(true);
#endif
    while(found.len) {free(found.tab[0]); ARRAY_REM(found, 0);}
    ARRAY_DONE(found);
    ARRAY_DONE(formulae);
    return 0;
}
