#include "token.h"

SF_API TokenSM *
sf_statem_token_new (char *r)
{
  TokenSM *smt = SFMALLOC (sizeof (*smt));
  smt->raw = r;
  smt->rl = strlen (r);
  smt->vl = 0;
  smt->vc = SF_TOKEN_STATEM_VALS_CAP;
  smt->vals = SFMALLOC (smt->vc * sizeof (*smt->vals));

  return smt;
}

int
is_keyword (char *r)
{
  static const char *KEYWORDS[]
      = { "if",    "else",     "for",     "while",  "in",  "to",
          "step",  "class",    "fun",     "return", "or",  "and",
          "not",   "repeat",   "import",  "as",     "try", "catch",
          "break", "continue", "extends", NULL };

  for (size_t i = 0; KEYWORDS[i] != NULL; i++)
    if (!strcmp (KEYWORDS[i], r))
      return 1;

  return 0;
}

int
is_bool (char *r)
{
  return !strcmp (r, "true") || !strcmp (r, "false");
}

void
makeop (char *op)
{
  switch (*op)
    {
    case '+':
    case '-':
    case '*':
    case '/':
    case '%':
    case '=':
      {
        if (op[1] == '=')
          op[2] = '\0';
        else
          op[1] = '\0';
      }
      break;

    case '!':
      {
        if (op[1] == '=')
          op[2] = '\0';
        else
          op[1] = '\0';
      }
      break;

    case '<':
    case '>':
      {
        if (op[1] == op[0])
          {
            if (op[2] != '=')
              op[2] = '\0';
          }
        else
          op[1] = '\0';
      }
      break;

    default:
      op[1] = '\0';
      break;
    }
}

void
__tokensm_add (TokenSM *smt, token_t tok)
{
  if (smt->vl >= smt->vc)
    sf_token_resize (smt);

  smt->vals[smt->vl++] = tok;
}

static char *__strbfr = NULL;
SF_API token_t *
sf_token_next (TokenSM *smt)
{
  token_t res;
  res.type = -1;

  int add_tok = 1;

  char d = *smt->raw++;

  if (d == '\0')
    {
      res.type = TOK_EOF;
      __tokensm_add (smt, res);
      return &smt->vals[smt->vl - 1];
    }

  if ((d >= 'A' && d <= 'Z') || (d >= 'a' && d <= 'z') || d == '_')
    {
      size_t idl = 0;
      __strbfr[idl++] = d;

      d = *smt->raw++;
      while (d)
        {
          if ((d >= 'A' && d <= 'Z') || (d >= 'a' && d <= 'z')
              || (d >= '0' && d <= '9') || d == '_')
            {
              __strbfr[idl++] = d;
              d = *smt->raw++;
            }
          else
            break;
        }

      __strbfr[idl] = '\0';

      if (is_keyword (__strbfr))
        {
          res.type = TOK_KEYWORD;
          res.v.t_keyword.value = SFSTRDUP (__strbfr);
        }
      else if (is_bool (__strbfr))
        {
          res.type = TOK_BOOL;
          res.v.t_bool.value = *__strbfr == 't';
        }
      else if (!strcmp (__strbfr, "none"))
        {
          res.type = TOK_NONE;
        }
      else
        {
          res.type = TOK_IDENTIFIER;
          res.v.t_identifier.value = SFSTRDUP (__strbfr);
        }

      smt->raw--;
    }

  else if (d >= '0' && d <= '9')
    {
      size_t idl = 0;
      __strbfr[idl++] = d;

      d = *smt->raw++;
      int saw_dot = 0;

      while (d)
        {
          if ((d >= '0' && d <= '9') || d == '.')
            {
              if (d == '.')
                {
                  if (saw_dot)
                    break;
                  saw_dot = 1;
                }

              __strbfr[idl++] = d;
              d = *smt->raw++;
            }
          else
            break;
        }

      __strbfr[idl] = '\0';

      if (saw_dot)
        {
          res.type = TOK_FLOAT;
          res.v.t_float.value = atof (__strbfr);
        }
      else
        {
          res.type = TOK_INTEGER;
          res.v.t_integer.value = atoi (__strbfr);
        }

      smt->raw--;
    }

  else if (strstr ("~!%^&*()-+=[]{}\\|:/.,<>", (char *)(char[]){ d, '\0' })
           != NULL)
    {
      char op[4] = { 0 };
      op[0] = d;

      if (*smt->raw)
        op[1] = *(smt->raw);

      if (*(smt->raw + 1))
        op[2] = *(smt->raw + 1);

      makeop (op);

      smt->raw += strlen (op) - 1;

      res.type = TOK_OPERATOR;
      res.v.t_operator.value = SFSTRDUP (op);
    }
  else if (d == '\'' || d == '\"')
    {
      char qt = d;

      size_t idl = 0;

      d = *smt->raw++;

      while (d)
        {
          if (d == '\\' && *smt->raw)
            {
              char next = *smt->raw++;
              switch (next)
                {
                case 'n':
                  __strbfr[idl++] = '\n';
                  break;
                case 't':
                  __strbfr[idl++] = '\t';
                  break;
                case 'r':
                  __strbfr[idl++] = '\r';
                  break;
                case '\\':
                  __strbfr[idl++] = '\\';
                  break;
                case '\'':
                  __strbfr[idl++] = '\'';
                  break;
                case '\"':
                  __strbfr[idl++] = '\"';
                  break;
                case '0':
                  __strbfr[idl++] = '\0';
                  break;
                default:
                  __strbfr[idl++] = '\\';
                  __strbfr[idl++] = next;
                  break;
                }
              d = *smt->raw++;
            }
          else if (d == qt)
            break;
          else
            {
              __strbfr[idl++] = d;
              d = *smt->raw++;
            }
        }

      __strbfr[idl] = '\0';

      res.type = TOK_STRING;
      res.v.t_string.value = SFSTRDUP (__strbfr);
    }
  else if (d == '\n')
    {
      res.type = TOK_NEWLINE;
    }
  else if (d == ' ')
    {
      if (smt->vl)
        {
          if (smt->vals[smt->vl - 1].type == TOK_NEWLINE && *smt->raw == ' ')
            {
              res.type = TOK_SPACE;
              res.v.space.v = 2;
              smt->raw++;
            }

          if (smt->vals[smt->vl - 1].type == TOK_SPACE)
            {
              smt->vals[smt->vl - 1].v.space.v += 1;
              add_tok = 0;
            }
        }
    }
  else
    return &smt->vals[smt->vl - 1];

  if (add_tok && res.type != -1)
    __tokensm_add (smt, res);

  return &smt->vals[smt->vl - 1];
}

SF_API void
sf_token_gen (TokenSM *smt)
{
  if (__strbfr == NULL)
    __strbfr = SFMALLOC (1024 * sizeof (char));

  token_t *n = sf_token_next (smt);

  while (n->type != TOK_EOF)
    n = sf_token_next (smt);

  SFFREE (__strbfr);
  __strbfr = NULL;
}

SF_API void
sf_statem_token_free (TokenSM *smt)
{
  SFFREE (smt->vals);
  SFFREE (smt);
}

SF_API void
sf_token_resize (TokenSM *smt)
{
  if (smt->vl >= smt->vc)
    {
      smt->vc += SF_TOKEN_STATEM_VALS_CAP;
      smt->vals = SFREALLOC (smt->vals, smt->vc * sizeof (*smt->vals));
    }
}

SF_API void
sf_token_print (token_t tok)
{
  switch (tok.type)
    {
    case TOK_NEWLINE:
      printf ("newline\n");
      break;
    case TOK_SPACE:
      printf ("Space: %zu\n", tok.v.space.v);
      break;
    case TOK_INTEGER:
      printf ("Integer: %d\n", tok.v.t_integer.value);
      break;
    case TOK_FLOAT:
      printf ("Float: %f\n", tok.v.t_float.value);
      break;
    case TOK_STRING:
      printf ("String: \"%s\"\n", tok.v.t_string.value);
      break;
    case TOK_OPERATOR:
      printf ("Operator: '%s'\n", tok.v.t_operator.value);
      break;
    case TOK_IDENTIFIER:
      printf ("Identifier: %s\n", tok.v.t_identifier.value);
      break;
    case TOK_KEYWORD:
      printf ("Keyword: %s\n", tok.v.t_keyword.value);
      break;
    case TOK_EOF:
      printf ("EOF\n");
      break;
    default:
      printf ("UNKNOWN(%d)\n", tok.type);
      break;
    }
}

SF_API void
sf_tokensm_print (TokenSM *smt)
{
  for (size_t i = 0; i < smt->vl; i++)
    {
      sf_token_print (smt->vals[i]);
    }
}