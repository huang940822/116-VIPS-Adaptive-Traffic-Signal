#ifndef LIST_H
#define LIST_H
#include <stddef.h>
/* "typeof" is a GNU extension.
 * Reference: https://gcc.gnu.org/onlinedocs/gcc/Typeof.html
 */
#if defined(__GNUC__)
#define __LIST_HAVE_TYPEOF 1
#endif
/**
 * container_of() - Calculate address of object that contains address ptr
 * @ptr: pointer to member variable
 * @type: type of the structure containing ptr
 * @member: name of the member variable in struct @type
 *
 * Return: @type pointer of object containing ptr
 */
#ifndef container_of
#ifdef __LIST_HAVE_TYPEOF
#define container_of(ptr, type, member)                           \
	__extension__({                                               \
		const __typeof__(((type *)0)->member) *__pmember = (ptr); \
		(type *)((char *)__pmember - offsetof(type, member));     \
	})
#else
#define container_of(ptr, type, member) \
	((type *)((char *)(ptr)-offsetof(type, member)))
#endif
#endif  //end of function 'container_of'

struct list_head {
	struct list_head *prev;
	struct list_head *next;
};
#define LIST_HEAD(head) struct list_head head = {&(head), &(head)}

/**
 * list_entry() - Calculate address of entry that contains list node
 * @node: pointer to list node
 * @type: type of the entry containing the list node
 * @member: name of the list_head member variable in struct @type
 *
 * Return: @type pointer of entry containing node
 */
#define list_entry(node, type, member) container_of(node, type, member)

#define list_first_entry(head, type, member) \
	list_entry((head)->next, type, member)

#define list_last_entry(head, type, member) \
	list_entry((head)->prev, type, member)

/**
 * list_for_each - iterate over list nodes
 * @node: list_head pointer used as iterator
 * @head: pointer to the head of the list
 *
 * The nodes and the head of the list must must be kept unmodified while
 * iterating through it. Any modifications to the the list will cause undefined
 * behavior.
 */
#define list_for_each(node, head) \
	for (node = (head)->next; node != (head); node = node->next)

/**
 * list_for_each_entry - iterate over list entries
 * @entry: pointer used as iterator
 * @head: pointer to the head of the list
 * @member: name of the list_head member variable in struct type of @entry
 *
 * The nodes and the head of the list must must be kept unmodified while
 * iterating through it. Any modifications to the the list will cause undefined
 * behavior.
 *
 * FIXME: remove dependency of __typeof__ extension
 */
#ifdef __LIST_HAVE_TYPEOF
#define list_for_each_entry(entry, head, member)                       \
	for (entry = list_entry((head)->next, __typeof__(*entry), member); \
	     &entry->member != (head);                                     \
	     entry = list_entry(entry->member.next, __typeof__(*entry), member))
#endif

/**
 * list_for_each_safe - iterate over list nodes and allow deletes
 * @node: list_head pointer used as iterator
 * @safe: list_head pointer used to store info for next entry in list
 * @head: pointer to the head of the list
 *
 * The current node (iterator) is allowed to be removed from the list. Any
 * other modifications to the the list will cause undefined behavior.
 */
#define list_for_each_safe(node, safe, head)                     \
	for (node = (head)->next, safe = node->next; node != (head); \
	     node = safe, safe = node->next)

/**
 * list_for_each_entry_safe - iterate over list entries and allow deletes
 * @entry: pointer used as iterator
 * @safe: @type pointer used to store info for next entry in list
 * @head: pointer to the head of the list
 * @member: name of the list_head member variable in struct type of @entry
 *
 * The current node (iterator) is allowed to be removed from the list. Any
 * other modifications to the the list will cause undefined behavior.
 *
 * FIXME: remove dependency of __typeof__ extension
 */
#define list_for_each_entry_safe(entry, safe, head, member)                \
	for (entry = list_entry((head)->next, __typeof__(*entry), member),     \
	    safe = list_entry(entry->member.next, __typeof__(*entry), member); \
	     &entry->member != (head); entry = safe,                           \
	    safe = list_entry(safe->member.next, __typeof__(*entry), member))

static inline void INIT_LIST_HEAD(struct list_head *head)
{
	head->next = head;
	head->prev = head;
}
/**
 * list_add() - Add a list node to the beginning of the list
 * @node: pointer to the new node
 * @head: pointer to the head of the list
 */
static inline void list_add(struct list_head *node, struct list_head *head)
{
	struct list_head *next = head->next;

	next->prev = node;
	node->next = next;
	node->prev = head;
	head->next = node;
}
/**
 * list_add_tail() - Add a list node to the end of the list
 * @node: pointer to the new node
 * @head: pointer to the head of the list
 */
static inline void list_add_tail(struct list_head *node, struct list_head *head)
{
	struct list_head *prev = head->prev;

	prev->next = node;
	node->next = head;
	node->prev = prev;
	head->prev = node;
}
/**
 * list_del() - Remove a list node from the list
 * @node: pointer to the node
 *
 * The node is only removed from the list. Neither the memory of the removed
 * node nor the memory of the entry containing the node is free'd. The node
 * has to be handled like an uninitialized node. Accessing the next or prev
 * pointer of the node is not safe.
 *
 * Unlinked, initialized nodes are also uninitialized after list_del.
 * (If due to some usage or some reason the deleted node will not be freed for a 
 * while,I strongly recommand you to use list_del_init.)
 *
 * LIST_POISONING can be enabled during build-time to provoke an invalid memory
 * access when the memory behind the next/prev pointer is used after a list_del.
 * This only works on systems which prohibit access to the predefined memory
 * addresses.
 */
static inline void list_del(struct list_head *node)
{
	struct list_head *next = node->next;
	struct list_head *prev = node->prev;

	next->prev = prev;
	prev->next = next;

#ifdef LIST_POISONING
	node->prev = (struct list_head *)(0x00100100);
	node->next = (struct list_head *)(0x00200200);
#endif
}

/**
 * list_del_init() - Remove a list node from the list and reinitialize it
 * @node: pointer to the node
 *
 * The removed node will not end up in an uninitialized state like when using
 * list_del. Instead the node is initialized again to the unlinked state.
 */
static inline void list_del_init(struct list_head *node)
{
	list_del(node);
	INIT_LIST_HEAD(node);
}

/**
 * list_empty() - Check if list head has no nodes attached
 * @head: pointer to the head of the list
 *
 * Return: 0 - list is not empty !0 - list is empty
 */
static inline int list_empty(const struct list_head *head)
{
	return (head->next == head);
}

/**
 * list_is_singular() - Check if list head has exactly one node attached
 * @head: pointer to the head of the list
 *
 * Return: 0 - list is not singular !0 -list has exactly one entry
 */
static inline int list_is_singular(const struct list_head *head)
{
	return (!list_empty(head) && head->prev == head->next);
}

/**
 * list_move() - Move a list node to the beginning of the list
 * @node: pointer to the node
 * @head: pointer to the head of the list
 *
 * The @node is removed from its old position/node and add to the beginning of
 * @head
 */
static inline void list_move(struct list_head *node, struct list_head *head)
{
	list_del(node);
	list_add(node, head);
}

/**
 * list_move_tail() - Move a list node to the end of the list
 * @node: pointer to the node
 * @head: pointer to the head of the list
 *
 * The @node is removed from its old position/node and add to the end of @head
 */
static inline void list_move_tail(struct list_head *node,
                                  struct list_head *head)
{
	list_del(node);
	list_add_tail(node, head);
}

#undef __LIST_HAVE_TYPEOF

#endif  //end of list.h
