/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output, and Bison version.  */
#define YYBISON 30802

/* Bison version string.  */
#define YYBISON_VERSION "3.8.2"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1




/* First part of user prologue.  */
#line 2 "src/tools/make_func.y"

#include "base/std.h"

#include <cstdlib>
#include <cstdio>
#include <cctype>
#include <cstring>
#include <string>

#define YYDEBUG 1
#define YYERROR_VERBOSE 1

#define IS_ID_CHAR(c) (uisalnum(c) || (c) == '_')

FILE *yyin = 0, *yyout = 0;
int yylex();

char *current_file = nullptr;
int current_line = 0;
void yyerror(const char *str) {
  fprintf(stderr, "%s:%d: %s\n", current_file, current_line, str);
  exit(1);
}

#define MAX_FUNC 2048 /* If we need more than this we're in trouble! */

#define T_VOID 1
#define T_INT 2
#define T_STRING 3
#define T_OBJECT 4
#define T_MAPPING 5
#define T_MIXED 6
#define T_UNKNOWN 7
#define T_FLOAT 8
#define T_FUNCTION 9
#define T_BUFFER 10

extern int num_buff;
extern int op_code, efun_code;
extern char *oper_codes[MAX_FUNC];
extern char *efun_codes[MAX_FUNC];
extern char *efun_names[MAX_FUNC];
extern const char *key[MAX_FUNC], *buf[MAX_FUNC];

extern int arg_types[1000], last_current_type;

const char *ctype(int);
std::string etype(int);

int num_buff = 0;
int op_code, efun_code;
char *oper_codes[MAX_FUNC];
char *efun_codes[MAX_FUNC];
char *efun_names[MAX_FUNC];
const char *key[MAX_FUNC], *buf[MAX_FUNC];

int min_arg = -1, limit_max = 0;

/*
 * arg_types is the types of all arguments. A 0 is used as a delimiter,
 * marking next argument. An argument can have several types.
 */
int arg_types[1000], last_current_type;

/*
 * Store the types of the current efun. They will be copied into the
 * arg_types list if they were not already there (to save memory).
 */
int curr_arg_types[40], curr_arg_type_size;

struct type {
  const char *name;
  int num;
};

type types[] = {
    {"void", T_VOID},
    {"int", T_INT},
    {"string", T_STRING},
    {"object", T_OBJECT},
    {"mapping", T_MAPPING},
    {"mixed", T_MIXED},
    {"unknown", T_UNKNOWN},
    {"float", T_FLOAT},
    {"function", T_FUNCTION},
    {"buffer", T_BUFFER}
};

#define NELEMS(arr) (sizeof arr / sizeof arr[0])

static char *duplicate_string(const std::string &value) {
  auto *result = static_cast<char *>(malloc(value.size() + 1));
  if (!result) {
    yyerror("Out of memory");
  }
  memcpy(result, value.c_str(), value.size() + 1);
  return result;
}

static void uppercase_identifier(std::string &value) {
  for (char &character : value) {
    const auto byte = static_cast<unsigned char>(character);
    if (std::islower(byte)) {
      character = static_cast<char>(std::toupper(byte));
    }
  }
}

#line 180 "src/tools/make_func.autogen.cc"

# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif


/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Token kinds.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    YYEMPTY = -2,
    YYEOF = 0,                     /* "end of file"  */
    YYerror = 256,                 /* error  */
    YYUNDEF = 257,                 /* "invalid token"  */
    ID = 258,                      /* ID  */
    NUM = 259,                     /* NUM  */
    DEFAULT = 260,                 /* DEFAULT  */
    OPERATOR = 261                 /* OPERATOR  */
  };
  typedef enum yytokentype yytoken_kind_t;
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 111 "src/tools/make_func.y"

  int number;
  const char *string;

#line 238 "src/tools/make_func.autogen.cc"

};
typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;


int yyparse (void);



/* Symbol kind.  */
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                      /* "end of file"  */
  YYSYMBOL_YYerror = 1,                    /* error  */
  YYSYMBOL_YYUNDEF = 2,                    /* "invalid token"  */
  YYSYMBOL_ID = 3,                         /* ID  */
  YYSYMBOL_NUM = 4,                        /* NUM  */
  YYSYMBOL_DEFAULT = 5,                    /* DEFAULT  */
  YYSYMBOL_OPERATOR = 6,                   /* OPERATOR  */
  YYSYMBOL_7_ = 7,                         /* ';'  */
  YYSYMBOL_8_ = 8,                         /* ','  */
  YYSYMBOL_9_ = 9,                         /* ':'  */
  YYSYMBOL_10_ = 10,                       /* '('  */
  YYSYMBOL_11_ = 11,                       /* ')'  */
  YYSYMBOL_12_ = 12,                       /* '*'  */
  YYSYMBOL_13_ = 13,                       /* '|'  */
  YYSYMBOL_14_ = 14,                       /* '.'  */
  YYSYMBOL_YYACCEPT = 15,                  /* $accept  */
  YYSYMBOL_specs = 16,                     /* specs  */
  YYSYMBOL_spec = 17,                      /* spec  */
  YYSYMBOL_operator = 18,                  /* operator  */
  YYSYMBOL_op_list = 19,                   /* op_list  */
  YYSYMBOL_op = 20,                        /* op  */
  YYSYMBOL_optional_ID = 21,               /* optional_ID  */
  YYSYMBOL_optional_default = 22,          /* optional_default  */
  YYSYMBOL_func = 23,                      /* func  */
  YYSYMBOL_type = 24,                      /* type  */
  YYSYMBOL_basic = 25,                     /* basic  */
  YYSYMBOL_arg_list = 26,                  /* arg_list  */
  YYSYMBOL_typel2 = 27,                    /* typel2  */
  YYSYMBOL_arg_type = 28,                  /* arg_type  */
  YYSYMBOL_typel = 29                      /* typel  */
};
typedef enum yysymbol_kind_t yysymbol_kind_t;




#ifdef short
# undef short
#endif

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
# include <limits.h> /* INFRINGES ON USER NAME SPACE */
# if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#  define YY_STDINT_H
# endif
#endif

/* Narrow types that promote to a signed type and that can represent a
   signed or unsigned integer of at least N bits.  In tables they can
   save space and decrease cache pressure.  Promoting to a signed type
   helps avoid bugs in integer arithmetic.  */

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

/* Work around bug in HP-UX 11.23, which defines these macros
   incorrectly for preprocessor constants.  This workaround can likely
   be removed in 2023, as HPE has promised support for HP-UX 11.23
   (aka HP-UX 11i v2) only through the end of 2022; see Table 2 of
   <https://h20195.www2.hpe.com/V2/getpdf.aspx/4AA4-7673ENW.pdf>.  */
#ifdef __hpux
# undef UINT_LEAST8_MAX
# undef UINT_LEAST16_MAX
# define UINT_LEAST8_MAX 255
# define UINT_LEAST16_MAX 65535
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif

#ifndef YYPTRDIFF_T
# if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__
#  define YYPTRDIFF_T __PTRDIFF_TYPE__
#  define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
# elif defined PTRDIFF_MAX
#  ifndef ptrdiff_t
#   include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  endif
#  define YYPTRDIFF_T ptrdiff_t
#  define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
# else
#  define YYPTRDIFF_T long
#  define YYPTRDIFF_MAXIMUM LONG_MAX
# endif
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM                                  \
  YY_CAST (YYPTRDIFF_T,                                 \
           (YYPTRDIFF_MAXIMUM < YY_CAST (YYSIZE_T, -1)  \
            ? YYPTRDIFF_MAXIMUM                         \
            : YY_CAST (YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST (YYPTRDIFF_T, sizeof (X))


/* Stored state numbers (used for stacks). */
typedef yytype_int8 yy_state_t;

/* State numbers in computations.  */
typedef int yy_state_fast_t;

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif


#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YY_USE(E) ((void) (E))
#else
# define YY_USE(E) /* empty */
#endif

/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
#if defined __GNUC__ && ! defined __ICC && 406 <= __GNUC__ * 100 + __GNUC_MINOR__
# if __GNUC__ * 100 + __GNUC_MINOR__ < 407
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")
# else
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# endif
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif


#define YY_ASSERT(E) ((void) (0 && (E)))

#if !defined yyoverflow

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* !defined yyoverflow */

#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (YYSIZEOF (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (YYSIZEOF (yy_state_t) + YYSIZEOF (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYPTRDIFF_T yynewbytes;                                         \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * YYSIZEOF (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / YYSIZEOF (*yyptr);                        \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, YY_CAST (YYSIZE_T, (Count)) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYPTRDIFF_T yyi;                      \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  2
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   29

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  15
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  15
/* YYNRULES -- Number of rules.  */
#define YYNRULES  26
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  40

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   261


/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK                     \
   ? YY_CAST (yysymbol_kind_t, yytranslate[YYX])        \
   : YYSYMBOL_YYUNDEF)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_int8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
      10,    11,    12,     2,     8,     2,    14,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     9,     7,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,    13,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6
};

#if YYDEBUG
/* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,   124,   124,   124,   126,   126,   128,   130,   130,   132,
     143,   143,   145,   146,   151,   157,   234,   234,   236,   253,
     254,   258,   263,   269,   277,   278,   279
};
#endif

/** Accessing symbol of state STATE.  */
#define YY_ACCESSING_SYMBOL(State) YY_CAST (yysymbol_kind_t, yystos[State])

#if YYDEBUG || 0
/* The user-facing name of the symbol whose (internal) number is
   YYSYMBOL.  No bounds checking.  */
static const char *yysymbol_name (yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end of file\"", "error", "\"invalid token\"", "ID", "NUM", "DEFAULT",
  "OPERATOR", "';'", "','", "':'", "'('", "')'", "'*'", "'|'", "'.'",
  "$accept", "specs", "spec", "operator", "op_list", "op", "optional_ID",
  "optional_default", "func", "type", "basic", "arg_list", "typel2",
  "arg_type", "typel", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#define YYPACT_NINF (-8)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-1)

#define yytable_value_is_error(Yyn) \
  0

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int8 yypact[] =
{
      -8,     1,    -8,    -8,    -1,    -8,    -8,    -8,     5,    -7,
      -8,     2,    -8,    11,    -8,    -8,    -1,    -8,     6,    -8,
      -3,     3,    -8,    -2,    -8,    -8,     7,     4,    10,    -3,
      12,    18,    -8,     9,    -8,     8,    -8,    -8,    -8,    -8
};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_int8 yydefact[] =
{
       2,     0,     1,    18,     0,     3,     4,     5,     0,    16,
       9,     0,     7,    11,    17,     6,     0,    10,     0,     8,
      19,     0,    23,    12,    20,    24,    22,     0,     0,     0,
       0,     0,    26,     0,    21,     0,    25,    14,    13,    15
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int8 yypgoto[] =
{
      -8,    -8,    -8,    -8,    -8,    13,    -8,    -8,    -8,    21,
      -8,    -8,    -5,    -6,    -8
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int8 yydefgoto[] =
{
       0,     1,     5,     6,    11,    12,    18,    30,     7,    22,
       9,    23,    24,    25,    26
};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int8 yytable[] =
{
       3,     2,    10,    28,     3,    14,    29,     4,    13,    15,
      16,    21,    37,    38,    17,    39,    20,    27,    32,    33,
      31,     3,     8,    35,    34,    36,     0,     0,     0,    19
};

static const yytype_int8 yycheck[] =
{
       3,     0,     3,     5,     3,    12,     8,     6,     3,     7,
       8,    14,     3,     4,     3,     7,    10,    14,    14,     9,
      13,     3,     1,    11,    29,    31,    -1,    -1,    -1,    16
};

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_int8 yystos[] =
{
       0,    16,     0,     3,     6,    17,    18,    23,    24,    25,
       3,    19,    20,     3,    12,     7,     8,     3,    21,    20,
      10,    14,    24,    26,    27,    28,    29,    14,     5,     8,
      22,    13,    14,     9,    27,    11,    28,     3,     4,     7
};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr1[] =
{
       0,    15,    16,    16,    17,    17,    18,    19,    19,    20,
      21,    21,    22,    22,    22,    23,    24,    24,    25,    26,
      26,    26,    27,    28,    29,    29,    29
};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     0,     2,     1,     1,     3,     1,     3,     1,
       1,     0,     0,     3,     3,     8,     1,     2,     1,     0,
       1,     3,     1,     1,     1,     3,     3
};


enum { YYENOMEM = -2 };

#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab
#define YYNOMEM         goto yyexhaustedlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                    \
  do                                                              \
    if (yychar == YYEMPTY)                                        \
      {                                                           \
        yychar = (Token);                                         \
        yylval = (Value);                                         \
        YYPOPSTACK (yylen);                                       \
        yystate = *yyssp;                                         \
        goto yybackup;                                            \
      }                                                           \
    else                                                          \
      {                                                           \
        yyerror (YY_("syntax error: cannot back up")); \
        YYERROR;                                                  \
      }                                                           \
  while (0)

/* Backward compatibility with an undocumented macro.
   Use YYerror or YYUNDEF. */
#define YYERRCODE YYUNDEF


/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)




# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Kind, Value); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo,
                       yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep)
{
  FILE *yyoutput = yyo;
  YY_USE (yyoutput);
  if (!yyvaluep)
    return;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print (FILE *yyo,
                 yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep)
{
  YYFPRINTF (yyo, "%s %s (",
             yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name (yykind));

  yy_symbol_value_print (yyo, yykind, yyvaluep);
  YYFPRINTF (yyo, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yy_state_t *yybottom, yy_state_t *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yy_state_t *yyssp, YYSTYPE *yyvsp,
                 int yyrule)
{
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %d):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       YY_ACCESSING_SYMBOL (+yyssp[yyi + 1 - yynrhs]),
                       &yyvsp[(yyi + 1) - (yynrhs)]);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args) ((void) 0)
# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif






/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg,
            yysymbol_kind_t yykind, YYSTYPE *yyvaluep)
{
  YY_USE (yyvaluep);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/* Lookahead token kind.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;
/* Number of syntax errors so far.  */
int yynerrs;




/*----------.
| yyparse.  |
`----------*/

int
yyparse (void)
{
    yy_state_fast_t yystate = 0;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus = 0;

    /* Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* Their size.  */
    YYPTRDIFF_T yystacksize = YYINITDEPTH;

    /* The state stack: array, bottom, top.  */
    yy_state_t yyssa[YYINITDEPTH];
    yy_state_t *yyss = yyssa;
    yy_state_t *yyssp = yyss;

    /* The semantic value stack: array, bottom, top.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs = yyvsa;
    YYSTYPE *yyvsp = yyvs;

  int yyn;
  /* The return value of yyparse.  */
  int yyresult;
  /* Lookahead symbol kind.  */
  yysymbol_kind_t yytoken = YYSYMBOL_YYEMPTY;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;



#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yychar = YYEMPTY; /* Cause a token to be read.  */

  goto yysetstate;


/*------------------------------------------------------------.
| yynewstate -- push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;


/*--------------------------------------------------------------------.
| yysetstate -- set current state (the top of the stack) to yystate.  |
`--------------------------------------------------------------------*/
yysetstate:
  YYDPRINTF ((stderr, "Entering state %d\n", yystate));
  YY_ASSERT (0 <= yystate && yystate < YYNSTATES);
  YY_IGNORE_USELESS_CAST_BEGIN
  *yyssp = YY_CAST (yy_state_t, yystate);
  YY_IGNORE_USELESS_CAST_END
  YY_STACK_PRINT (yyss, yyssp);

  if (yyss + yystacksize - 1 <= yyssp)
#if !defined yyoverflow && !defined YYSTACK_RELOCATE
    YYNOMEM;
#else
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYPTRDIFF_T yysize = yyssp - yyss + 1;

# if defined yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        yy_state_t *yyss1 = yyss;
        YYSTYPE *yyvs1 = yyvs;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * YYSIZEOF (*yyssp),
                    &yyvs1, yysize * YYSIZEOF (*yyvsp),
                    &yystacksize);
        yyss = yyss1;
        yyvs = yyvs1;
      }
# else /* defined YYSTACK_RELOCATE */
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        YYNOMEM;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yy_state_t *yyss1 = yyss;
        union yyalloc *yyptr =
          YY_CAST (union yyalloc *,
                   YYSTACK_ALLOC (YY_CAST (YYSIZE_T, YYSTACK_BYTES (yystacksize))));
        if (! yyptr)
          YYNOMEM;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YY_IGNORE_USELESS_CAST_BEGIN
      YYDPRINTF ((stderr, "Stack size increased to %ld\n",
                  YY_CAST (long, yystacksize)));
      YY_IGNORE_USELESS_CAST_END

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }
#endif /* !defined yyoverflow && !defined YYSTACK_RELOCATE */


  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;


/*-----------.
| yybackup.  |
`-----------*/
yybackup:
  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either empty, or end-of-input, or a valid lookahead.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token\n"));
      yychar = yylex ();
    }

  if (yychar <= YYEOF)
    {
      yychar = YYEOF;
      yytoken = YYSYMBOL_YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else if (yychar == YYerror)
    {
      /* The scanner already issued an error message, process directly
         to error recovery.  But do not keep the error token as
         lookahead, it is too special and may lead us to an endless
         loop in error recovery. */
      yychar = YYUNDEF;
      yytoken = YYSYMBOL_YYerror;
      goto yyerrlab1;
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);
  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  /* Discard the shifted token.  */
  yychar = YYEMPTY;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
  case 9: /* op: ID  */
#line 132 "src/tools/make_func.y"
       {
  if (op_code >= MAX_FUNC) yyerror("Too many operators");
  std::string f_name = "F_";
  f_name += (yyvsp[0].string);
  uppercase_identifier(f_name);
  oper_codes[op_code] = duplicate_string(f_name);
  free((void *)(yyvsp[0].string));

  op_code++;
}
#line 1266 "src/tools/make_func.autogen.cc"
    break;

  case 11: /* optional_ID: %empty  */
#line 143 "src/tools/make_func.y"
                              { (yyval.string) = ""; }
#line 1272 "src/tools/make_func.autogen.cc"
    break;

  case 12: /* optional_default: %empty  */
#line 145 "src/tools/make_func.y"
                               { (yyval.string) = "DEFAULT_NONE"; }
#line 1278 "src/tools/make_func.autogen.cc"
    break;

  case 13: /* optional_default: DEFAULT ':' NUM  */
#line 146 "src/tools/make_func.y"
                  {
  static std::string default_value;
  default_value = std::to_string((yyvsp[0].number));
  (yyval.string) = default_value.c_str();
}
#line 1288 "src/tools/make_func.autogen.cc"
    break;

  case 14: /* optional_default: DEFAULT ':' ID  */
#line 151 "src/tools/make_func.y"
                 {
  if (strcmp((yyvsp[0].string), "F__THIS_OBJECT")) yyerror("Illegal default");
  (yyval.string) = "DEFAULT_THIS_OBJECT";
  free((void *)(yyvsp[0].string));
}
#line 1298 "src/tools/make_func.autogen.cc"
    break;

  case 15: /* func: type ID optional_ID '(' arg_list optional_default ')' ';'  */
#line 157 "src/tools/make_func.y"
                                                                {
  int i;
  std::string f_name = "F_";
  if (min_arg == -1) min_arg = (yyvsp[-3].number);
  if (min_arg > 127) yyerror("min_arg > 127\n");
  if ((yyvsp[-5].string)[0] == '\0') {
    if (efun_code >= MAX_FUNC) yyerror("Too many efuns");
    f_name += (yyvsp[-6].string);
    uppercase_identifier(f_name);
    efun_codes[efun_code] = duplicate_string(f_name);
    efun_names[efun_code] = duplicate_string(std::string((yyvsp[-6].string)));
    efun_code++;
  } else {
    f_name += (yyvsp[-5].string);
    f_name += " | F_ALIAS_FLAG";
    uppercase_identifier(f_name);
    free((void *)(yyvsp[-5].string));
  }

  for (i = 0; i < last_current_type; i++) {
    int j;
    for (j = 0; j + i < last_current_type && j < curr_arg_type_size; j++) {
      if (curr_arg_types[j] != arg_types[i + j]) break;
    }
    if (j == curr_arg_type_size) break;
  }
  if (i == last_current_type) {
    if (curr_arg_type_size > static_cast<int>(NELEMS(arg_types)) - last_current_type) {
      yyerror("Array 'arg_types' is too small");
    }
    for (int j = 0; j < curr_arg_type_size; j++) {
      arg_types[last_current_type++] = curr_arg_types[j];
    }
  }
#ifndef CAST_CALL_OTHER
  if (!strcmp((yyvsp[-6].string), "call_other")) {
    (yyvsp[-7].number) = T_MIXED;
  }
#endif
  const std::string return_type = (yyvsp[-7].number) != T_VOID ? ctype((yyvsp[-7].number)) : "TYPE_NOVALUE";
  const std::string arg_type0 = etype(0);
  const std::string arg_type1 = etype(1);
  const std::string arg_type2 = etype(2);
  const std::string arg_type3 = etype(3);
  std::string buff = "{\"";
  buff += (yyvsp[-6].string);
  buff += "\",";
  buff += f_name;
  buff += ",0,0,";
  buff += std::to_string(min_arg);
  buff += ",";
  buff += std::to_string(limit_max ? -1 : (yyvsp[-3].number));
  buff += ",";
  buff += return_type;
  buff += ",";
  buff += arg_type0;
  buff += ",";
  buff += arg_type1;
  buff += ",";
  buff += arg_type2;
  buff += ",";
  buff += arg_type3;
  buff += ",";
  buff += std::to_string(i);
  buff += ",";
  buff += (yyvsp[-2].string);
  buff += "},\n";

  if (num_buff >= MAX_FUNC) yyerror("Too many function definitions");
  key[num_buff] = (yyvsp[-6].string);
  buf[num_buff] = duplicate_string(buff);
  num_buff++;
  min_arg = -1;
  limit_max = 0;
  curr_arg_type_size = 0;
}
#line 1379 "src/tools/make_func.autogen.cc"
    break;

  case 17: /* type: basic '*'  */
#line 234 "src/tools/make_func.y"
                        { (yyval.number) = (yyvsp[-1].number) | 0x10000; }
#line 1385 "src/tools/make_func.autogen.cc"
    break;

  case 18: /* basic: ID  */
#line 236 "src/tools/make_func.y"
          {
  int i = NELEMS(types);
  (yyval.number) = 0;
  while (i--) {
    if (strcmp((yyvsp[0].string), types[i].name) == 0) {
      (yyval.number) = types[i].num;
      break;
    }
  }
  if (!(yyval.number)) {
    std::string message = "Invalid type: ";
    message += (yyvsp[0].string);
    yyerror(message.c_str());
  }
  free((void *)(yyvsp[0].string));
}
#line 1406 "src/tools/make_func.autogen.cc"
    break;

  case 19: /* arg_list: %empty  */
#line 253 "src/tools/make_func.y"
                       { (yyval.number) = 0; }
#line 1412 "src/tools/make_func.autogen.cc"
    break;

  case 20: /* arg_list: typel2  */
#line 254 "src/tools/make_func.y"
                  {
  (yyval.number) = 1;
  if ((yyvsp[0].number)) min_arg = 0;
}
#line 1421 "src/tools/make_func.autogen.cc"
    break;

  case 21: /* arg_list: arg_list ',' typel2  */
#line 258 "src/tools/make_func.y"
                               {
  (yyval.number) = (yyvsp[-2].number) + 1;
  if ((yyvsp[0].number)) min_arg = (yyval.number) - 1;
}
#line 1430 "src/tools/make_func.autogen.cc"
    break;

  case 22: /* typel2: typel  */
#line 263 "src/tools/make_func.y"
              {
  (yyval.number) = (yyvsp[0].number);
  curr_arg_types[curr_arg_type_size++] = 0;
  if (curr_arg_type_size == NELEMS(curr_arg_types)) yyerror("Too many arguments");
}
#line 1440 "src/tools/make_func.autogen.cc"
    break;

  case 23: /* arg_type: type  */
#line 269 "src/tools/make_func.y"
               {
  if ((yyvsp[0].number) != T_VOID) {
    curr_arg_types[curr_arg_type_size++] = (yyvsp[0].number);
    if (curr_arg_type_size == NELEMS(curr_arg_types)) yyerror("Too many arguments");
  }
  (yyval.number) = (yyvsp[0].number);
}
#line 1452 "src/tools/make_func.autogen.cc"
    break;

  case 24: /* typel: arg_type  */
#line 277 "src/tools/make_func.y"
                { (yyval.number) = ((yyvsp[0].number) == T_VOID && min_arg == -1); }
#line 1458 "src/tools/make_func.autogen.cc"
    break;

  case 25: /* typel: typel '|' arg_type  */
#line 278 "src/tools/make_func.y"
                     { (yyval.number) = (min_arg == -1 && ((yyvsp[-2].number) || (yyvsp[0].number) == T_VOID)); }
#line 1464 "src/tools/make_func.autogen.cc"
    break;

  case 26: /* typel: '.' '.' '.'  */
#line 279 "src/tools/make_func.y"
              {
  (yyval.number) = min_arg == -1;
  limit_max = 1;
}
#line 1473 "src/tools/make_func.autogen.cc"
    break;


#line 1477 "src/tools/make_func.autogen.cc"

      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", YY_CAST (yysymbol_kind_t, yyr1[yyn]), &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;

  *++yyvsp = yyval;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */
  {
    const int yylhs = yyr1[yyn] - YYNTOKENS;
    const int yyi = yypgoto[yylhs] + *yyssp;
    yystate = (0 <= yyi && yyi <= YYLAST && yycheck[yyi] == *yyssp
               ? yytable[yyi]
               : yydefgoto[yylhs]);
  }

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYSYMBOL_YYEMPTY : YYTRANSLATE (yychar);
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
      yyerror (YY_("syntax error"));
    }

  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= YYEOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == YYEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval);
          yychar = YYEMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:
  /* Pacify compilers when the user code never invokes YYERROR and the
     label yyerrorlab therefore never appears in user code.  */
  if (0)
    YYERROR;
  ++yynerrs;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  /* Pop stack until we find a state that shifts the error token.  */
  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYSYMBOL_YYerror;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYSYMBOL_YYerror)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;


      yydestruct ("Error: popping",
                  YY_ACCESSING_SYMBOL (yystate), yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", YY_ACCESSING_SYMBOL (yyn), yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturnlab;


/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturnlab;


/*-----------------------------------------------------------.
| yyexhaustedlab -- YYNOMEM (memory exhaustion) comes here.  |
`-----------------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturnlab;


/*----------------------------------------------------------.
| yyreturnlab -- parsing is finished, clean up and return.  |
`----------------------------------------------------------*/
yyreturnlab:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  YY_ACCESSING_SYMBOL (+*yyssp), yyvsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif

  return yyresult;
}

#line 284 "src/tools/make_func.y"


const char *ctype(int n) {
  static char buff[100]; /* 100 is such a comfortable size :-) */
  const char *p = (char *)NULL;

  if (n & 0x10000)
    strcpy(buff, "TYPE_MOD_ARRAY|");
  else
      buff[0] = '\0';
    n &= ~0x10000;
    switch (n) {
      case T_FLOAT:
        p = "TYPE_REAL";
        break;
      case T_FUNCTION:
        p = "TYPE_FUNCTION";
        break;
      case T_VOID:
        p = "TYPE_VOID";
        break;
      case T_STRING:
        p = "TYPE_STRING";
        break;
      case T_INT:
        p = "TYPE_NUMBER";
        break;
      case T_OBJECT:
        p = "TYPE_OBJECT";
        break;
      case T_MAPPING:
        p = "TYPE_MAPPING";
        break;
      case T_BUFFER:
        p = "TYPE_BUFFER";
        break;
      case T_MIXED:
        p = "TYPE_ANY";
        break;
      case T_UNKNOWN:
        p = "TYPE_UNKNOWN";
        break;
      default:
        yyerror("Bad type!");
    }
    strcat(buff, p);
    if (strlen(buff) + 1 > sizeof buff) yyerror("Local buffer overwritten in ctype()");
    return buff;
}

const char *etype1(int n) {
  if (n & 0x10000) return "T_ARRAY";
  switch (n) {
    case T_FLOAT:
      return "T_REAL";
    case T_FUNCTION:
      return "T_FUNCTION";
    case T_INT:
      return "T_NUMBER";
    case T_OBJECT:
      return "T_OBJECT";
    case T_MAPPING:
      return "T_MAPPING";
    case T_STRING:
      return "T_STRING";
    case T_BUFFER:
      return "T_BUFFER";
    case T_MIXED:
      return "T_ANY"; /* 0 means any type */
    default:
      yyerror("Illegal type for argument");
  }
  return "What?";
}

std::string etype(int n) {
  int i;
  std::string buff;

  for (i = 0; i < curr_arg_type_size; i++) {
    if (n == 0) break;
    if (curr_arg_types[i] == 0) n--;
  }
  if (i == curr_arg_type_size) return "T_ANY";
  for (; curr_arg_types[i] != 0; i++) {
    if (curr_arg_types[i] == T_VOID) continue;
    if (!buff.empty()) {
      buff += '|';
    }
    buff += etype1(curr_arg_types[i]);
  }
  if (buff.empty()) buff = "T_ANY";
  return buff;
}

int ident(int);

int yylex() {
  int c;

  for (;;) {
    switch (c = getc(yyin)) {
      case ' ':
      case '\t':
      case '\r':
        continue;
      case '\n':
        current_line++;
        continue;
      case '!': {
        char buff[2048];
        if (fgets(buff, sizeof(buff), yyin) == nullptr) {
          buff[0] = '\0';
        }
        fprintf(stderr, "Configuration problem: %s\n", buff);
        exit(-1);
      }
      case '#': {
        // skip to the end of the line
        while ((c = getc(yyin)) != '\n') {
          ;  // empty
        }
        current_line++;
        continue;
      }
      case EOF:
        fclose(yyin);
        return -1;
      default:
        if ((c >= '0' && c <= '9') || c == '-') {
          int v;
          int neg = 0;
          v = 0;
          if (c == '-') {
            neg = 1;
            c = '0';
          }
          do {
            v = v * 10 + (c - '0');
            c = getc(yyin);
          } while (c >= '0' && c <= '9');
          ungetc(c, yyin);
          yylval.number = (neg ? -v : v);
          return NUM;
        }
        if (IS_ID_CHAR(c)) return ident(c);
        return c;
    }
  }
}

int ident(int c) {
  char buff[100];
  int len;

  for (len = 0; IS_ID_CHAR(c); c = getc(yyin)) {
    buff[len++] = c;
    if (len == sizeof buff - 1) {
      yyerror("Too long indentifier");
      break;
    }
  }
  (void)ungetc(c, yyin);
  buff[len] = '\0';
  if (!strcmp(buff, "default")) return DEFAULT;
  if (!strcmp(buff, "operator")) return OPERATOR;

  yylval.string = (char *)malloc(strlen(buff) + 1);
  strcpy((char *)yylval.string, buff);
  return ID;
}

void make_efun_tables();

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: make_func <spec_file>\n");
    return 1;
  }
  if ((yyin = fopen(argv[1], "r")) == NULL) {
    fprintf(stderr, "make_func: unable to open input file: %s\n", argv[1]);
    return 1;
  }
  current_file = new char[strlen(argv[1]) + 1];
  strcpy(current_file, argv[1]);
  current_line = 1;

  num_buff = op_code = efun_code = 0;
  yyparse();

  make_efun_tables();

  exit(0);
}

#define EFUN_CC "efuns.autogen.cc"
#define EFUN_H "efuns.autogen.h"

static const char HEADER[] =
    "// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"
    "// !!! This file is automatically generated by make_func. !!!\n"
    "// !!! do not make any manual changes to this file.       !!!\n"
    "// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n\n";

static const char GUARD_TOP[] =
    "#ifndef __EFUNS_HH__\n#define __EFUNS_HH__\n\n";

static const char GUARD_BOTTOM[] = "\n\n#endif  // __EFUNS_HH__\n";

void make_efun_tables() {
  FILE* f = fopen(EFUN_CC, "w");

  if (!f) {
    fprintf(stderr, "make_func: unable to open %s\n", EFUN_CC);
    exit(-1);
  }

  fprintf(f, HEADER);
  fprintf(f, "#include \"base/std.h\"\n\n");
  fprintf(f, "#include \"" EFUN_H "\"\n");

  fprintf(f, "\n/* Operator names */\n");
  fprintf(f, "const char* operator_names[] = {\n");
  fprintf(f, "    \"INVALID OPCODE\", // 0\n");
  for (int i = 0; i < op_code; i++) {
    fprintf(f, "    \"%s\", // %s: %d \n ", oper_codes[i], oper_codes[i], i+1);
  }
  fprintf(f, "};\n");

  fprintf(f, "\n// EFUN tables\n\n");
  fprintf(f, "func_t efun_table[] = {\n");
  for (int i = 0; i < efun_code; i++) {
    fprintf(f, "    f_%s,\n", efun_names[i]);
  }
  fprintf(f, "};\n");

  fprintf(f, "struct keyword_t predefs[] = {\n");
  for (int i = 0; i < num_buff; i++) {
    fprintf(f, "    %s", buf[i]);
  }
  fprintf(f, "};\n");

  fprintf(f, "const int size_of_predefs = sizeof(predefs) / sizeof(predefs[0]);\n");

  // EFUN arg types

  fprintf(f, "int efun_arg_types[] = {\n");
  for (int i = 0; i < last_current_type; i++) {
    if (arg_types[i] == 0) {
      fprintf(f, "0,\n");
    } else {
      fprintf(f, "%s, ", ctype(arg_types[i]));
    }
  }
  fprintf(f, "};\n");

  fprintf(f, "int efun_arg_etypes[] = {\n");
  for (int i = 0; i < last_current_type; i++) {
    if (arg_types[i] == 0) {
      fprintf(f, "0,\n");
    } else {
      fprintf(f, "%s, ", etype1(arg_types[i]));
    }
  }
  fprintf(f, "};\n");

  fclose(f);

  // Now generating Header
  f = fopen(EFUN_H, "w");
  if (!f) {
    fprintf(stderr, "make_func: unable to open %s\n", EFUN_H);
    exit(-1);
  }

  fprintf(f, HEADER);
  fprintf(f, GUARD_TOP);
  fprintf(f, "\n/* operators */\n\n");

  int total_code = 0;
  for (int i = 0; i < op_code; i++) {
    fprintf(f, "#define %-30s %d\n", oper_codes[i], i + 1);
    total_code++;
  }

  fprintf(f, "\n/* efuns */\n");

  int efun_base = op_code + 1;
  fprintf(f, "#define EFUN_BASE %d\n\n", efun_base);
  for (int i = 0; i < efun_code; i++) {
    fprintf(f, "#define %-30s %d\n", efun_codes[i], i + efun_base);
    total_code++;
  }

  fprintf(f, "\n#define NUM_OPCODES %d\n", total_code + 1);

  fprintf(f, "\n/* EFUN prototypes */\n\n");
  for (int i = 0; i < efun_code; i++) {
    fprintf(f, "void f_%s (void);\n", efun_names[i]);
  }
  fprintf(f, "typedef void (*func_t) (void);\n\n");
  fprintf(f, "extern func_t efun_table[];\n");
  fprintf(f, "extern const char* operator_names[];\n");

  /* Now sort the main_list */
  for (int i = 0; i < num_buff; i++) {
    for (int j = 0; j < i; j++)
      if (strcmp(key[i], key[j]) < 0) {
        const char *tmp;
        tmp = key[i];
        key[i] = key[j];
        key[j] = tmp;
        tmp = buf[i];
        buf[i] = buf[j];
        buf[j] = tmp;
      }
  }

  fprintf(f, "\n // EFUN arguments list (used by compiler.cc) \n\n");

  fprintf(f, "#include \"compiler/internal/keyword.h\"\n");
  fprintf(f, "#include \"compiler/internal/compiler.h\"\n");
  fprintf(f, "#include \"vm/internal/base/svalue.h\"\n");
  fprintf(f, "\n\n");
  fprintf(f, "#define DEFAULT_NONE 0xff\n");
  fprintf(f, "#define DEFAULT_THIS_OBJECT 0xfe\n");
  fprintf(f, "// indicates that the instruction is only used at compile time\n");
  fprintf(f, "#define F_ALIAS_FLAG 1024\n");

  fprintf(f, "\n\n");

  fprintf(f, "extern struct keyword_t predefs[];\n");
  fprintf(f, "extern const int size_of_predefs;\n");

  fprintf(f, GUARD_BOTTOM);

  fclose(f);
}
