#if !defined(TOKEN_H)
#define TOKEN_H

#include "header.h"
#include "malloc.h"

enum TokenType
{
  TOK_NEWLINE = 0,
  TOK_SPACE = 1,
  TOK_INTEGER = 2,
  TOK_FLOAT = 3,
  TOK_STRING = 4,
  TOK_OPERATOR = 5,
  TOK_IDENTIFIER = 6,
  TOK_KEYWORD = 7,
  TOK_BOOL = 8,
  TOK_NONE = 9,
  TOK_EOF
};

typedef struct
{
  int type;

  union
  {
    struct
    {
      size_t v;
    } space;

    struct
    {
      int value;
    } t_integer;

    struct
    {
      float value;
    } t_float;

    struct
    {
      const char *value;
    } t_string;

    struct
    {
      const char *value;
    } t_operator;

    struct
    {
      const char *value;
    } t_identifier;

    struct
    {
      const char *value;
    } t_keyword;

    struct
    {
      int value;
    } t_bool;

  } v;

} token_t;

typedef struct
{
  char *raw;
  size_t rl;

  token_t *vals;
  size_t vl; /* size */
  size_t vc; /* capacity */

} TokenSM;

#define SF_TOKEN_STATEM_VALS_CAP (64)

#if defined(__cplusplus)
extern "C"
{
#endif // __cplusplus

  SF_API TokenSM *sf_statem_token_new (char *_Raw);
  SF_API token_t *sf_token_next (TokenSM *);
  SF_API void sf_token_gen (TokenSM *);
  SF_API void sf_token_resize (TokenSM *);
  SF_API void sf_statem_token_free (TokenSM *);
  SF_API void sf_token_print (token_t);
  SF_API void sf_tokensm_print (TokenSM *);

#if defined(__cplusplus)
}
#endif // __cplusplus

#endif // TOKEN_H
