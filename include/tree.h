/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
 * copyright (c) 2010-2011 Storlek
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

#ifndef TREE_H
#define TREE_H

/* opaque structure */
typedef struct tree tree_t;

/* This function should behave like strcmp. (i.e. return < 0, 0, or > 0 depending on how 'a' relates to 'b') */
typedef int (*treecmp_t) (const void *a, const void *b);

/* warning; don't change any part of value that would alter the return of treecmp! */
typedef void (*treewalk_t) (void *value);


/* Create a new tree. */
tree_t *tree_alloc(treecmp_t cmp);

/* Deallocate a tree. 'freeval' is called for each node in the tree. */
void tree_free(tree_t *tree, treewalk_t freeval);

/* Postorder traversal. */
void tree_walk(tree_t *tree, treewalk_t apply);

/* On successful insert, this function returns NULL. If one of the items in the tree compares equal to 'value',
the tree is not modified, and the existing value in the tree is returned. */
void *tree_insert(tree_t *tree, void *value);

/* On successful insert, this function returns NULL. If one of the items in the tree compares equal to 'value',
its value is replaced and the previous value is returned.
It is entirely possible to wrap this function in a free() call to deallocate the old data. */
void *tree_replace(tree_t *tree, void *value);

/* If one of the items in the tree compares equal to 'value', its value is returned. Otherwise, this function
returns NULL. (Only the parts of 'value' relevant to 'cmp' need be filled in.) */
void *tree_find(tree_t *tree, void *value);

#endif /* ! TREE_H */

