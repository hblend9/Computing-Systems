/*
 * mm-explicit.c - The best malloc package EVAR!
 *
 * TODO (bug): Uh..this is an implicit list???
 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#include "memlib.h"
#include "mm.h"

/** The required alignment of heap payloads */
const size_t ALIGNMENT = 2 * sizeof(size_t);

/** The layout of each block allocated on the heap */
typedef struct {
    /** The size of the block and whether it is allocated (stored in the low bit) */
    size_t header;
    /**
     * We don't know what the size of the payload will be, so we will
     * declare it as a zero-length array.  This allow us to obtain a
     * pointer to the start of the payload.
     */
    uint8_t payload[];
} block_t;

typedef struct free_block {
    size_t header;
    struct free_block *prev;
    struct free_block *next;
} free_block_t;

typedef struct {
    size_t f_size;
} footer_t;

/** The first and last blocks on the heap */
static block_t *mm_heap_first = NULL;
static block_t *mm_heap_last = NULL;
static free_block_t *head = NULL;

void add_block(block_t *block) {
    free_block_t *free_block = (free_block_t *) block;
    free_block->header = block->header;
    if (head == NULL) {
        head = free_block;
        head->header = block->header;
        head->next = NULL;
        head->prev = NULL;
        return;
    }
    free_block_t *temp = head;
    head = free_block;
    head->next = temp;
    temp->prev = head;
}

void remove_block(block_t *block) {
    free_block_t *free_block = (free_block_t *) block;
    if (head == free_block) {
        if (free_block->next == NULL) { // only have head
            head = NULL;
            return;
        }
        head = head->next;
        return; // nothing to remove
    }
    free_block_t *prev_block = free_block->prev;
    free_block_t *next_block = free_block->next;
    prev_block->next = next_block;
    if (next_block != NULL) {
        next_block->prev = prev_block;
    }
}

/** Rounds up `size` to the nearest multiple of `n` */
static size_t round_up(size_t size, size_t n) {
    return (size + (n - 1)) / n * n;
}

/** Extracts a block's size from its header */
static size_t get_size(block_t *block) {
    return block->header & ~1;
}

/** Set's a block's header with the given size and allocation state */
static void set_header(block_t *block, size_t size, bool is_allocated) {
    block->header = size | is_allocated;
    footer_t *footer = (void *) block + get_size(block) - sizeof(footer_t);
    footer->f_size = size | is_allocated;
}

/** Extracts a block's allocation state from its header */
static bool is_allocated(block_t *block) {
    return block->header & 1;
}

/** Gets the header corresponding to a given payload pointer */
static block_t *block_from_payload(void *ptr) {
    return ptr - offsetof(block_t, payload);
}

/**
 * Finds the first free block in the heap with at least the given size.
 * If no block is large enough, returns NULL.
 */
static block_t *find_fit(size_t size) {
    // Traverse the blocks in the heap using the explicit list
    // Only look at the free blocks
    if (head == NULL) {
        return NULL;
    }
    for (free_block_t *b = head; b != NULL; b = b->next) {
        block_t *bb = (block_t *) b;
        if (bb->header >= size) {
            remove_block(bb);
            return bb;
        }
    }
    return NULL;
}

/**
 * mm_init - Initializes the allocator state
 */
bool mm_init(void) {
    head = NULL;
    // We want the first payload to start at ALIGNMENT bytes from the start of the heap
    void *padding = mem_sbrk(ALIGNMENT - sizeof(block_t));
    if (padding == (void *) -1) {
        return false;
    }

    // Initialize the heap with no blocks
    mm_heap_first = NULL;
    mm_heap_last = NULL;
    return true;
}

// coalesce:
void coalesce_helper(block_t *left, block_t *right) {
    size_t left_size = get_size(left);
    size_t right_size = get_size(right);
    size_t add = left_size + right_size;

    // check if allocated
    bool left_allocated = is_allocated(left);
    bool right_allocated = is_allocated(right);

    if (!left_allocated && !right_allocated) {
        set_header(left, add, false); // set_header for left because we will remove right
        remove_block(right);
        // change mm_heap_last after removing the right block
        if (mm_heap_last == right) {
            mm_heap_last = left;
        }
    }
}

void coalesce(block_t *block) {
    // check =, <, and >
    size_t block_size = get_size(block);
    if (mm_heap_first == mm_heap_last) {
        return;
    }
    if (mm_heap_last > block) { // next block (right)
        // coalesce prev and curr
        block_t *next_block = (void *) block + block_size;
        coalesce_helper(block, next_block);
    }
    if (mm_heap_first < block) { // prev block (left)
        // coalesce curr and next
        footer_t *prev_footer = (void *) block - sizeof(footer_t);
        size_t prev_size = get_size((block_t *) prev_footer);
        block_t *prev_block = (void *) block - prev_size;
        coalesce_helper(prev_block, block);
    }
}

/**
 * mm_malloc - Allocates a block with the given size
 */
void *mm_malloc(size_t size) {
    if (size == 0) {
        return NULL;
    }

    // The block must have enough space for a header and be 16-byte aligned
    // make sure we now have size for the free blocks and the footer now

    size = round_up(sizeof(block_t) + size + sizeof(footer_t), ALIGNMENT);

    // If there is a large enough free block, use it
    block_t *block = find_fit(size);
    size_t new_size = get_size(block);
    if (block != NULL) {
        // Splitting:
        // follow logic of mm_implict, but add_list the splitted block
        if (new_size > size + sizeof(block_t) +
                           sizeof(size_t)) { // need to account for the block_2 header
            block_t *block_2 = (void *) block + size;
            size_t block_2_size = get_size(block) - size;

            set_header(block, size, true);
            set_header(block_2, block_2_size, false);
            add_block(block_2);

            if (block == mm_heap_last) {
                mm_heap_last = block_2;
            }

            return block->payload;
        }
        set_header(block, get_size(block), true);
        return block->payload;
    }

    // Otherwise, a new block needs to be allocated at the end of the heap
    block = mem_sbrk(size);
    if (block == (void *) -1) {
        return NULL;
    }

    // Update mm_heap_first and mm_heap_last since we extended the heap
    if (mm_heap_first == NULL) {
        mm_heap_first = block;
    }
    mm_heap_last = block;

    // Initialize the block with the allocated size
    set_header(block, size, true);
    return block->payload;
}

/**
 * mm_free - Releases a block to be reused for future allocations
 */
void mm_free(void *ptr) {
    // mm_free(NULL) does nothing
    if (ptr == NULL) {
        return;
    }

    // Mark the block as unallocated
    block_t *block = block_from_payload(ptr);
    set_header(block, get_size(block), false);
    add_block(block);
    coalesce(block);
}

/**
 * mm_realloc - Change the size of the block by mm_mallocing a new block,
 *      copying its data, and mm_freeing the old block.
 */
void *mm_realloc(void *old_ptr, size_t size) {
    if (old_ptr == NULL) {
        return mm_malloc(size);
    }
    else if (size == 0) {
        mm_free(old_ptr);
        return NULL;
    }
    void *new_ptr = mm_malloc(size);
    block_t *block = block_from_payload(old_ptr);
    size_t block_size = get_size(block) - sizeof(block_t) - sizeof(footer_t);
    block_t *new_block = block_from_payload(new_ptr);
    size_t new_size = get_size(new_block) - sizeof(block_t) - sizeof(footer_t);
    if (new_size < block_size) {
        memcpy(new_ptr, old_ptr, new_size); // don't want to copy over the header
    }
    else {
        memcpy(new_ptr, old_ptr, block_size); // changed from old_size to size
    }
    mm_free(old_ptr);
    return new_ptr;
}

/**
 * mm_calloc - Allocate the block and set it to zero.
 */
void *mm_calloc(size_t nmemb, size_t size) {
    size_t new_size = nmemb * size;
    void *ptr = mm_malloc(new_size);
    memset(ptr, 0, new_size);
    mm_free(ptr); // free the pointer
    return ptr;
}

/**
 * mm_checkheap - So simple, it doesn't need a checker!
 */
void mm_checkheap(void) {
}
