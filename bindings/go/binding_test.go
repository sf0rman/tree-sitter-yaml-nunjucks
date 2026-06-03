package tree_sitter_yaml_nunjucks_test

import (
	"testing"

	tree_sitter "github.com/tree-sitter/go-tree-sitter"
	tree_sitter_yaml_nunjucks "github.com/tree-sitter/tree-sitter-yaml-nunjucks/bindings/go"
)

func TestCanLoadGrammar(t *testing.T) {
	language := tree_sitter.NewLanguage(tree_sitter_yaml_nunjucks.Language())
	if language == nil {
		t.Errorf("Error loading Yaml grammar")
	}
}
