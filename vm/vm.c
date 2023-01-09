/* vm.c: Generic interface for virtual memory objects. */
#include <stddef.h>

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "vm/uninit.h"
#include "include/lib/kernel/hash.h"
#include "threads/vaddr.h"
#include "threads/mmu.h"

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

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {
	printf("vm_alloc_page_with_initializer: upage = %p\n", upage);
	ASSERT (VM_TYPE(type) != VM_UNINIT);
	struct supplemental_page_table *spt = &thread_current()->spt;
	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		struct page *initial_page = (struct page *)calloc(1, sizeof(struct page));
		initial_page->writable = writable;
		switch (type)
		{
		case VM_ANON:
			uninit_new(initial_page, upage, init, type, aux, anon_initializer);
			break;
		case VM_FILE:
			uninit_new(initial_page, upage, init, type, aux, file_backed_initializer);
			break;
		case 7:
			/* for setup_stack */
			initial_page->stack = 1;
			initial_page->va = upage;
			//uninit_new(initial_page, upage, init, type, aux, anon_initializer);
			break;
		default:
			break;
		}
		// TODO: 수정합시다.
		// uninit_new(initial_page, upage, init, type, aux, initial_page->uninit.page_initializer);
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
	frame->kva = palloc_get_page(PAL_USER | PAL_ZERO);

	if (frame->kva == NULL)
		PANIC("todo");

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);

	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	printf("vm_try_handle_fault: addr = %p\n", addr);
	struct supplemental_page_table *spt UNUSED = &thread_current()->spt;
	//bool succ = false;
	struct page *page = NULL;
	/* TODO: Your code goes here */

	/* TODO: Validate the fault */
	if (is_kernel_vaddr(addr))
		return false;
	// if (write)
	// 	return false;
	if (not_present)
	{
		page = spt_find_page(spt, addr);
		if (page == NULL)
			return false;
		if (!vm_do_claim_page (page))
			return false;
	}

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
	printf("vm_claim_page: addr = %p\n", page->va);

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
	printf("vm_do_claim_page: mapping fail\n");
	/* Set links */
	ASSERT (frame != NULL);
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	/* 스택의 경우 initializer가 없다. */

	if (pml4_get_page(t_curr->pml4, page->va) == NULL
		&& pml4_set_page(t_curr->pml4, page->va, frame->kva, page->writable))
	{
		printf("vm_do_claim_page: page->va = %p\n", page->va);
		printf("vm_do_claim_page: frame->kva = %p\n", frame->kva);
		if (page->stack)
			return true;
		return swap_in (page, page->uninit.aux);	// 여기서 uninit_initialize 호출된다.
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
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
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
