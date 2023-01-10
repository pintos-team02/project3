/* vm.c: Generic interface for virtual memory objects. */
#include <stddef.h>

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "vm/uninit.h"
#include "include/lib/kernel/hash.h"
#include "threads/vaddr.h"
#include "threads/mmu.h"
#include <stdio.h>
/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init(void) {
	vm_anon_init();
	vm_file_init();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init();
#endif
	register_inspect_intr();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
	page_get_type(struct page *page) {
	int ty = VM_TYPE(page->operations->type);
	switch (ty) {
	case VM_UNINIT:
		return VM_TYPE(page->uninit.type);
	default:
		return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim(void);
static bool vm_do_claim_page(struct page *page);
static struct frame *vm_evict_frame(void);
struct page *page_lookup(const void *address, struct supplemental_page_table* spt);
uint64_t va_hash(const struct hash_elem *e, void *aux);
bool va_less(const struct hash_elem *a, const struct hash_elem *b, void *aux);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer(enum vm_type type, void *upage, bool writable,
	vm_initializer *init, void *aux) {
	ASSERT(VM_TYPE(type) != VM_UNINIT);

	struct supplemental_page_table *spt = &thread_current()->spt;

	struct page *initial_page = spt_find_page(spt, upage);
	/* Check whether the upage is already occupied or not. */
	if (initial_page == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */

		 /* TODO: Insert the page into the spt. */
		initial_page = (struct page *)malloc(sizeof(struct page));

		switch (type)
		{
		case VM_ANON:
			uninit_new(initial_page, upage, init, VM_ANON, aux, anon_initializer);
			break;
		case VM_FILE:
			uninit_new(initial_page, upage, init, VM_FILE, aux, file_backed_initializer);
			break;
		default:
			break;
		}

		return spt_insert_page(spt, initial_page);
	}
	else {

		return true;
	}

err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
	spt_find_page(struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	/* TODO: Fill this function. */
	struct page *p = page_lookup(va, spt);
	return p;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page(struct supplemental_page_table *spt UNUSED,
	struct page *page UNUSED) {
	int succ = false;
	/* TODO: Fill this function. */
	if (hash_insert(spt->hash_page_table, &page->page_elem) == NULL)
		succ = true;
	return succ;
}

void
spt_remove_page(struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page(page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim(void) {
	struct frame *victim = NULL;
	/* TODO: The policy for eviction is up to you. */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame(void) {
	struct frame *victim UNUSED = vm_get_victim();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame(void) {
	struct frame *frame = (struct frame*)malloc(sizeof(struct frame));
	/* TODO: Fill this function. */
	frame->kva = palloc_get_page(PAL_USER);
	frame->page = NULL;
	/*TODO: NULL인 경우 핸들링 */
	ASSERT(frame != NULL);
	ASSERT(frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth(void *addr UNUSED) {
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp(struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault(struct intr_frame *f UNUSED, void *addr UNUSED,
	bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	if (user == false && addr == NULL) { // 어떻게 해야할지 모르겠다
		return true;
	}
	if (user && not_present) {
		if ((addr == NULL || is_kernel_vaddr(addr))) {
			return false;
		}
		/* TODO: Your code goes here */
		// page = spt_find_page(spt, (uint64_t)addr & ~0xFFF);
		// if (page == NULL) {
		// 	page = (struct page*)malloc(sizeof(struct page));
		// 	page->va = (uint64_t)addr & ~0xFFF;
		// 	printf("try_handle_fault!\n");

		return vm_claim_page(addr);

	}
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page(struct page *page) {
	destroy(page);
	free(page);
}

/* Claim the page that allocate on VA.*/
// You will first need to get a page and then calls vm_do_claim_page with the page.
bool
vm_claim_page(void *va UNUSED) {
	struct thread *t_curr = thread_current();
	struct supplemental_page_table *spt UNUSED = &thread_current()->spt;
	struct page *page = spt_find_page(spt, (uint64_t)va & ~0xFFF);
	/* TODO: Fill this function */
	// va에 해당하는 page 연결해주기, spt에서 찾아보고 없을 때 생성해서 연결해주기
	if (page == NULL) {
		page = (struct page *)malloc(sizeof(struct page));
		page->va = (uint64_t)va & ~0xFFF;
		if (vm_do_claim_page(page))
			return spt_insert_page(spt, page);
		else
			return false;

	}
	// printf("my type: %d\n", page->uninit.type);
	return vm_do_claim_page(page);

}
//페이지+프레임 처리?
/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page(struct page *page) {
	struct frame *frame = vm_get_frame();
	struct thread *t_curr = thread_current();

	struct supplemental_page_table *spt UNUSED = &thread_current()->spt;
	/* Set links */
	frame->page = page;
	page->frame = frame;
	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	pml4_set_page(t_curr->pml4, page->va, frame->kva, true);
	page_get_type(page);
	return swap_in(page, frame->kva);// if anon page -> call lazy_load_segment
}



/* Initialize new supplemental page table */
void
supplemental_page_table_init(struct supplemental_page_table *spt UNUSED) {
	spt->hash_page_table = palloc_get_page(PAL_USER);
	hash_init(spt->hash_page_table, va_hash, va_less, NULL);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy(struct supplemental_page_table *dst UNUSED,
	struct supplemental_page_table *src UNUSED) {
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill(struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
}

/* project3 implements */
uint64_t
va_hash(const struct hash_elem *e, void *aux) {
	struct page *p = hash_entry(e, struct page, page_elem);
	uint64_t a = hash_int(p->va);
	// printf("hash : %d, %p\n", a, p->va);
	return a;
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
	page_lookup(const void *address, struct supplemental_page_table* spt) {
	struct page p;
	struct hash_elem *e;

	p.va = address;
	e = hash_find(spt->hash_page_table, &p.page_elem);
	return e != NULL ? hash_entry(e, struct page, page_elem) : NULL;
}
