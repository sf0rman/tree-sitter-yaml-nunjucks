#include "tree_sitter/array.h"
#include "tree_sitter/parser.h"

#define _str(x) #x
#define _file(x) _str(schema.x.c)

#ifndef YAML_SCHEMA
#define YAML_SCHEMA core
#endif

#include _file(YAML_SCHEMA)

// clang-format off

typedef enum {
    END_OF_FILE,

    S_DIR_YML_BGN,  R_DIR_YML_VER,
    S_DIR_TAG_BGN,  R_DIR_TAG_HDL,  R_DIR_TAG_PFX,
    S_DIR_RSV_BGN,  R_DIR_RSV_PRM,
    S_DRS_END,
    S_DOC_END,
    R_BLK_SEQ_BGN,  BR_BLK_SEQ_BGN, B_BLK_SEQ_BGN,
    R_BLK_KEY_BGN,  BR_BLK_KEY_BGN, B_BLK_KEY_BGN,
    R_BLK_VAL_BGN,  BR_BLK_VAL_BGN, B_BLK_VAL_BGN,
    R_BLK_IMP_BGN,
    R_BLK_LIT_BGN,  BR_BLK_LIT_BGN,
    R_BLK_FLD_BGN,  BR_BLK_FLD_BGN,
    BR_BLK_STR_CTN,
    R_FLW_SEQ_BGN,  BR_FLW_SEQ_BGN, B_FLW_SEQ_BGN,
    R_FLW_SEQ_END,  BR_FLW_SEQ_END, B_FLW_SEQ_END,
    R_FLW_MAP_BGN,  BR_FLW_MAP_BGN, B_FLW_MAP_BGN,
    R_FLW_MAP_END,  BR_FLW_MAP_END, B_FLW_MAP_END,
    R_FLW_SEP_BGN,  BR_FLW_SEP_BGN,
    R_FLW_KEY_BGN,  BR_FLW_KEY_BGN,
    R_FLW_JSV_BGN,  BR_FLW_JSV_BGN,
    R_FLW_NJV_BGN,  BR_FLW_NJV_BGN,
    R_DQT_STR_BGN,  BR_DQT_STR_BGN, B_DQT_STR_BGN,
    R_DQT_STR_CTN,  BR_DQT_STR_CTN,
    R_DQT_ESC_NWL,  BR_DQT_ESC_NWL,
    R_DQT_ESC_SEQ,  BR_DQT_ESC_SEQ,
    R_DQT_STR_END,  BR_DQT_STR_END,
    R_SQT_STR_BGN,  BR_SQT_STR_BGN, B_SQT_STR_BGN,
    R_SQT_STR_CTN,  BR_SQT_STR_CTN,
    R_SQT_ESC_SQT,  BR_SQT_ESC_SQT,
    R_SQT_STR_END,  BR_SQT_STR_END,

    R_SGL_PLN_NUL_BLK, BR_SGL_PLN_NUL_BLK, B_SGL_PLN_NUL_BLK, R_SGL_PLN_NUL_FLW, BR_SGL_PLN_NUL_FLW,
    R_SGL_PLN_BOL_BLK, BR_SGL_PLN_BOL_BLK, B_SGL_PLN_BOL_BLK, R_SGL_PLN_BOL_FLW, BR_SGL_PLN_BOL_FLW,
    R_SGL_PLN_INT_BLK, BR_SGL_PLN_INT_BLK, B_SGL_PLN_INT_BLK, R_SGL_PLN_INT_FLW, BR_SGL_PLN_INT_FLW,
    R_SGL_PLN_FLT_BLK, BR_SGL_PLN_FLT_BLK, B_SGL_PLN_FLT_BLK, R_SGL_PLN_FLT_FLW, BR_SGL_PLN_FLT_FLW,
    R_SGL_PLN_STR_BLK, BR_SGL_PLN_STR_BLK, B_SGL_PLN_STR_BLK, R_SGL_PLN_STR_FLW, BR_SGL_PLN_STR_FLW,

    R_MTL_PLN_STR_BLK,  BR_MTL_PLN_STR_BLK,
    R_MTL_PLN_STR_FLW,  BR_MTL_PLN_STR_FLW,

    R_TAG,     BR_TAG,     B_TAG,
    R_ACR_BGN, BR_ACR_BGN, B_ACR_BGN, R_ACR_CTN,
    R_ALS_BGN, BR_ALS_BGN, B_ALS_BGN, R_ALS_CTN,

    BL,
    COMMENT,

    // Nunjucks template delimiters and content (must match externals order in grammar.js)
    NJK_INTERP_BGN,  // {{
    NJK_INTERP_END,  // }}
    NJK_STMT_BGN,    // {%
    NJK_STMT_END,    // %}
    NJK_CMT_BGN,     // {#
    NJK_CMT_END,     // #}
    NJK_CONTENT,     // raw content inside {{ }} or {% %} (everything before the closer)
    NJK_KEYWORD,     // leading identifier word inside {% %} (e.g. "if", "for", "endfor")
    NJK_IDENTIFIER,  // any other identifier inside {{ }} or {% %} (variable/filter names, "in", ...)

    ERR_REC,
} TokenType;

// clang-format on

#define SCN_SUCC 1
#define SCN_STOP 0
#define SCN_FAIL (-1)

#define IND_ROT 'r'
#define IND_MAP 'm'
#define IND_SEQ 'q'
#define IND_STR 's'

#define RET_SYM(RESULT_SYMBOL)                                                                                         \
    {                                                                                                                  \
        flush(scanner);                                                                                                \
        lexer->result_symbol = RESULT_SYMBOL;                                                                          \
        return true;                                                                                                   \
    }

#define POP_IND()                                                                                                      \
    {                                                                                                                  \
        /* incorrect status caused by error recovering */                                                              \
        if (scanner->ind_typ_stk.size == 1) {                                                                          \
            return false;                                                                                              \
        }                                                                                                              \
        pop_ind(scanner);                                                                                              \
    }

#define PUSH_IND(TYP, LEN) push_ind(scanner, TYP, LEN)

#define PUSH_BGN_IND(TYP)                                                                                              \
    {                                                                                                                  \
        if (has_tab_ind)                                                                                               \
            return false;                                                                                              \
        push_ind(scanner, TYP, bgn_col);                                                                               \
    }

#define MAY_PUSH_IMP_IND(TYP)                                                                                          \
    {                                                                                                                  \
        if (cur_ind != scanner->blk_imp_col) {                                                                         \
            if (scanner->blk_imp_tab)                                                                                  \
                return false;                                                                                          \
            push_ind(scanner, IND_MAP, scanner->blk_imp_col);                                                          \
        }                                                                                                              \
    }

#define MAY_PUSH_SPC_SEQ_IND()                                                                                         \
    {                                                                                                                  \
        if (cur_ind_typ == IND_MAP) {                                                                                  \
            push_ind(scanner, IND_SEQ, bgn_col);                                                                       \
        }                                                                                                              \
    }

#define MAY_UPD_IMP_COL()                                                                                              \
    {                                                                                                                  \
        if (scanner->blk_imp_row != bgn_row) {                                                                         \
            scanner->blk_imp_row = bgn_row;                                                                            \
            scanner->blk_imp_col = bgn_col;                                                                            \
            scanner->blk_imp_tab = has_tab_ind;                                                                        \
        }                                                                                                              \
    }

#define SGL_PLN_SYM(POS, CTX)                                                                                          \
    (scanner->rlt_sch == RS_NULL    ? POS##_SGL_PLN_NUL_##CTX                                                          \
     : scanner->rlt_sch == RS_BOOL  ? POS##_SGL_PLN_BOL_##CTX                                                          \
     : scanner->rlt_sch == RS_INT   ? POS##_SGL_PLN_INT_##CTX                                                          \
     : scanner->rlt_sch == RS_FLOAT ? POS##_SGL_PLN_FLT_##CTX                                                          \
                                    : POS##_SGL_PLN_STR_##CTX)

typedef struct {
    int16_t row;
    int16_t col;
    int16_t blk_imp_row;
    int16_t blk_imp_col;
    int16_t blk_imp_tab;
    int16_t njk_depth; // >0 when inside {{ }} or {% %} or {# #}
    Array(int16_t) ind_typ_stk;
    Array(int16_t) ind_len_stk;

    // temp
    int16_t end_row;
    int16_t end_col;
    int16_t cur_row;
    int16_t cur_col;
    int32_t cur_chr;
    int8_t sch_stt;
    ResultSchema rlt_sch;
} Scanner;

static unsigned serialize(Scanner *scanner, char *buffer) {
    size_t size = 0;
    *(int16_t *)&buffer[size] = scanner->row;
    size += sizeof(int16_t);
    *(int16_t *)&buffer[size] = scanner->col;
    size += sizeof(int16_t);
    *(int16_t *)&buffer[size] = scanner->blk_imp_row;
    size += sizeof(int16_t);
    *(int16_t *)&buffer[size] = scanner->blk_imp_col;
    size += sizeof(int16_t);
    *(int16_t *)&buffer[size] = scanner->blk_imp_tab;
    size += sizeof(int16_t);
    *(int16_t *)&buffer[size] = scanner->njk_depth;
    size += sizeof(int16_t);
    int16_t *typ_itr = scanner->ind_typ_stk.contents + 1;
    int16_t *typ_end = scanner->ind_typ_stk.contents + scanner->ind_typ_stk.size;
    int16_t *len_itr = scanner->ind_len_stk.contents + 1;
    for (; typ_itr != typ_end && size < TREE_SITTER_SERIALIZATION_BUFFER_SIZE; ++typ_itr, ++len_itr) {
        *(int16_t *)&buffer[size] = *typ_itr;
        size += sizeof(int16_t);
        *(int16_t *)&buffer[size] = *len_itr;
        size += sizeof(int16_t);
    }
    return size;
}

static void deserialize(Scanner *scanner, const char *buffer, unsigned length) {
    scanner->row = 0;
    scanner->col = 0;
    scanner->blk_imp_row = -1;
    scanner->blk_imp_col = -1;
    scanner->blk_imp_tab = 0;
    scanner->njk_depth = 0;
    array_delete(&scanner->ind_typ_stk);
    array_push(&scanner->ind_typ_stk, IND_ROT);
    array_delete(&scanner->ind_len_stk);
    array_push(&scanner->ind_len_stk, -1);
    if (length > 0) {
        size_t size = 0;
        scanner->row = *(int16_t *)&buffer[size];
        size += sizeof(int16_t);
        scanner->col = *(int16_t *)&buffer[size];
        size += sizeof(int16_t);
        scanner->blk_imp_row = *(int16_t *)&buffer[size];
        size += sizeof(int16_t);
        scanner->blk_imp_col = *(int16_t *)&buffer[size];
        size += sizeof(int16_t);
        scanner->blk_imp_tab = *(int16_t *)&buffer[size];
        size += sizeof(int16_t);
        if (size < length) {
            scanner->njk_depth = *(int16_t *)&buffer[size];
            size += sizeof(int16_t);
        }
        while (size < length) {
            array_push(&scanner->ind_typ_stk, *(int16_t *)&buffer[size]);
            size += sizeof(int16_t);
            array_push(&scanner->ind_len_stk, *(int16_t *)&buffer[size]);
            size += sizeof(int16_t);
        }
        assert(size == length);
    }
}

static inline void adv(Scanner *scanner, TSLexer *lexer) {
    scanner->cur_col++;
    scanner->cur_chr = lexer->lookahead;
    lexer->advance(lexer, false);
}

static inline void adv_nwl(Scanner *scanner, TSLexer *lexer) {
    scanner->cur_row++;
    scanner->cur_col = 0;
    scanner->cur_chr = lexer->lookahead;
    lexer->advance(lexer, false);
}

static inline void skp(Scanner *scanner, TSLexer *lexer) {
    scanner->cur_col++;
    scanner->cur_chr = lexer->lookahead;
    lexer->advance(lexer, true);
}

static inline void skp_nwl(Scanner *scanner, TSLexer *lexer) {
    scanner->cur_row++;
    scanner->cur_col = 0;
    scanner->cur_chr = lexer->lookahead;
    lexer->advance(lexer, true);
}

static inline void mrk_end(Scanner *scanner, TSLexer *lexer) {
    scanner->end_row = scanner->cur_row;
    scanner->end_col = scanner->cur_col;
    lexer->mark_end(lexer);
}

static inline void init(Scanner *scanner) {
    scanner->cur_row = scanner->row;
    scanner->cur_col = scanner->col;
    scanner->cur_chr = 0;
    scanner->sch_stt = 0;
    scanner->rlt_sch = RS_STR;
}

static inline void flush(Scanner *scanner) {
    scanner->row = scanner->end_row;
    scanner->col = scanner->end_col;
}

static inline void pop_ind(Scanner *scanner) {
    array_pop(&scanner->ind_len_stk);
    array_pop(&scanner->ind_typ_stk);
}

static inline void push_ind(Scanner *scanner, int16_t typ, int16_t len) {
    array_push(&scanner->ind_len_stk, len);
    array_push(&scanner->ind_typ_stk, typ);
}

static inline bool is_wsp(int32_t c) { return c == ' ' || c == '\t'; }

static inline bool is_nwl(int32_t c) { return c == '\r' || c == '\n'; }

static inline bool is_wht(int32_t c) { return is_wsp(c) || is_nwl(c) || c == 0; }

static inline bool is_ns_dec_digit(int32_t c) { return c >= '0' && c <= '9'; }

static inline bool is_ns_hex_digit(int32_t c) {
    return is_ns_dec_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static inline bool is_ns_word_char(int32_t c) {
    return c == '-' || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static inline bool is_njk_ident_start(int32_t c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static inline bool is_njk_ident_cont(int32_t c) {
    return is_njk_ident_start(c) || (c >= '0' && c <= '9');
}

static inline bool is_nb_json(int32_t c) { return c == 0x09 || (c >= 0x20 && c <= 0x10ffff); }

static inline bool is_nb_double_char(int32_t c) { return is_nb_json(c) && c != '\\' && c != '"'; }

static inline bool is_nb_single_char(int32_t c) { return is_nb_json(c) && c != '\''; }

static inline bool is_ns_char(int32_t c) {
    return (c >= 0x21 && c <= 0x7e) || c == 0x85 || (c >= 0xa0 && c <= 0xd7ff) || (c >= 0xe000 && c <= 0xfefe) ||
           (c >= 0xff00 && c <= 0xfffd) || (c >= 0x10000 && c <= 0x10ffff);
}

static inline bool is_c_indicator(int32_t c) {
    return c == '-' || c == '?' || c == ':' || c == ',' || c == '[' || c == ']' || c == '{' || c == '}' || c == '#' ||
           c == '&' || c == '*' || c == '!' || c == '|' || c == '>' || c == '\'' || c == '"' || c == '%' || c == '@' ||
           c == '`';
}

static inline bool is_c_flow_indicator(int32_t c) { return c == ',' || c == '[' || c == ']' || c == '{' || c == '}'; }

static inline bool is_plain_safe_in_block(int32_t c) { return is_ns_char(c); }

static inline bool is_plain_safe_in_flow(int32_t c) { return is_ns_char(c) && !is_c_flow_indicator(c); }

static inline bool is_ns_uri_char(int32_t c) {
    return is_ns_word_char(c) || c == '#' || c == ';' || c == '/' || c == '?' || c == ':' || c == '@' || c == '&' ||
           c == '=' || c == '+' || c == '$' || c == ',' || c == '_' || c == '.' || c == '!' || c == '~' || c == '*' ||
           c == '\'' || c == '(' || c == ')' || c == '[' || c == ']';
}

static inline bool is_ns_tag_char(int32_t c) {
    return is_ns_word_char(c) || c == '#' || c == ';' || c == '/' || c == '?' || c == ':' || c == '@' || c == '&' ||
           c == '=' || c == '+' || c == '$' || c == '_' || c == '.' || c == '~' || c == '*' || c == '\'' || c == '(' ||
           c == ')';
}

static inline bool is_ns_anchor_char(int32_t c) { return is_ns_char(c) && !is_c_flow_indicator(c); }

// Peek one char ahead (advance without mrk_end) and return it.
// Safe to call when the caller will either emit a token or return false/break.
static inline int32_t njk_peek(TSLexer *lexer) {
    lexer->advance(lexer, false);
    return lexer->lookahead;
}

// Absorb a Nunjucks whitespace-control '-' immediately following an opening
// delimiter ('{{-', '{%-', '{#-') into the same token, unconditionally consuming
// it. Called right before mrk_end()/RET_SYM() at an opener site, so there is
// nothing to back out of if no '-' is present.
static inline void njk_absorb_dash(Scanner *scanner, TSLexer *lexer) {
    if (lexer->lookahead == '-') {
        adv(scanner, lexer);
    }
}

static char scn_uri_esc(Scanner *scanner, TSLexer *lexer) {
    if (lexer->lookahead != '%') {
        return SCN_STOP;
    }
    mrk_end(scanner, lexer);
    adv(scanner, lexer);
    if (!is_ns_hex_digit(lexer->lookahead)) {
        return SCN_FAIL;
    }
    adv(scanner, lexer);
    if (!is_ns_hex_digit(lexer->lookahead)) {
        return SCN_FAIL;
    }
    adv(scanner, lexer);
    return SCN_SUCC;
}

static char scn_ns_uri_char(Scanner *scanner, TSLexer *lexer) {
    if (is_ns_uri_char(lexer->lookahead)) {
        adv(scanner, lexer);
        return SCN_SUCC;
    }
    return scn_uri_esc(scanner, lexer);
}

static char scn_ns_tag_char(Scanner *scanner, TSLexer *lexer) {
    if (is_ns_tag_char(lexer->lookahead)) {
        adv(scanner, lexer);
        return SCN_SUCC;
    }
    return scn_uri_esc(scanner, lexer);
}

static bool scn_dir_bgn(Scanner *scanner, TSLexer *lexer) {
    adv(scanner, lexer);
    if (lexer->lookahead == 'Y') {
        adv(scanner, lexer);
        if (lexer->lookahead == 'A') {
            adv(scanner, lexer);
            if (lexer->lookahead == 'M') {
                adv(scanner, lexer);
                if (lexer->lookahead == 'L') {
                    adv(scanner, lexer);
                    if (is_wht(lexer->lookahead)) {
                        mrk_end(scanner, lexer);
                        RET_SYM(S_DIR_YML_BGN);
                    }
                }
            }
        }
    } else if (lexer->lookahead == 'T') {
        adv(scanner, lexer);
        if (lexer->lookahead == 'A') {
            adv(scanner, lexer);
            if (lexer->lookahead == 'G') {
                adv(scanner, lexer);
                if (is_wht(lexer->lookahead)) {
                    mrk_end(scanner, lexer);
                    RET_SYM(S_DIR_TAG_BGN);
                }
            }
        }
    }
    for (;;) {
        if (!is_ns_char(lexer->lookahead)) {
            break;
        }
        adv(scanner, lexer);
    }
    if (scanner->cur_col > 1 && is_wht(lexer->lookahead)) {
        mrk_end(scanner, lexer);
        RET_SYM(S_DIR_RSV_BGN);
    }
    return false;
}

static bool scn_dir_yml_ver(Scanner *scanner, TSLexer *lexer, TSSymbol result_symbol) {
    uint16_t n1 = 0;
    uint16_t n2 = 0;
    while (is_ns_dec_digit(lexer->lookahead)) {
        adv(scanner, lexer);
        n1++;
    }
    if (lexer->lookahead != '.') {
        return false;
    }
    adv(scanner, lexer);
    while (is_ns_dec_digit(lexer->lookahead)) {
        adv(scanner, lexer);
        n2++;
    }
    if (n1 == 0 || n2 == 0) {
        return false;
    }
    mrk_end(scanner, lexer);
    RET_SYM(result_symbol);
}

static bool scn_tag_hdl_tal(Scanner *scanner, TSLexer *lexer) {
    if (lexer->lookahead == '!') {
        adv(scanner, lexer);
        return true;
    }
    uint16_t n = 0;
    while (is_ns_word_char(lexer->lookahead)) {
        adv(scanner, lexer);
        n++;
    }
    if (n == 0) {
        return true;
    }
    if (lexer->lookahead == '!') {
        adv(scanner, lexer);
        return true;
    }
    return false;
}

static bool scn_dir_tag_hdl(Scanner *scanner, TSLexer *lexer, TSSymbol result_symbol) {
    if (lexer->lookahead == '!') {
        adv(scanner, lexer);
        if (scn_tag_hdl_tal(scanner, lexer)) {
            mrk_end(scanner, lexer);
            RET_SYM(result_symbol);
        }
    }
    return false;
}

static bool scn_dir_tag_pfx(Scanner *scanner, TSLexer *lexer, TSSymbol result_symbol) {
    if (lexer->lookahead == '!') {
        adv(scanner, lexer);
    } else if (scn_ns_tag_char(scanner, lexer) == SCN_SUCC) {
        ;
    } else {
        return false;
    }
    for (;;) {
        switch (scn_ns_uri_char(scanner, lexer)) {
            case SCN_STOP:
                mrk_end(scanner, lexer);
            case SCN_FAIL:
                RET_SYM(result_symbol);
            default:
                break;
        }
    }
}

static bool scn_dir_rsv_prm(Scanner *scanner, TSLexer *lexer, TSSymbol result_symbol) {
    if (!is_ns_char(lexer->lookahead)) {
        return false;
    }
    adv(scanner, lexer);
    while (is_ns_char(lexer->lookahead)) {
        adv(scanner, lexer);
    }
    mrk_end(scanner, lexer);
    RET_SYM(result_symbol);
}

static bool scn_tag(Scanner *scanner, TSLexer *lexer, TSSymbol result_symbol) {
    if (lexer->lookahead != '!') {
        return false;
    }
    adv(scanner, lexer);
    if (is_wht(lexer->lookahead)) {
        mrk_end(scanner, lexer);
        RET_SYM(result_symbol);
    }
    if (lexer->lookahead == '<') {
        adv(scanner, lexer);
        if (scn_ns_uri_char(scanner, lexer) != SCN_SUCC) {
            return false;
        }
        for (;;) {
            switch (scn_ns_uri_char(scanner, lexer)) {
                case SCN_STOP:
                    if (lexer->lookahead == '>') {
                        adv(scanner, lexer);
                        mrk_end(scanner, lexer);
                        RET_SYM(result_symbol);
                    }
                case SCN_FAIL:
                    return false;
                default:
                    break;
            }
        }
    } else {
        if (scn_tag_hdl_tal(scanner, lexer) && scn_ns_tag_char(scanner, lexer) != SCN_SUCC) {
            return false;
        }
        for (;;) {
            switch (scn_ns_tag_char(scanner, lexer)) {
                case SCN_STOP:
                    mrk_end(scanner, lexer);
                case SCN_FAIL:
                    RET_SYM(result_symbol);
                default:
                    break;
            }
        }
    }
    return false;
}

static bool scn_acr_bgn(Scanner *scanner, TSLexer *lexer, TSSymbol result_symbol) {
    if (lexer->lookahead != '&') {
        return false;
    }
    adv(scanner, lexer);
    if (!is_ns_anchor_char(lexer->lookahead)) {
        return false;
    }
    mrk_end(scanner, lexer);
    RET_SYM(result_symbol);
}

static bool scn_acr_ctn(Scanner *scanner, TSLexer *lexer, TSSymbol result_symbol) {
    while (is_ns_anchor_char(lexer->lookahead)) {
        adv(scanner, lexer);
    }
    mrk_end(scanner, lexer);
    RET_SYM(result_symbol);
}

static bool scn_als_bgn(Scanner *scanner, TSLexer *lexer, TSSymbol result_symbol) {
    if (lexer->lookahead != '*') {
        return false;
    }
    adv(scanner, lexer);
    if (!is_ns_anchor_char(lexer->lookahead)) {
        return false;
    }
    mrk_end(scanner, lexer);
    RET_SYM(result_symbol);
}

static bool scn_als_ctn(Scanner *scanner, TSLexer *lexer, TSSymbol result_symbol) {
    while (is_ns_anchor_char(lexer->lookahead)) {
        adv(scanner, lexer);
    }
    mrk_end(scanner, lexer);
    RET_SYM(result_symbol);
}

static bool scn_dqt_esc_seq(Scanner *scanner, TSLexer *lexer, TSSymbol result_symbol) {
    uint16_t i;
    switch (lexer->lookahead) {
        case '0':
        case 'a':
        case 'b':
        case 't':
        case '\t':
        case 'n':
        case 'v':
        case 'r':
        case 'e':
        case 'f':
        case ' ':
        case '"':
        case '/':
        case '\\':
        case 'N':
        case '_':
        case 'L':
        case 'P':
            adv(scanner, lexer);
            break;
        case 'U':
            adv(scanner, lexer);
            for (i = 0; i < 8; i++) {
                if (is_ns_hex_digit(lexer->lookahead)) {
                    adv(scanner, lexer);
                } else {
                    return false;
                }
            }
            break;
        case 'u':
            adv(scanner, lexer);
            for (i = 0; i < 4; i++) {
                if (is_ns_hex_digit(lexer->lookahead)) {
                    adv(scanner, lexer);
                } else {
                    return false;
                }
            }
            break;
        case 'x':
            adv(scanner, lexer);
            for (i = 0; i < 2; i++) {
                if (is_ns_hex_digit(lexer->lookahead)) {
                    adv(scanner, lexer);
                } else {
                    return false;
                }
            }
            break;
        default:
            return false;
    }
    mrk_end(scanner, lexer);
    RET_SYM(result_symbol);
}

static bool scn_drs_doc_end(Scanner *scanner, TSLexer *lexer) {
    if (lexer->lookahead != '-' && lexer->lookahead != '.') {
        return false;
    }
    int32_t delimeter = lexer->lookahead;
    adv(scanner, lexer);
    if (lexer->lookahead == delimeter) {
        adv(scanner, lexer);
        if (lexer->lookahead == delimeter) {
            adv(scanner, lexer);
            if (is_wht(lexer->lookahead)) {
                return true;
            }
        }
    }
    mrk_end(scanner, lexer);
    return false;
}

static bool scn_dqt_str_cnt(Scanner *scanner, TSLexer *lexer, TSSymbol result_symbol, const bool *valid_symbols) {
    if (!is_nb_double_char(lexer->lookahead)) {
        return false;
    }
    // If the string content starts immediately with '{{', let the interpolation scanner
    // handle it (return false so the '{' dispatch at the call site fires instead). A lone
    // '{' here (not '{{') is handled by that same '{' dispatch — see njk_resume_dqt_cnt —
    // because confirming the second char would require an irreversible lexer->advance(),
    // and we must not consume anything before returning false from this entry check.
    if (lexer->lookahead == '{' && valid_symbols[NJK_INTERP_BGN]) {
        return false;
    }
    if (scanner->cur_col == 0 && scn_drs_doc_end(scanner, lexer)) {
        mrk_end(scanner, lexer);
        RET_SYM(scanner->cur_chr == '-' ? S_DRS_END : S_DOC_END);
    } else {
        adv(scanner, lexer);
    }
    while (is_nb_double_char(lexer->lookahead)) {
        // Stop before '{{' so the interpolation can be tokenized separately.
        if (lexer->lookahead == '{' && valid_symbols[NJK_INTERP_BGN]) {
            mrk_end(scanner, lexer);              // mark end BEFORE the first '{'
            adv(scanner, lexer);                  // advance past first '{' to peek second
            if (lexer->lookahead == '{') {
                // Confirmed '{{' — return content token ending before the first '{'.
                // (do NOT call mrk_end again; the mark is already set before '{')
                RET_SYM(result_symbol);
            }
            // Only one '{' — not an interpolation; include it and continue.
            mrk_end(scanner, lexer);
            continue;
        }
        adv(scanner, lexer);
    }
    mrk_end(scanner, lexer);
    RET_SYM(result_symbol);
}

static bool scn_sqt_str_cnt(Scanner *scanner, TSLexer *lexer, TSSymbol result_symbol, const bool *valid_symbols) {
    if (!is_nb_single_char(lexer->lookahead)) {
        return false;
    }
    // Same '{{'-break logic as double-quote (see scn_dqt_str_cnt for why a lone '{'
    // is handled by the caller's '{' dispatch instead of being confirmed here).
    if (lexer->lookahead == '{' && valid_symbols[NJK_INTERP_BGN]) {
        return false;
    }
    if (scanner->cur_col == 0 && scn_drs_doc_end(scanner, lexer)) {
        mrk_end(scanner, lexer);
        RET_SYM(scanner->cur_chr == '-' ? S_DRS_END : S_DOC_END);
    } else {
        adv(scanner, lexer);
    }
    while (is_nb_single_char(lexer->lookahead)) {
        // Stop before '{{' so the interpolation can be tokenized separately.
        if (lexer->lookahead == '{' && valid_symbols[NJK_INTERP_BGN]) {
            mrk_end(scanner, lexer);              // mark end BEFORE the first '{'
            adv(scanner, lexer);                  // advance past first '{' to peek second
            if (lexer->lookahead == '{') {
                // Confirmed '{{' — return content token ending before the first '{'.
                RET_SYM(result_symbol);
            }
            // Only one '{' — not an interpolation; include it and continue.
            mrk_end(scanner, lexer);
            continue;
        }
        adv(scanner, lexer);
    }
    mrk_end(scanner, lexer);
    RET_SYM(result_symbol);
}

// Resume double/single-quoted string content scanning after the caller's main '{'
// dispatch has already peeked past a lone '{' (confirmed NOT to start a real '{{'
// interpolation) via njk_peek. The caller must have called mrk_end() right before
// that peek so the emitted token includes the lone '{'; lexer->lookahead here is
// therefore already the character immediately after it. Mirrors the tail of
// scn_dqt_str_cnt/scn_sqt_str_cnt — this exists separately because that second
// character cannot be un-peeked, so this path can never itself return false.
static bool njk_resume_dqt_cnt(Scanner *scanner, TSLexer *lexer, TSSymbol result_symbol,
                                const bool *valid_symbols) {
    while (is_nb_double_char(lexer->lookahead)) {
        if (lexer->lookahead == '{' && valid_symbols[NJK_INTERP_BGN]) {
            mrk_end(scanner, lexer);
            adv(scanner, lexer);
            if (lexer->lookahead == '{') {
                RET_SYM(result_symbol);
            }
            mrk_end(scanner, lexer);
            continue;
        }
        adv(scanner, lexer);
    }
    mrk_end(scanner, lexer);
    RET_SYM(result_symbol);
}

static bool njk_resume_sqt_cnt(Scanner *scanner, TSLexer *lexer, TSSymbol result_symbol,
                                const bool *valid_symbols) {
    while (is_nb_single_char(lexer->lookahead)) {
        if (lexer->lookahead == '{' && valid_symbols[NJK_INTERP_BGN]) {
            mrk_end(scanner, lexer);
            adv(scanner, lexer);
            if (lexer->lookahead == '{') {
                RET_SYM(result_symbol);
            }
            mrk_end(scanner, lexer);
            continue;
        }
        adv(scanner, lexer);
    }
    mrk_end(scanner, lexer);
    RET_SYM(result_symbol);
}

static bool scn_blk_str_bgn(Scanner *scanner, TSLexer *lexer, TSSymbol result_symbol) {
    if (lexer->lookahead != '|' && lexer->lookahead != '>') {
        return false;
    }
    adv(scanner, lexer);
    int16_t cur_ind = *array_back(&scanner->ind_len_stk);
    int16_t ind = -1;
    if (lexer->lookahead >= '1' && lexer->lookahead <= '9') {
        ind = lexer->lookahead - '1';
        adv(scanner, lexer);
        if (lexer->lookahead == '+' || lexer->lookahead == '-') {
            adv(scanner, lexer);
        }
    } else if (lexer->lookahead == '+' || lexer->lookahead == '-') {
        adv(scanner, lexer);
        if (lexer->lookahead >= '1' && lexer->lookahead <= '9') {
            ind = lexer->lookahead - '1';
            adv(scanner, lexer);
        }
    }
    if (!is_wht(lexer->lookahead)) {
        return false;
    }
    mrk_end(scanner, lexer);
    if (ind != -1) {
        ind += cur_ind;
    } else {
        ind = cur_ind;
        while (is_wsp(lexer->lookahead)) {
            adv(scanner, lexer);
        }
        if (lexer->lookahead == '#') {
            adv(scanner, lexer);
            while (!is_nwl(lexer->lookahead) && lexer->lookahead != 0) {
                adv(scanner, lexer);
            }
        }
        if (is_nwl(lexer->lookahead)) {
            adv_nwl(scanner, lexer);
        }
        while (lexer->lookahead != 0) {
            if (lexer->lookahead == ' ') {
                adv(scanner, lexer);
            } else if (is_nwl(lexer->lookahead)) {
                if (scanner->cur_col - 1 < ind) {
                    break;
                }
                ind = scanner->cur_col - 1;
                adv_nwl(scanner, lexer);
            } else {
                if (scanner->cur_col - 1 > ind) {
                    ind = scanner->cur_col - 1;
                }
                break;
            }
        }
    }
    PUSH_IND(IND_STR, ind);
    RET_SYM(result_symbol);
}

static bool scn_blk_str_cnt(Scanner *scanner, TSLexer *lexer, TSSymbol result_symbol) {
    if (!is_ns_char(lexer->lookahead)) {
        return false;
    }
    if (scanner->cur_col == 0 && scn_drs_doc_end(scanner, lexer)) {
        POP_IND();
        RET_SYM(BL);
    } else {
        adv(scanner, lexer);
    }
    mrk_end(scanner, lexer);
    for (;;) {
        if (is_ns_char(lexer->lookahead)) {
            adv(scanner, lexer);
            while (is_ns_char(lexer->lookahead)) {
                adv(scanner, lexer);
            }
            mrk_end(scanner, lexer);
        }
        if (is_wsp(lexer->lookahead)) {
            adv(scanner, lexer);
            while (is_wsp(lexer->lookahead)) {
                adv(scanner, lexer);
            }
        } else {
            break;
        }
    }
    RET_SYM(result_symbol);
}

static char scn_pln_cnt(Scanner *scanner, TSLexer *lexer, bool (*is_plain_safe)(int32_t)) {
    bool is_cur_wsp = is_wsp(scanner->cur_chr);
    bool is_cur_saf = is_plain_safe(scanner->cur_chr);
    bool is_lka_wsp = is_wsp(lexer->lookahead);
    bool is_lka_saf = is_plain_safe(lexer->lookahead);
    if (is_lka_saf || is_lka_wsp) {
        for (;;) {
            if (is_lka_saf && lexer->lookahead != '#' && lexer->lookahead != ':') {
                adv(scanner, lexer);
                mrk_end(scanner, lexer);
                scanner->sch_stt = adv_sch_stt(scanner->sch_stt, scanner->cur_chr, &scanner->rlt_sch);
            } else if (is_cur_saf && lexer->lookahead == '#') {
                adv(scanner, lexer);
                mrk_end(scanner, lexer);
                scanner->sch_stt = adv_sch_stt(scanner->sch_stt, scanner->cur_chr, &scanner->rlt_sch);
            } else if (is_lka_wsp) {
                adv(scanner, lexer);
                scanner->sch_stt = adv_sch_stt(scanner->sch_stt, scanner->cur_chr, &scanner->rlt_sch);
            } else if (lexer->lookahead == ':') {
                adv(scanner, lexer); // check later
            } else {
                break;
            }

            is_cur_wsp = is_lka_wsp;
            is_cur_saf = is_lka_saf;
            is_lka_wsp = is_wsp(lexer->lookahead);
            is_lka_saf = is_plain_safe(lexer->lookahead);

            if (scanner->cur_chr == ':') {
                if (is_lka_saf) {
                    mrk_end(scanner, lexer);
                    scanner->sch_stt = adv_sch_stt(scanner->sch_stt, scanner->cur_chr, &scanner->rlt_sch);
                } else {
                    return SCN_FAIL;
                }
            }
        }
    } else {
        return SCN_STOP;
    }
    return SCN_SUCC;
}

static bool scan(Scanner *scanner, TSLexer *lexer, const bool *valid_symbols) {
    init(scanner);
    mrk_end(scanner, lexer);

    // Disable YAML comment scanning when inside quoted scalars or nunjucks constructs
    bool allow_comment = !(valid_symbols[R_DQT_STR_CTN] || valid_symbols[BR_DQT_STR_CTN] ||
                           valid_symbols[R_SQT_STR_CTN] || valid_symbols[BR_SQT_STR_CTN] ||
                           valid_symbols[NJK_INTERP_END] || valid_symbols[NJK_STMT_END] ||
                           valid_symbols[NJK_CMT_END]);
    int16_t *ind_ptr = scanner->ind_len_stk.contents + scanner->ind_len_stk.size - 1;
    int16_t *ind_end = scanner->ind_len_stk.contents - 1;
    int16_t cur_ind = *ind_ptr--;
    int16_t prt_ind = ind_ptr == ind_end ? -1 : *ind_ptr;
    int16_t cur_ind_typ = *array_back(&scanner->ind_typ_stk);

    bool is_njk_inner = scanner->njk_depth > 0 &&
                        (valid_symbols[NJK_INTERP_END] || valid_symbols[NJK_STMT_END] || valid_symbols[NJK_CMT_END]);

    // When inside a nunjucks construct ({{ }}, {% %}, {# #}):
    // Emit NJK_CONTENT (raw text until the closing delimiter), then the closing delimiter.
    // This avoids all the tree-sitter internal-vs-external lexer contention for expression
    // syntax tokens (identifiers, operators, etc.).
    if (is_njk_inner) {
        // Keyword phase: emit the first identifier word inside {% %} as NJK_KEYWORD.
        // Only valid immediately after {% (before any NJK_CONTENT has been emitted),
        // and only for the statement context (not {{ }} interpolation or {# #} comment).
        if (valid_symbols[NJK_KEYWORD] && valid_symbols[NJK_STMT_END]) {
            // Skip leading whitespace (not counted in the token).
            while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
                skp(scanner, lexer);
            }
            // If the next char is an identifier start, emit the word as NJK_KEYWORD.
            int32_t c = lexer->lookahead;
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
                mrk_end(scanner, lexer);  // mark start of keyword token
                while (true) {
                    adv(scanner, lexer);
                    mrk_end(scanner, lexer);
                    c = lexer->lookahead;
                    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                          (c >= '0' && c <= '9') || c == '_')) {
                        break;
                    }
                }
                RET_SYM(NJK_KEYWORD);
            }
            // Not an identifier start — fall through to NJK_CONTENT.
        }

        // Identifier phase: variable names, filter names, mid-statement words (in, and,
        // or, is, ...) — anywhere in the expression, not just right after {% (unlike
        // NJK_KEYWORD above, which is specifically the leading word of a statement tag).
        // highlights.scm can still special-case specific identifier text via #any-of?.
        if (valid_symbols[NJK_IDENTIFIER] && is_njk_ident_start(lexer->lookahead)) {
            mrk_end(scanner, lexer);  // mark start of identifier token
            do {
                adv(scanner, lexer);
                mrk_end(scanner, lexer);
            } while (is_njk_ident_cont(lexer->lookahead));
            RET_SYM(NJK_IDENTIFIER);
        }

        if (valid_symbols[NJK_CONTENT]) {
            // Consume everything until the two-char closing delimiter.
            bool has_content = false;
            while (lexer->lookahead != 0) {
                int32_t c = lexer->lookahead;
                // Stop before an identifier so it can be tokenized separately.
                if (valid_symbols[NJK_IDENTIFIER] && is_njk_ident_start(c)) {
                    break;
                }
                if (c == '}' || c == '%' || c == '#') {
                    // Could be the start of a two-char closer — a single occurrence
                    // of the char is not enough (e.g. the '%' in "x % 2", or the
                    // lone '}' in "{a: 1}"); peek the next char before deciding.
                    adv(scanner, lexer);
                    bool is_real_closer =
                        lexer->lookahead == '}' &&
                        ((c == '}' && valid_symbols[NJK_INTERP_END]) ||
                         (c == '%' && valid_symbols[NJK_STMT_END]) ||
                         (c == '#' && valid_symbols[NJK_CMT_END]));
                    if (is_real_closer) {
                        if (!has_content) {
                            // Nothing to emit as NJK_CONTENT (repeat1 means this loop can
                            // run again with zero characters before the closer). We've
                            // already committed the 2-char peek via adv(), so "return
                            // false" here would leave the lexer one char into the closer
                            // with no token emitted — finish the closer ourselves instead.
                            adv(scanner, lexer);
                            mrk_end(scanner, lexer);
                            if (scanner->njk_depth > 0) scanner->njk_depth--;
                            RET_SYM(c == '}' ? NJK_INTERP_END : c == '%' ? NJK_STMT_END : NJK_CMT_END);
                        }
                        // Found the real closer '}}' / '%}' / '#}'. mrk_end was not
                        // called for this char, so the content token still ends right
                        // before it; the closer itself is picked up on the next call.
                        break;
                    }
                    // False alarm — both peeked characters are ordinary content.
                    mrk_end(scanner, lexer);
                    has_content = true;
                    continue;
                }
                if (c == '-') {
                    // Whitespace control: could be the start of '-%}' / '-}}' / '-#}'.
                    // Confirm all three characters before excluding the '-' from content.
                    adv(scanner, lexer);
                    int32_t c2 = lexer->lookahead;
                    bool maybe_closer = (c2 == '%' && valid_symbols[NJK_STMT_END]) ||
                                        (c2 == '}' && valid_symbols[NJK_INTERP_END]) ||
                                        (c2 == '#' && valid_symbols[NJK_CMT_END]);
                    if (maybe_closer) {
                        adv(scanner, lexer);
                        if (lexer->lookahead == '}') {
                            if (!has_content) {
                                // Same reasoning as the '}'/'%'/'#' case above: finish the
                                // closer here rather than returning false mid-peek.
                                adv(scanner, lexer);
                                mrk_end(scanner, lexer);
                                if (scanner->njk_depth > 0) scanner->njk_depth--;
                                RET_SYM(c2 == '%' ? NJK_STMT_END : c2 == '}' ? NJK_INTERP_END : NJK_CMT_END);
                            }
                            break; // confirmed; mrk_end stays before the '-'
                        }
                    }
                    mrk_end(scanner, lexer);
                    has_content = true;
                    continue;
                }
                adv(scanner, lexer);
                mrk_end(scanner, lexer);
                has_content = true;
            }
            if (has_content) {
                RET_SYM(NJK_CONTENT);
            }
            return false;
        }
        // NJK_CONTENT already consumed — now handle the closing delimiter
        mrk_end(scanner, lexer);
        int32_t lk = lexer->lookahead;
        bool is_closing =
            (lk == '}' && valid_symbols[NJK_INTERP_END]) ||
            (lk == '%' && valid_symbols[NJK_STMT_END]) ||
            (lk == '#' && valid_symbols[NJK_CMT_END]) ||
            // Whitespace control: '-%}' / '-}}' / '-#}' — the dedicated '-' dispatch
            // further down confirms and consumes the full three-char closer.
            (lk == '-' && (valid_symbols[NJK_INTERP_END] || valid_symbols[NJK_STMT_END] ||
                            valid_symbols[NJK_CMT_END]));
        if (!is_closing) {
            return false;
        }
        // Fall through to the main dispatch for the closing delimiter
    }

    bool has_tab_ind = false;
    int16_t leading_spaces = 0;

    for (;;) {
        if (lexer->lookahead == ' ') {
            if (!has_tab_ind) {
                leading_spaces++;
            }
            skp(scanner, lexer);
        } else if (lexer->lookahead == '\t') {
            has_tab_ind = true;
            skp(scanner, lexer);
        } else if (is_nwl(lexer->lookahead)) {
            has_tab_ind = false;
            leading_spaces = 0;
            skp_nwl(scanner, lexer);
        } else if (allow_comment && lexer->lookahead == '#') {
            if (valid_symbols[BR_BLK_STR_CTN] && valid_symbols[BL] && scanner->cur_col <= cur_ind) {
                POP_IND();
                RET_SYM(BL);
            }
            if (valid_symbols[BR_BLK_STR_CTN]
                    ? scanner->cur_row == scanner->row
                    : scanner->cur_col == 0 || scanner->cur_row != scanner->row || scanner->cur_col > scanner->col) {
                adv(scanner, lexer);
                while (!is_nwl(lexer->lookahead) && lexer->lookahead != 0) {
                    adv(scanner, lexer);
                }
                mrk_end(scanner, lexer);
                RET_SYM(COMMENT);
            } else {
                break;
            }
        } else {
            break;
        }
    }

    if (lexer->lookahead == 0) {
        if (valid_symbols[BL]) {
            mrk_end(scanner, lexer);
            POP_IND();
            RET_SYM(BL)
        }
        if (valid_symbols[END_OF_FILE]) {
            mrk_end(scanner, lexer);
            RET_SYM(END_OF_FILE)
        }
        return false;
    }

    int16_t bgn_row = scanner->cur_row;
    int16_t bgn_col = scanner->cur_col;
    int32_t bgn_chr = lexer->lookahead;

    // Hoist here so they are available for the Nunjucks dedent-suppression block below.
    bool has_nwl = scanner->cur_row > scanner->row;
    bool is_r = !has_nwl;
    bool is_br = has_nwl && leading_spaces > cur_ind;
    bool is_b = has_nwl && leading_spaces == cur_ind && !has_tab_ind;
    bool is_s = bgn_col == 0;

    // Nunjucks dedent-suppression: {% %} and {# #} statements are always on their own
    // line (possibly indented) and are extras — they must NOT close surrounding YAML
    // blocks.  If this line would fire a BL (block-level dedent) and leads with a
    // Nunjucks opener, emit the opener directly instead of popping the indent stack.
    // njk_peek advances the lexer irreversibly past '{', so every post-peek outcome
    // (statement, comment, interpolation, flow-map, plain) is fully handled here.
    bool bl_would_fire =
        valid_symbols[BL] && bgn_col <= cur_ind && !has_tab_ind &&
        (cur_ind == prt_ind && cur_ind_typ == IND_SEQ
             ? bgn_col < cur_ind || lexer->lookahead != '-'
             : bgn_col <= prt_ind || cur_ind_typ == IND_STR);

    if (bgn_chr == '{' && bl_would_fire &&
        (valid_symbols[NJK_STMT_BGN] || valid_symbols[NJK_CMT_BGN] || valid_symbols[NJK_INTERP_BGN])) {
        int32_t second = njk_peek(lexer);            // advance past first '{'

        if (second == '%' && valid_symbols[NJK_STMT_BGN]) {
            adv(scanner, lexer); njk_absorb_dash(scanner, lexer); mrk_end(scanner, lexer); scanner->njk_depth++;
            RET_SYM(NJK_STMT_BGN);                   // BL suppressed: statement is indent-transparent
        }
        if (second == '#' && valid_symbols[NJK_CMT_BGN]) {
            adv(scanner, lexer); njk_absorb_dash(scanner, lexer); mrk_end(scanner, lexer); scanner->njk_depth++;
            RET_SYM(NJK_CMT_BGN);                    // BL suppressed: comment is indent-transparent
        }
        if (second == '{' && valid_symbols[NJK_INTERP_BGN]) {
            MAY_UPD_IMP_COL();  // may be an implicit mapping key, e.g. {{ key }}: value
            adv(scanner, lexer); njk_absorb_dash(scanner, lexer); mrk_end(scanner, lexer); scanner->njk_depth++;
            RET_SYM(NJK_INTERP_BGN);                 // BL suppressed for line-leading interpolation
        }
        // Not a Nunjucks opener: we already advanced past '{'. Reproduce the flow-map
        // emission (mirror of the '{' dispatch ~1160-1180); otherwise mark end and fall
        // through to plain-scalar (cur_col - bgn_col == 1 is handled downstream).
        if (valid_symbols[R_FLW_MAP_BGN]  && is_r)  { MAY_UPD_IMP_COL(); mrk_end(scanner, lexer); RET_SYM(R_FLW_MAP_BGN) }
        if (valid_symbols[BR_FLW_MAP_BGN] && is_br) { MAY_UPD_IMP_COL(); mrk_end(scanner, lexer); RET_SYM(BR_FLW_MAP_BGN) }
        if (valid_symbols[B_FLW_MAP_BGN]  && is_b)  { MAY_UPD_IMP_COL(); mrk_end(scanner, lexer); RET_SYM(B_FLW_MAP_BGN) }
        mrk_end(scanner, lexer);                     // advanced one char; fall through to main dispatch
    }

    if (valid_symbols[BL] && bgn_col <= cur_ind && !has_tab_ind) {
        if (cur_ind == prt_ind && cur_ind_typ == IND_SEQ ? bgn_col < cur_ind || lexer->lookahead != '-'
                                                         : bgn_col <= prt_ind || cur_ind_typ == IND_STR) {
            POP_IND();
            RET_SYM(BL);
        }
    }

    if (valid_symbols[R_DIR_YML_VER] && is_r) {
        return scn_dir_yml_ver(scanner, lexer, R_DIR_YML_VER);
    }
    if (valid_symbols[R_DIR_TAG_HDL] && is_r) {
        return scn_dir_tag_hdl(scanner, lexer, R_DIR_TAG_HDL);
    }
    if (valid_symbols[R_DIR_TAG_PFX] && is_r) {
        return scn_dir_tag_pfx(scanner, lexer, R_DIR_TAG_PFX);
    }
    if (valid_symbols[R_DIR_RSV_PRM] && is_r) {
        return scn_dir_rsv_prm(scanner, lexer, R_DIR_RSV_PRM);
    }
    if (valid_symbols[BR_BLK_STR_CTN] && is_br && scn_blk_str_cnt(scanner, lexer, BR_BLK_STR_CTN)) {
        return true;
    }

    if ((valid_symbols[R_DQT_STR_CTN] && is_r && scn_dqt_str_cnt(scanner, lexer, R_DQT_STR_CTN, valid_symbols)) ||
        (valid_symbols[BR_DQT_STR_CTN] && is_br && scn_dqt_str_cnt(scanner, lexer, BR_DQT_STR_CTN, valid_symbols))) {
        return true;
    }

    if ((valid_symbols[R_SQT_STR_CTN] && is_r && scn_sqt_str_cnt(scanner, lexer, R_SQT_STR_CTN, valid_symbols)) ||
        (valid_symbols[BR_SQT_STR_CTN] && is_br && scn_sqt_str_cnt(scanner, lexer, BR_SQT_STR_CTN, valid_symbols))) {
        return true;
    }

    if (valid_symbols[R_ACR_CTN] && is_r) {
        return scn_acr_ctn(scanner, lexer, R_ACR_CTN);
    }
    if (valid_symbols[R_ALS_CTN] && is_r) {
        return scn_als_ctn(scanner, lexer, R_ALS_CTN);
    }

    if (lexer->lookahead == '%') {
        if (valid_symbols[NJK_STMT_END]) {
            // Nunjucks: %} closes a statement tag
            adv(scanner, lexer);
            if (lexer->lookahead == '}') {
                adv(scanner, lexer);
                mrk_end(scanner, lexer);
                if (scanner->njk_depth > 0) scanner->njk_depth--;
                RET_SYM(NJK_STMT_END);
            }
        } else if (valid_symbols[S_DIR_YML_BGN] && is_s) {
            return scn_dir_bgn(scanner, lexer);
        }
    } else if (lexer->lookahead == '*') {
        if (valid_symbols[R_ALS_BGN] && is_r) {
            MAY_UPD_IMP_COL();
            return scn_als_bgn(scanner, lexer, R_ALS_BGN);
        }
        if (valid_symbols[BR_ALS_BGN] && is_br) {
            MAY_UPD_IMP_COL();
            return scn_als_bgn(scanner, lexer, BR_ALS_BGN);
        }
        if (valid_symbols[B_ALS_BGN] && is_b) {
            MAY_UPD_IMP_COL();
            return scn_als_bgn(scanner, lexer, B_ALS_BGN);
        }
    } else if (lexer->lookahead == '&') {
        if (valid_symbols[R_ACR_BGN] && is_r) {
            MAY_UPD_IMP_COL();
            return scn_acr_bgn(scanner, lexer, R_ACR_BGN);
        }
        if (valid_symbols[BR_ACR_BGN] && is_br) {
            MAY_UPD_IMP_COL();
            return scn_acr_bgn(scanner, lexer, BR_ACR_BGN);
        }
        if (valid_symbols[B_ACR_BGN] && is_b) {
            MAY_UPD_IMP_COL();
            return scn_acr_bgn(scanner, lexer, B_ACR_BGN);
        }
    } else if (lexer->lookahead == '!') {
        if (valid_symbols[R_TAG] && is_r) {
            MAY_UPD_IMP_COL();
            return scn_tag(scanner, lexer, R_TAG);
        }
        if (valid_symbols[BR_TAG] && is_br) {
            MAY_UPD_IMP_COL();
            return scn_tag(scanner, lexer, BR_TAG);
        }
        if (valid_symbols[B_TAG] && is_b) {
            MAY_UPD_IMP_COL();
            return scn_tag(scanner, lexer, B_TAG);
        }
    } else if (lexer->lookahead == '[') {
        if (valid_symbols[R_FLW_SEQ_BGN] && is_r) {
            MAY_UPD_IMP_COL();
            adv(scanner, lexer);
            mrk_end(scanner, lexer);
            RET_SYM(R_FLW_SEQ_BGN)
        }
        if (valid_symbols[BR_FLW_SEQ_BGN] && is_br) {
            MAY_UPD_IMP_COL();
            adv(scanner, lexer);
            mrk_end(scanner, lexer);
            RET_SYM(BR_FLW_SEQ_BGN)
        }
        if (valid_symbols[B_FLW_SEQ_BGN] && is_b) {
            MAY_UPD_IMP_COL();
            adv(scanner, lexer);
            mrk_end(scanner, lexer);
            RET_SYM(B_FLW_SEQ_BGN)
        }
    } else if (lexer->lookahead == ']') {
        if (valid_symbols[R_FLW_SEQ_END] && is_r) {
            adv(scanner, lexer);
            mrk_end(scanner, lexer);
            RET_SYM(R_FLW_SEQ_END)
        }
        if (valid_symbols[BR_FLW_SEQ_END] && is_br) {
            adv(scanner, lexer);
            mrk_end(scanner, lexer);
            RET_SYM(BR_FLW_SEQ_END)
        }
        if (valid_symbols[B_FLW_SEQ_END] && is_b) {
            adv(scanner, lexer);
            mrk_end(scanner, lexer);
            RET_SYM(BR_FLW_SEQ_END)
        }
    } else if (lexer->lookahead == '{') {
        // Check for nunjucks opening delimiters: {{ {% {#
        // Use njk_peek to look at the second char without committing — if we don't emit a
        // nunjucks token we must not have advanced, so we check valid_symbols first.
        bool want_njk = valid_symbols[NJK_INTERP_BGN] || valid_symbols[NJK_STMT_BGN] || valid_symbols[NJK_CMT_BGN];
        if (want_njk) {
            int32_t second = njk_peek(lexer); // advances past first '{', no mrk_end
            if (second == '{' && valid_symbols[NJK_INTERP_BGN]) {
                MAY_UPD_IMP_COL();  // may be an implicit mapping key, e.g. {{ key }}: value
                adv(scanner, lexer); // consume second '{'
                njk_absorb_dash(scanner, lexer);
                mrk_end(scanner, lexer);
                scanner->njk_depth++;
                RET_SYM(NJK_INTERP_BGN);
            }
            if (second == '%' && valid_symbols[NJK_STMT_BGN]) {
                adv(scanner, lexer); // consume '%'
                njk_absorb_dash(scanner, lexer);
                mrk_end(scanner, lexer);
                scanner->njk_depth++;
                RET_SYM(NJK_STMT_BGN);
            }
            if (second == '#' && valid_symbols[NJK_CMT_BGN]) {
                adv(scanner, lexer); // consume '#'
                njk_absorb_dash(scanner, lexer);
                mrk_end(scanner, lexer);
                scanner->njk_depth++;
                RET_SYM(NJK_CMT_BGN);
            }
            // Second char wasn't a nunjucks opener. We already advanced past the first '{'.
            // Now we must handle as a flow-map-begin (already advanced, cannot undo).
            if (valid_symbols[R_FLW_MAP_BGN] && is_r) {
                MAY_UPD_IMP_COL();
                mrk_end(scanner, lexer);
                RET_SYM(R_FLW_MAP_BGN)
            }
            if (valid_symbols[BR_FLW_MAP_BGN] && is_br) {
                MAY_UPD_IMP_COL();
                mrk_end(scanner, lexer);
                RET_SYM(BR_FLW_MAP_BGN)
            }
            if (valid_symbols[B_FLW_MAP_BGN] && is_b) {
                MAY_UPD_IMP_COL();
                mrk_end(scanner, lexer);
                RET_SYM(B_FLW_MAP_BGN)
            }
            // Still not a nunjucks opener or a flow-map-begin: this is a lone '{' inside
            // already-open quoted-string content (e.g. "{a: 1}" or 'braces {like} this').
            // scn_dqt_str_cnt/scn_sqt_str_cnt deliberately bail out on ANY '{' without
            // peeking (an unconfirmable '{{' peek can't be undone), so resume their
            // content loop here using the peek we already committed to. mrk_end was
            // never moved since scan() started, so it still marks the position right
            // before this '{', which is exactly where the content token must begin.
            if (valid_symbols[R_DQT_STR_CTN] && is_r) {
                return njk_resume_dqt_cnt(scanner, lexer, R_DQT_STR_CTN, valid_symbols);
            }
            if (valid_symbols[BR_DQT_STR_CTN] && is_br) {
                return njk_resume_dqt_cnt(scanner, lexer, BR_DQT_STR_CTN, valid_symbols);
            }
            if (valid_symbols[R_SQT_STR_CTN] && is_r) {
                return njk_resume_sqt_cnt(scanner, lexer, R_SQT_STR_CTN, valid_symbols);
            }
            if (valid_symbols[BR_SQT_STR_CTN] && is_br) {
                return njk_resume_sqt_cnt(scanner, lexer, BR_SQT_STR_CTN, valid_symbols);
            }
            // Nothing matched — but we advanced. Mark end to prevent corrupt state,
            // then fall through to plain-scalar (cur_col - bgn_col == 1, which is handled).
            mrk_end(scanner, lexer);
        } else {
            // No nunjucks token expected — original flow-map logic unmodified.
            if (valid_symbols[R_FLW_MAP_BGN] && is_r) {
                MAY_UPD_IMP_COL();
                adv(scanner, lexer);
                mrk_end(scanner, lexer);
                RET_SYM(R_FLW_MAP_BGN)
            }
            if (valid_symbols[BR_FLW_MAP_BGN] && is_br) {
                MAY_UPD_IMP_COL();
                adv(scanner, lexer);
                mrk_end(scanner, lexer);
                RET_SYM(BR_FLW_MAP_BGN)
            }
            if (valid_symbols[B_FLW_MAP_BGN] && is_b) {
                MAY_UPD_IMP_COL();
                adv(scanner, lexer);
                mrk_end(scanner, lexer);
                RET_SYM(B_FLW_MAP_BGN)
            }
        }
    } else if (lexer->lookahead == '}') {
        // Nunjucks: }} closes an interpolation (checked before flow-map-end)
        if (valid_symbols[NJK_INTERP_END]) {
            adv(scanner, lexer);
            if (lexer->lookahead == '}') {
                adv(scanner, lexer);
                mrk_end(scanner, lexer);
                if (scanner->njk_depth > 0) scanner->njk_depth--;
                RET_SYM(NJK_INTERP_END);
            }
            // Only one '}' — flow map end
            mrk_end(scanner, lexer);
            if (valid_symbols[R_FLW_MAP_END] && is_r) RET_SYM(R_FLW_MAP_END)
            if (valid_symbols[BR_FLW_MAP_END] && is_br) RET_SYM(BR_FLW_MAP_END)
            if (valid_symbols[B_FLW_MAP_END] && is_b) RET_SYM(BR_FLW_MAP_END)
        } else {
            if (valid_symbols[R_FLW_MAP_END] && is_r) {
                adv(scanner, lexer);
                mrk_end(scanner, lexer);
                RET_SYM(R_FLW_MAP_END)
            }
            if (valid_symbols[BR_FLW_MAP_END] && is_br) {
                adv(scanner, lexer);
                mrk_end(scanner, lexer);
                RET_SYM(BR_FLW_MAP_END)
            }
            if (valid_symbols[B_FLW_MAP_END] && is_b) {
                adv(scanner, lexer);
                mrk_end(scanner, lexer);
                RET_SYM(BR_FLW_MAP_END)
            }
        }
    } else if (lexer->lookahead == '#' && valid_symbols[NJK_CMT_END]) {
        // Nunjucks: #} closes a comment tag (allow_comment is false when NJK_CMT_END is valid,
        // so # was not consumed as a YAML comment in the whitespace loop above)
        adv(scanner, lexer);
        if (lexer->lookahead == '}') {
            adv(scanner, lexer);
            mrk_end(scanner, lexer);
            if (scanner->njk_depth > 0) scanner->njk_depth--;
            RET_SYM(NJK_CMT_END);
        }
    } else if (lexer->lookahead == ',') {
        if (valid_symbols[R_FLW_SEP_BGN] && is_r) {
            adv(scanner, lexer);
            mrk_end(scanner, lexer);
            RET_SYM(R_FLW_SEP_BGN)
        }
        if (valid_symbols[BR_FLW_SEP_BGN] && is_br) {
            adv(scanner, lexer);
            mrk_end(scanner, lexer);
            RET_SYM(BR_FLW_SEP_BGN)
        }
    } else if (lexer->lookahead == '"') {
        if (valid_symbols[R_DQT_STR_BGN] && is_r) {
            MAY_UPD_IMP_COL();
            adv(scanner, lexer);
            mrk_end(scanner, lexer);
            RET_SYM(R_DQT_STR_BGN)
        }
        if (valid_symbols[BR_DQT_STR_BGN] && is_br) {
            MAY_UPD_IMP_COL();
            adv(scanner, lexer);
            mrk_end(scanner, lexer);
            RET_SYM(BR_DQT_STR_BGN)
        }
        if (valid_symbols[B_DQT_STR_BGN] && is_b) {
            MAY_UPD_IMP_COL();
            adv(scanner, lexer);
            mrk_end(scanner, lexer);
            RET_SYM(B_DQT_STR_BGN)
        }
        if (valid_symbols[R_DQT_STR_END] && is_r) {
            adv(scanner, lexer);
            mrk_end(scanner, lexer);
            RET_SYM(R_DQT_STR_END)
        }
        if (valid_symbols[BR_DQT_STR_END] && is_br) {
            adv(scanner, lexer);
            mrk_end(scanner, lexer);
            RET_SYM(BR_DQT_STR_END)
        }
    } else if (lexer->lookahead == '\'') {
        if (valid_symbols[R_SQT_STR_BGN] && is_r) {
            MAY_UPD_IMP_COL();
            adv(scanner, lexer);
            mrk_end(scanner, lexer);
            RET_SYM(R_SQT_STR_BGN)
        }
        if (valid_symbols[BR_SQT_STR_BGN] && is_br) {
            MAY_UPD_IMP_COL();
            adv(scanner, lexer);
            mrk_end(scanner, lexer);
            RET_SYM(BR_SQT_STR_BGN)
        }
        if (valid_symbols[B_SQT_STR_BGN] && is_b) {
            MAY_UPD_IMP_COL();
            adv(scanner, lexer);
            mrk_end(scanner, lexer);
            RET_SYM(B_SQT_STR_BGN)
        }
        if (valid_symbols[R_SQT_STR_END] && is_r) {
            adv(scanner, lexer);
            if (lexer->lookahead == '\'') {
                adv(scanner, lexer);
                mrk_end(scanner, lexer);
                RET_SYM(R_SQT_ESC_SQT)
            } else {
                mrk_end(scanner, lexer);
                RET_SYM(R_SQT_STR_END)
            }
        }
        if (valid_symbols[BR_SQT_STR_END] && is_br) {
            adv(scanner, lexer);
            if (lexer->lookahead == '\'') {
                adv(scanner, lexer);
                mrk_end(scanner, lexer);
                RET_SYM(BR_SQT_ESC_SQT)
            } else {
                mrk_end(scanner, lexer);
                RET_SYM(BR_SQT_STR_END)
            }
        }
    } else if (lexer->lookahead == '?') {
        bool is_r_blk_key_bgn = valid_symbols[R_BLK_KEY_BGN] && is_r;
        bool is_br_blk_key_bgn = valid_symbols[BR_BLK_KEY_BGN] && is_br;
        bool is_b_blk_key_bgn = valid_symbols[B_BLK_KEY_BGN] && is_b;
        bool is_r_flw_key_bgn = valid_symbols[R_FLW_KEY_BGN] && is_r;
        bool is_br_flw_key_bgn = valid_symbols[BR_FLW_KEY_BGN] && is_br;
        if (is_r_blk_key_bgn || is_br_blk_key_bgn || is_b_blk_key_bgn || is_r_flw_key_bgn || is_br_flw_key_bgn) {
            adv(scanner, lexer);
            if (is_wht(lexer->lookahead)) {
                mrk_end(scanner, lexer);
                if (is_r_blk_key_bgn) {
                    PUSH_BGN_IND(IND_MAP);
                    RET_SYM(R_BLK_KEY_BGN);
                }
                if (is_br_blk_key_bgn) {
                    PUSH_BGN_IND(IND_MAP);
                    RET_SYM(BR_BLK_KEY_BGN);
                }
                if (is_b_blk_key_bgn)
                    RET_SYM(B_BLK_KEY_BGN);
                if (is_r_flw_key_bgn)
                    RET_SYM(R_FLW_KEY_BGN);
                if (is_br_flw_key_bgn)
                    RET_SYM(BR_FLW_KEY_BGN);
            }
        }
    } else if (lexer->lookahead == ':') {
        if (valid_symbols[R_FLW_JSV_BGN] && is_r) {
            adv(scanner, lexer);
            mrk_end(scanner, lexer);
            RET_SYM(R_FLW_JSV_BGN);
        }
        if (valid_symbols[BR_FLW_JSV_BGN] && is_br) {
            adv(scanner, lexer);
            mrk_end(scanner, lexer);
            RET_SYM(BR_FLW_JSV_BGN);
        }
        bool is_r_blk_val_bgn = valid_symbols[R_BLK_VAL_BGN] && is_r;
        bool is_br_blk_val_bgn = valid_symbols[BR_BLK_VAL_BGN] && is_br;
        bool is_b_blk_val_bgn = valid_symbols[B_BLK_VAL_BGN] && is_b;
        bool is_r_blk_imp_bgn = valid_symbols[R_BLK_IMP_BGN] && is_r;
        bool is_r_flw_njv_bgn = valid_symbols[R_FLW_NJV_BGN] && is_r;
        bool is_br_flw_njv_bgn = valid_symbols[BR_FLW_NJV_BGN] && is_br;
        if (is_r_blk_val_bgn || is_br_blk_val_bgn || is_b_blk_val_bgn || is_r_blk_imp_bgn || is_r_flw_njv_bgn ||
            is_br_flw_njv_bgn) {
            adv(scanner, lexer);
            bool is_lka_wht = is_wht(lexer->lookahead);
            if (is_lka_wht) {
                if (is_r_blk_val_bgn) {
                    PUSH_BGN_IND(IND_MAP);
                    mrk_end(scanner, lexer);
                    RET_SYM(R_BLK_VAL_BGN);
                }
                if (is_br_blk_val_bgn) {
                    PUSH_BGN_IND(IND_MAP);
                    mrk_end(scanner, lexer);
                    RET_SYM(BR_BLK_VAL_BGN);
                }
                if (is_b_blk_val_bgn) {
                    mrk_end(scanner, lexer);
                    RET_SYM(B_BLK_VAL_BGN);
                }
                if (is_r_blk_imp_bgn) {
                    MAY_PUSH_IMP_IND();
                    mrk_end(scanner, lexer);
                    RET_SYM(R_BLK_IMP_BGN);
                }
            }
            if (is_lka_wht || lexer->lookahead == ',' || lexer->lookahead == ']' || lexer->lookahead == '}') {
                if (is_r_flw_njv_bgn) {
                    mrk_end(scanner, lexer);
                    RET_SYM(R_FLW_NJV_BGN);
                }
                if (is_br_flw_njv_bgn) {
                    mrk_end(scanner, lexer);
                    RET_SYM(BR_FLW_NJV_BGN);
                }
            }
        }
    } else if (lexer->lookahead == '-' &&
               (valid_symbols[NJK_STMT_END] || valid_symbols[NJK_INTERP_END] || valid_symbols[NJK_CMT_END])) {
        // Nunjucks whitespace control: '-%}' / '-}}' / '-#}' closes a tag, absorbing
        // the trim marker into the closing delimiter token (mirrors the plain '%}' /
        // '}}' / '#}' closers above).
        adv(scanner, lexer);
        int32_t second = lexer->lookahead;
        if ((second == '%' && valid_symbols[NJK_STMT_END]) ||
            (second == '}' && valid_symbols[NJK_INTERP_END]) ||
            (second == '#' && valid_symbols[NJK_CMT_END])) {
            adv(scanner, lexer);
            if (lexer->lookahead == '}') {
                adv(scanner, lexer);
                mrk_end(scanner, lexer);
                if (scanner->njk_depth > 0) scanner->njk_depth--;
                RET_SYM(second == '%' ? NJK_STMT_END : second == '}' ? NJK_INTERP_END : NJK_CMT_END);
            }
        }
    } else if (lexer->lookahead == '-') {
        bool is_r_blk_seq_bgn = valid_symbols[R_BLK_SEQ_BGN] && is_r;
        bool is_br_blk_seq_bgn = valid_symbols[BR_BLK_SEQ_BGN] && is_br;
        bool is_b_blk_seq_bgn = valid_symbols[B_BLK_SEQ_BGN] && is_b;
        bool is_s_drs_end = is_s;
        if (is_r_blk_seq_bgn || is_br_blk_seq_bgn || is_b_blk_seq_bgn || is_s_drs_end) {
            adv(scanner, lexer);
            if (is_wht(lexer->lookahead)) {
                if (is_r_blk_seq_bgn) {
                    PUSH_BGN_IND(IND_SEQ);
                    mrk_end(scanner, lexer);
                    RET_SYM(R_BLK_SEQ_BGN)
                }
                if (is_br_blk_seq_bgn) {
                    PUSH_BGN_IND(IND_SEQ);
                    mrk_end(scanner, lexer);
                    RET_SYM(BR_BLK_SEQ_BGN)
                }
                if (is_b_blk_seq_bgn) {
                    MAY_PUSH_SPC_SEQ_IND();
                    mrk_end(scanner, lexer);
                    RET_SYM(B_BLK_SEQ_BGN)
                }
            } else if (lexer->lookahead == '-' && is_s_drs_end) {
                adv(scanner, lexer);
                if (lexer->lookahead == '-') {
                    adv(scanner, lexer);
                    if (is_wht(lexer->lookahead)) {
                        if (valid_symbols[BL]) {
                            POP_IND();
                            RET_SYM(BL);
                        }
                        mrk_end(scanner, lexer);
                        RET_SYM(S_DRS_END);
                    }
                }
            }
        }
    } else if (lexer->lookahead == '.') {
        if (is_s) {
            adv(scanner, lexer);
            if (lexer->lookahead == '.') {
                adv(scanner, lexer);
                if (lexer->lookahead == '.') {
                    adv(scanner, lexer);
                    if (is_wht(lexer->lookahead)) {
                        if (valid_symbols[BL]) {
                            POP_IND();
                            RET_SYM(BL);
                        }
                        mrk_end(scanner, lexer);
                        RET_SYM(S_DOC_END);
                    }
                }
            }
        }
    } else if (lexer->lookahead == '\\') {
        bool is_r_dqt_esc_nwl = valid_symbols[R_DQT_ESC_NWL] && is_r;
        bool is_br_dqt_esc_nwl = valid_symbols[BR_DQT_ESC_NWL] && is_br;
        bool is_r_dqt_esc_seq = valid_symbols[R_DQT_ESC_SEQ] && is_r;
        bool is_br_dqt_esc_seq = valid_symbols[BR_DQT_ESC_SEQ] && is_br;
        if (is_r_dqt_esc_nwl || is_br_dqt_esc_nwl || is_r_dqt_esc_seq || is_br_dqt_esc_seq) {
            adv(scanner, lexer);
            if (is_nwl(lexer->lookahead)) {
                if (is_r_dqt_esc_nwl) {
                    mrk_end(scanner, lexer);
                    RET_SYM(R_DQT_ESC_NWL)
                }
                if (is_br_dqt_esc_nwl) {
                    mrk_end(scanner, lexer);
                    RET_SYM(BR_DQT_ESC_NWL)
                }
            }
            if (is_r_dqt_esc_seq) {
                return scn_dqt_esc_seq(scanner, lexer, R_DQT_ESC_SEQ);
            }
            if (is_br_dqt_esc_seq) {
                return scn_dqt_esc_seq(scanner, lexer, BR_DQT_ESC_SEQ);
            }
            return false;
        }
    } else if (lexer->lookahead == '|') {
        if (valid_symbols[R_BLK_LIT_BGN] && is_r) {
            return scn_blk_str_bgn(scanner, lexer, R_BLK_LIT_BGN);
        }
        if (valid_symbols[BR_BLK_LIT_BGN] && is_br) {
            return scn_blk_str_bgn(scanner, lexer, BR_BLK_LIT_BGN);
        }
    } else if (lexer->lookahead == '>') {
        if (valid_symbols[R_BLK_FLD_BGN] && is_r) {
            return scn_blk_str_bgn(scanner, lexer, R_BLK_FLD_BGN);
        }
        if (valid_symbols[BR_BLK_FLD_BGN] && is_br) {
            return scn_blk_str_bgn(scanner, lexer, BR_BLK_FLD_BGN);
        }
    }

    bool maybe_sgl_pln_blk = (valid_symbols[R_SGL_PLN_STR_BLK] && is_r) ||
                             (valid_symbols[BR_SGL_PLN_STR_BLK] && is_br) || (valid_symbols[B_SGL_PLN_STR_BLK] && is_b);
    bool maybe_sgl_pln_flw = (valid_symbols[R_SGL_PLN_STR_FLW] && is_r) || (valid_symbols[BR_SGL_PLN_STR_FLW] && is_br);
    bool maybe_mtl_pln_blk = (valid_symbols[R_MTL_PLN_STR_BLK] && is_r) || (valid_symbols[BR_MTL_PLN_STR_BLK] && is_br);
    bool maybe_mtl_pln_flw = (valid_symbols[R_MTL_PLN_STR_FLW] && is_r) || (valid_symbols[BR_MTL_PLN_STR_FLW] && is_br);

    if (maybe_sgl_pln_blk || maybe_sgl_pln_flw || maybe_mtl_pln_blk || maybe_mtl_pln_flw) {
        bool is_in_blk = maybe_sgl_pln_blk || maybe_mtl_pln_blk;
        bool (*is_plain_safe)(int32_t) = is_in_blk ? is_plain_safe_in_block : is_plain_safe_in_flow;
        if (scanner->cur_col - bgn_col == 0) {
            adv(scanner, lexer);
        }
        if (scanner->cur_col - bgn_col == 1) {
            bool is_plain_first =
                (is_ns_char(bgn_chr) && !is_c_indicator(bgn_chr)) ||
                ((bgn_chr == '-' || bgn_chr == '?' || bgn_chr == ':') && is_plain_safe(lexer->lookahead));
            if (!is_plain_first) {
                return false;
            }
            scanner->sch_stt = adv_sch_stt(scanner->sch_stt, scanner->cur_chr, &scanner->rlt_sch);
        } else {
            // no need to check the following cases:
            // ..X
            // ...X
            // --X
            // ---X
            // X: lookahead
            scanner->sch_stt = SCH_STT_FRZ; // must be RS_STR
        }

        mrk_end(scanner, lexer);

        for (;;) {
            if (!is_nwl(lexer->lookahead)) {
                if (scn_pln_cnt(scanner, lexer, is_plain_safe) != SCN_SUCC) {
                    break;
                }
            }
            if (lexer->lookahead == 0 || !is_nwl(lexer->lookahead)) {
                break;
            }
            for (;;) {
                if (is_nwl(lexer->lookahead)) {
                    adv_nwl(scanner, lexer);
                } else if (is_wsp(lexer->lookahead)) {
                    adv(scanner, lexer);
                } else {
                    break;
                }
            }
            if (lexer->lookahead == 0 || scanner->cur_col <= cur_ind) {
                break;
            }
            if (scanner->cur_col == 0 && scn_drs_doc_end(scanner, lexer)) {
                break;
            }
        }

        if (scanner->end_row == bgn_row) {
            if (maybe_sgl_pln_blk) {
                MAY_UPD_IMP_COL();
                RET_SYM(is_r ? SGL_PLN_SYM(R, BLK) : is_br ? SGL_PLN_SYM(BR, BLK) : SGL_PLN_SYM(B, BLK));
            }
            if (maybe_sgl_pln_flw)
                RET_SYM(is_r ? SGL_PLN_SYM(R, FLW) : SGL_PLN_SYM(BR, FLW));
        } else {
            if (maybe_mtl_pln_blk) {
                MAY_UPD_IMP_COL();
                RET_SYM(is_r ? R_MTL_PLN_STR_BLK : BR_MTL_PLN_STR_BLK);
            }
            if (maybe_mtl_pln_flw)
                RET_SYM(is_r ? R_MTL_PLN_STR_FLW : BR_MTL_PLN_STR_FLW);
        }

        return false;
    }

    return !valid_symbols[ERR_REC];
}

void *tree_sitter_yaml_nunjucks_external_scanner_create() {
    Scanner *scanner = ts_calloc(1, sizeof(Scanner));
    deserialize(scanner, NULL, 0);
    return scanner;
}

void tree_sitter_yaml_nunjucks_external_scanner_destroy(void *payload) {
    Scanner *scanner = (Scanner *)payload;
    array_delete(&scanner->ind_len_stk);
    array_delete(&scanner->ind_typ_stk);
    ts_free(scanner);
}

unsigned tree_sitter_yaml_nunjucks_external_scanner_serialize(void *payload, char *buffer) {
    Scanner *scanner = (Scanner *)payload;
    return serialize(scanner, buffer);
}

void tree_sitter_yaml_nunjucks_external_scanner_deserialize(void *payload, const char *buffer, unsigned length) {
    Scanner *scanner = (Scanner *)payload;
    deserialize(scanner, buffer, length);
}

bool tree_sitter_yaml_nunjucks_external_scanner_scan(void *payload, TSLexer *lexer, const bool *valid_symbols) {
    Scanner *scanner = (Scanner *)payload;
    return scan(scanner, lexer, valid_symbols);
}
