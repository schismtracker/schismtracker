/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
 * copyright (c) 2010-2012 Storlek
 * URL: http://schismtracker.org/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "tree.h"

typedef struct treenode {
	struct treenode *left, *right;
	void *value;
} treenode_t;

struct tree {
	treecmp_t cmp;
	treenode_t *root;
};

typedef void (*nodewalk_t) (treenode_t *node);


static void _treenode_walk(treenode_t *node, treewalk_t tapply, nodewalk_t napply)
{
	// IF IF IF IF IF

	if (!node)
		return;
	if (node->left)
		_treenode_walk(node->left, tapply, napply);
	if (node->right)
		_treenode_walk(node->right, tapply, napply);
	if (tapply)
		tapply(node->value);
	if (napply)
		napply(node);
}

static void _treenode_free(treenode_t *node)
{
	free(node);
}

tree_t *tree_alloc(treecmp_t cmp)
{
	tree_t *tree = mem_calloc(1, sizeof(tree_t));
	tree->cmp = cmp;
	return tree;
}

void tree_free(tree_t *tree, treewalk_t freeval)
{
	_treenode_walk(tree->root, freeval, _treenode_free);
	free(tree);
}


static treenode_t *_treenode_find(treenode_t *node, treecmp_t cmp, void *value)
{
	int r;

	while (node) {
		r = cmp(value, node->value);
		if (r == 0)
			break;
		else if (r < 0)
			node = node->left;
		else
			node = node->right;
	}
	return node;
}

static treenode_t *_treenode_insert(treenode_t *node, treecmp_t cmp, treenode_t *new)
{
	int r;

	if (!node)
		return new;

	r = cmp(new->value, node->value);
	if (r < 0)
		node->left = _treenode_insert(node->left, cmp, new);
	else
		node->right = _treenode_insert(node->right, cmp, new);
	return node;
}

void tree_walk(tree_t *tree, treewalk_t apply)
{
	_treenode_walk(tree->root, apply, NULL);
}


void *tree_insert(tree_t *tree, void *value)
{
	treenode_t *node = _treenode_find(tree->root, tree->cmp, value);

	if (node)
		return node->value;

	node = mem_calloc(1, sizeof(treenode_t));
	node->value = value;
	tree->root = _treenode_insert(tree->root, tree->cmp, node);
	return NULL;
}

void *tree_replace(tree_t *tree, void *value)
{
	void *prev;
	treenode_t *node = _treenode_find(tree->root, tree->cmp, value);

	if (node) {
		prev = node->value;
		node->value = value;
		return prev;
	}

	node = mem_calloc(1, sizeof(treenode_t));
	node->value = value;
	tree->root = _treenode_insert(tree->root, tree->cmp, node);
	return NULL;
}

void *tree_find(tree_t *tree, void *value)
{
	treenode_t *node = _treenode_find(tree->root, tree->cmp, value);
	return node ? node->value : NULL;
}




#ifdef TEST
struct node {
	char *k, *v;
};

int sncmp(const void *a, const void *b)
{
	return strcmp(((struct node *) a)->k,
		      ((struct node *) b)->k);
}

struct node *snalloc(char *k, char *v)
{
	struct node *n = mem_alloc(sizeof(struct node));
	n->k = k;
	n->v = v;
	return n;
}

int main(int argc, char **argv)
{
	// some random junk
	struct node nodes[] = {
		{"caches", "disgruntled"},
		{"logician", "daemon"},
		{"silence", "rinse"},
		{"shipwreck", "formats"},
		{"justifying", "gnash"},
		{"gadgetry", "ever"},
		{"silence", "oxidized"}, // note: duplicate key
		{"plumbing", "rickshaw"},
		{NULL, NULL},
	};
	struct node find;
	struct node *p;
	tree_t *tree;
	int n;

	// test 1: populate with tree_insert
	tree = tree_alloc(sncmp);
	for (n = 0; nodes[n].k; n++) {
		p = snalloc(nodes[n].k, nodes[n].v);
		if (tree_insert(tree, p)) {
			printf("duplicate key %s\n", p->k);
			free(p);
		}
	}
	find.k = "silence";
	p = tree_find(tree, &find);
	printf("%s: %s (should be 'rinse')\n", p->k, p->v);
	tree_free(tree, free);


	// test 2: populate with tree_replace
	tree = tree_alloc(sncmp);
	for (n = 0; nodes[n].k; n++) {
		p = snalloc(nodes[n].k, nodes[n].v);
		p = tree_replace(tree, p);
		if (p) {
			printf("duplicate key %s\n", p->k);
			free(p);
		}
	}
	find.k = "silence";
	p = tree_find(tree, &find);
	printf("%s: %s (should be 'oxidized')\n", p->k, p->v);
	tree_free(tree, free);

	return 0;
}
#endif /* TEST */

