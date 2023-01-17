/* vm.c: Generic interface for virtual memory objects. */
#include <stddef.h>
#include <string.h>

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "vm/uninit.h"
#include "lib/kernel/hash.h"
#include "threads/vaddr.h"
#include "threads/mmu.h"
#include "userprog/process.h"

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);
struct page *page_lookup (const void *address, struct supplemental_page_table* spt);
uint64_t va_hash(const struct hash_elem *e, void *aux);
bool va_less(const struct hash_elem *a, const struct hash_elem *b, void *aux);
void page_destructor (struct hash_elem*, void *);
struct page *spt_find_page (struct supplemental_page_table *spt, void *va);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT);
	struct supplemental_page_table *spt = &thread_current()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		struct page *initial_page = (struct page *)calloc(1, sizeof(struct page));
		typedef bool(*page_initializer)(struct page *, enum vm_type, void *);
		page_initializer initializer = NULL;

		switch (type)
		{
			case VM_ANON:
				initializer = anon_initializer;
				break;
			case VM_FILE:
				initializer = file_backed_initializer;
				break;
		}
		uninit_new(initial_page, upage, init, type, aux, initializer);
		initial_page->writable = writable;
		if (type == VM_STACK)
			initial_page->stack = true;
		/* TODO: Insert the page into the spt. */
		return spt_insert_page(spt, initial_page);
	}

err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	/* TODO: Fill this function. */
	struct page *page = page_lookup(pg_round_down(va), spt);
	return page;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	int succ = false;
	/* TODO: Fill this function. */
	if (hash_insert(spt->hash_page_table, &page->page_elem) == NULL)
		succ = true;
	return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	struct frame *frame = NULL;
	/* TODO: Fill this function. */
	/* TODO: NULL인 경우 핸들링 */
	frame = (struct frame *)calloc(1, sizeof(struct frame));
	frame->kva = palloc_get_page(PAL_USER);

	if (frame->kva == NULL)
		PANIC("todo");

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);

	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
	/* 4KB 페이지 추가 할당 또는 addr까지 할당 */
	void *pg_addr = pg_round_down(addr);
	ASSERT((uintptr_t)USER_STACK - (uintptr_t)pg_addr <= (1 << 20));

	while (vm_alloc_page(VM_STACK, pg_addr, true))
	{
		struct page *pg = spt_find_page(&thread_current()->spt, pg_addr);
		vm_claim_page(pg_addr);
		pg_addr += PGSIZE;
	}
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct thread *t_curr = thread_current();
	struct supplemental_page_table *spt UNUSED = &t_curr->spt;

	// /* save stack pointer for stack_growth */
	struct page *page = NULL;
	page = spt_find_page(spt, addr);
	/* TODO: Your code goes here */
	/* TODO: Validate the fault */
	if (is_kernel_vaddr(addr))
		return false;

	if (!addr)
		return false;
	uintptr_t rsp = user ? f->rsp : t_curr->stack_pointer;
	if (not_present) {	
		if (page == NULL) {
			if (write && (addr >= rsp - 8) && (addr < USER_STACK) && addr > STACK_LIMIT) {
				vm_stack_growth(addr);
				return true;
    		}
			else {
				return false;
			}
		}

		if (!vm_claim_page (addr)) {
			return false;
		}
	}
	
	if (write && !page->writable)
		return false;
	
	return true;
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function */
	page = spt_find_page(&thread_current()->spt, va);
	
	if(page == NULL)
		return false;
	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	ASSERT (page != NULL);

	struct frame *frame = vm_get_frame();
	struct thread * t_curr = thread_current();

	/* Set links */
	ASSERT (frame != NULL);
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	/* 스택의 경우 initializer가 없다. */
	if (pml4_get_page(t_curr->pml4, page->va) == NULL 
		&& pml4_set_page(t_curr->pml4, page->va, frame->kva, page->writable))
	{
		if (page->stack == true)
		{
			return true;
		}
		
		return swap_in (page, frame->kva);	// 여기서 uninit_initialize 호출된다.

	} else {
		printf("vm_do_claim_page: mapping fail\n");
	}

	return false;
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	/* Initialize the page table by using palloc_get_page to map physical memory */ 
	spt->hash_page_table = palloc_get_page(PAL_USER);
	hash_init(spt->hash_page_table, va_hash, va_less, NULL);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {

	if (dst == NULL || src == NULL)
		return false;

	size_t i;
	struct hash *parent_hash = src->hash_page_table;
	struct hash *child_hash = dst->hash_page_table;

	for (i = 0; i < parent_hash->bucket_cnt; i++) {
		struct list *bucket = &parent_hash->buckets[i];
		struct list_elem *elem, *next;

		for (elem = list_begin (bucket); elem != list_end (bucket); elem = next) {
			next = list_next (elem);

			/* copy parent page */
			struct page *parent_page = hash_entry(list_elem_to_hash_elem (elem), struct page, page_elem);
		
			/*  아직 lazy load segment가 안 된 얘들 */
			if(parent_page->operations->type == VM_UNINIT && !parent_page->stack) {	
				if (!vm_alloc_page_with_initializer(page_get_type(parent_page), parent_page->va,
						parent_page->writable, parent_page->uninit.init, parent_page->uninit.aux)) {
						return false;
					}
			}
			else {
				if (!vm_alloc_page(parent_page->uninit.type, parent_page->va, parent_page->writable))
					return false;
				if (!vm_claim_page(parent_page->va)) 
					return false;
				struct page* found_child = spt_find_page(dst, parent_page->va);
				
				memcpy(found_child->frame->kva, parent_page->frame->kva, PGSIZE);	/* 부모가 읽은 파일 내용을 자식에게 복사 */
			}
		}
	}
	return true;
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	if (spt->hash_page_table) {
		hash_clear(spt->hash_page_table, page_destructor);
	}
	else
		return;

}

/* project3 implements */

uint64_t 
va_hash(const struct hash_elem *e, void *aux) {
	struct page *p = hash_entry(e, struct page, page_elem);
	return hash_int(p->va);
}

bool 
va_less(const struct hash_elem *a, const struct hash_elem *b, void *aux) {
	// Returns true if A is less than B, or false if A is greater than or equal to B. 
	// hash_entry(HASH_ELEM, STRUCT, MEMBER) ((STRUCT *) ((uint8_t *) &(HASH_ELEM)->list_elem- offsetof (STRUCT, MEMBER.list_elem)))
	struct page *a_page = hash_entry(a, struct page, page_elem);	
	struct page *b_page = hash_entry(b, struct page, page_elem);

	return a_page->va < b_page->va;
}

/* Returns the page containing the given virtual address, or a null pointer if no such page exists. */
struct page *
page_lookup (const void *address, struct supplemental_page_table* spt) {
  struct page p;
  struct hash_elem *e;

  p.va = address;
  //spt->hash_page_table->aux = address;
  e = hash_find (spt->hash_page_table, &p.page_elem);
  return e != NULL ? hash_entry (e, struct page, page_elem) : NULL;
}

void page_destructor (struct hash_elem *hash_elem, void *aux) {
	spt_remove_page(&thread_current()->spt, hash_entry(hash_elem, struct page, page_elem));
}