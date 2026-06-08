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
(nunjucks_interpolation) @punctuation.special
(nunjucks_statement) @punctuation.special
; nunjucks_keyword captures the first word of {% %} statements.
; "in" and other mid-statement words appear in nunjucks_expression (@embedded) —
; editors may highlight them via a secondary injection or regex scope.
((nunjucks_keyword) @keyword
  (#any-of? @keyword
    "if" "elif" "else" "endif"
    "for" "endfor" "asyncEach" "endeach" "asyncAll" "endall"
    "in"
    "set" "endset" "block" "endblock" "extends" "include" "import"
    "from" "macro" "endmacro" "call" "endcall" "filter" "endfilter"
    "raw" "endraw" "verbatim" "endverbatim" "ignore" "missing"
    "recursive" "as" "with" "context" "endwith"))
(nunjucks_comment) @comment
(nunjucks_expression) @embedded
