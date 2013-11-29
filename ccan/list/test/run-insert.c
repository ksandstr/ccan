#define CCAN_LIST_DEBUG 1
#include <ccan/list/list.h>
#include <ccan/tap/tap.h>
#include <ccan/list/list.c>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>
#include <assert.h>

struct int_node {
	struct list_node n;
	int value;
};


static struct list_node *mknode(int value) {
	struct int_node *n = malloc(sizeof(*n));
	n->value = value;
	return &n->n;
}


static void rmnodes(struct list_head *list)
{
	struct int_node *n, *next;
	list_for_each_safe(list, n, next, n) {
		list_del_from(list, &n->n);
		free(n);
	}
	assert(list_empty(list));
}


static bool is_ascending(struct list_head *list)
{
	int prev = INT_MIN;
	struct int_node *cur;
	list_for_each(list, cur, n) {
		if(prev > cur->value) return false;
		prev = cur->value;
	}
	return true;
}


int main(void)
{
	struct list_head *head;
	struct list_node *nptr;
	struct int_node *node, *next;
	int i;

	plan_tests(5);
	head = malloc(sizeof(*head));	/* improves valgrinding */

	/* selftest: see that is_ascending() may return false. */
	list_head_init(head);
	list_add_tail(head, mknode(1));
	list_add_tail(head, mknode(0));
	ok(!is_ascending(head), "is_ascending() self-test");
	rmnodes(head);

	/* base case: in the absence of list_insert(), list_add_tail() creates a
	 * list in ascending order.
	 */
	list_head_init(head);
	for(i=0; i < 100; i++) list_add_tail(head, mknode(i));
	ok(is_ascending(head), "ascending tail-adds produce ascending order");
	rmnodes(head);

	/* empty case. list_insert() behind the list's head node. like, wow,
	 * man.
	 */
	list_head_init(head);
	list_insert(&head->n, mknode(0));
	list_check(head, "list_insert() caused breakage [empty]");
	ok(!list_empty(head), "insert into zero-length list");
	rmnodes(head);

	/* one-node case. list_insert at the only node should act like
	 * list_add().
	 */
	list_head_init(head);
	list_add_tail(head, nptr = mknode(0));
	list_insert(nptr, mknode(-1));
	list_check(head, "list_insert() caused breakage [one-node]");
	ok(is_ascending(head), "insert into one-item list");
	rmnodes(head);

	/* many-node case. first add odd items at tail, then insert even items
	 * between them.
	 */
	list_head_init(head);
	for(i=1; i < 100; i += 2) list_add_tail(head, mknode(i));
	i = 0;
	list_for_each_safe(head, node, next, n) {
		list_insert(&node->n, mknode(i));
		assert(i == node->value - 1);
		i += 2;
	}
	list_check(head, "list_insert() caused breakage [multi]");
	ok(is_ascending(head), "multi-insert into long list");
	rmnodes(head);

	free(head);

	return exit_status();
}
