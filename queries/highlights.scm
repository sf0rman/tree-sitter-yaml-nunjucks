(boolean_scalar) @boolean

(null_scalar) @constant.builtin

[
  (double_quote_scalar)
  (single_quote_scalar)
  (block_scalar)
  (string_scalar)
] @string

[
  (integer_scalar)
  (float_scalar)
] @number

(comment) @comment

[
  (anchor_name)
  (alias_name)
] @label

(tag) @type

[
  (yaml_directive)
  (tag_directive)
  (reserved_directive)
] @attribute

(block_mapping_pair
  key: (flow_node
    [
      (double_quote_scalar)
      (single_quote_scalar)
    ] @property))

(block_mapping_pair
  key: (flow_node
    (plain_scalar
      (string_scalar) @property)))

(flow_mapping
  (_
    key: (flow_node
      [
        (double_quote_scalar)
        (single_quote_scalar)
      ] @property)))

(flow_mapping
  (_
    key: (flow_node
      (plain_scalar
        (string_scalar) @property))))

[
  ","
  "-"
  ":"
  ">"
  "?"
  "|"
] @punctuation.delimiter

[
  "["
  "]"
  "{"
  "}"
] @punctuation.bracket

[
  "*"
  "&"
  "---"
  "..."
] @punctuation.special

; Nunjucks template syntax
; Delimiters ({{ }}, {% %}, {# #}, including any whitespace-control '-') are their
; own node, captured directly instead of painting the whole construct and relying
; on children to override it.
(nunjucks_delimiter) @punctuation.special

; nunjucks_keyword is specifically the leading word of a {% %} statement (if/for/...).
((nunjucks_keyword) @keyword
  (#any-of? @keyword
    "if" "elif" "else" "endif"
    "for" "endfor" "asyncEach" "endeach" "asyncAll" "endall"
    "in"
    "set" "endset" "block" "endblock" "extends" "include" "import"
    "from" "macro" "endmacro" "call" "endcall" "filter" "endfilter"
    "raw" "endraw" "verbatim" "endverbatim" "ignore" "missing"
    "recursive" "as" "with" "context" "endwith"))

; nunjucks_identifier covers every other identifier in the expression body:
; variable names, filter/function names, and mid-statement words. Default to
; @variable, then override with @keyword for the word forms below by text (a
; later pattern's capture wins over an earlier one for the same node).
(nunjucks_identifier) @variable
((nunjucks_identifier) @keyword
  (#any-of? @keyword
    "in" "and" "or" "not" "is" "if" "else"
    "recursive" "as" "with" "context"))

; A nunjucks interpolation used as a mapping key (e.g. "{{ name }}": value) reads
; as the property name, same as any other key.
(block_mapping_pair key: (nunjucks_interpolation) @property)
(flow_pair key: (nunjucks_interpolation) @property)

(nunjucks_comment) @comment
; nunjucks_content is raw operator/punctuation/whitespace text with no further
; structure — left uncaptured so it renders in the default foreground.
