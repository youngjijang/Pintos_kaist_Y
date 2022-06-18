#ifndef VM_VM_H
#define VM_VM_H
#include <stdbool.h>
#include <list.h>
#include "threads/palloc.h"
#include "kernel/hash.h"

#define STACK_SIZE 0Xf4240

enum vm_type {
	/* page not initialized */
	VM_UNINIT = 0,
	/* page not related to the file, aka anonymous page */
	VM_ANON = 1, // 스왑영역으로 부터 데이터 로드
	/* page that realated to the file */
	VM_FILE = 2, // 매핑된 파일로부터 데이터 로드
	/* page that hold the page cache, for project 4 */
	VM_PAGE_CACHE = 3, 

	/* Bit flags to store state */

	/* Auxillary bit flag marker for store information. You can add more
	 * markers, until the value is fit in the int. */
	VM_MARKER_0 = (1 << 3),
	VM_MARKER_1 = (1 << 4),

	/* DO NOT EXCEED THIS VALUE. */
	VM_MARKER_END = (1 << 31),
};

#include "vm/uninit.h"
#include "vm/anon.h"
#include "vm/file.h"
#ifdef EFILESYS
#include "filesys/page_cache.h"
#endif

struct page_operations;
struct thread;

#define VM_TYPE(type) ((type) & 7)

/* The representation of "page".
 * This is kind of "parent class", which has four "child class"es, which are
 * uninit_page, file_page, anon_page, and page cache (project4).
 * DO NOT REMOVE/MODIFY PREDEFINED MEMBER OF THIS STRUCTURE. */
struct page {
	const struct page_operations *operations;
	void *va;              /* Address in terms of user space - 가상주소..데이터가 시작되는 가상 주소의 시작점*/
	struct frame *frame;   /* Back reference for frame - 물리 주소*/

	/* Your implementation */

	struct hash_elem hash_elem; /* 추가 - Hash table element. */
	bool writable; //쓰기 권한 추가?? pml4_set_page 때 사용

	struct file_info *file_inf;

	struct list_elem mmap_elem;
	/* Per-type data are binded into the union.
	 * Each function automatically detects the current union */
	union {
		struct uninit_page uninit;
		struct anon_page anon;
		struct file_page file;
#ifdef EFILESYS
		struct page_cache page_cache;
#endif
	};
};


/* The representation of "frame" 
	1번 - 추가해도된다. 
*/
struct frame {
	void *kva; //커널주소
	struct page *page;

	struct list_elem frame_elem;
}; // 물리메모리를 나타내는????

/* The function table for page operations.
 * This is one way of implementing "interface" in C.
 * Put the table of "method" into the struct's member, and
 * call it whenever you needed. */
struct page_operations {
	bool (*swap_in) (struct page *, void *);
	bool (*swap_out) (struct page *);
	void (*destroy) (struct page *);
	enum vm_type type;
};

#define swap_in(page, v) (page)->operations->swap_in ((page), v)
#define swap_out(page) (page)->operations->swap_out (page)
#define destroy(page) \
	if ((page)->operations->destroy) (page)->operations->destroy (page)

/* Representation of current process's memory space.
 * We don't want to force you to obey any specific design for this struct.
 * All designs up to you for this. 
 * 
 * 페이지 폴트를 처리하고 리소스 관리를 하기위해 spt로 추가 정보를 들고있어야한다. 
 * 
 * */
struct supplemental_page_table { //페이지 테이블
	struct hash spt_hash;
};

#include "threads/thread.h"
void supplemental_page_table_init (struct supplemental_page_table *spt); //1번
bool supplemental_page_table_copy (struct supplemental_page_table *dst,
		struct supplemental_page_table *src);
void supplemental_page_table_kill (struct supplemental_page_table *spt);
struct page *spt_find_page (struct supplemental_page_table *spt,
		void *va); //1번
bool spt_insert_page (struct supplemental_page_table *spt, struct page *page); //1번
void spt_remove_page (struct supplemental_page_table *spt, struct page *page);

void vm_init (void);
bool vm_try_handle_fault (struct intr_frame *f, void *addr, bool user,
		bool write, bool not_present);

#define vm_alloc_page(type, upage, writable) \
	vm_alloc_page_with_initializer ((type), (upage), (writable), NULL, NULL)
bool vm_alloc_page_with_initializer (enum vm_type type, void *upage,
		bool writable, vm_initializer *init, void *aux);
void vm_dealloc_page (struct page *page);
bool vm_claim_page (void *va);
enum vm_type page_get_type (struct page *page);

//추가//////////
unsigned page_hash (const struct hash_elem *p_, void *aux UNUSED);
bool page_less (const struct hash_elem *a_,
           const struct hash_elem *b_, void *aux UNUSED);

unsigned mapped_hash(const struct hash_elem *f_, void *aux UNUSED);
bool mapped_less(const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);

void *page_destroy(struct hash_elem *h_elem, void *aux UNUSED);

struct list frame_table; // 프로그램 끝날때까지 유지
struct list_elem *clock_elem;


struct file_info
{
	struct file *file;
	off_t ofs;
	size_t page_read_bytes;
	size_t page_zero_bytes;
	//bool is_loaded;
};


#endif  /* VM_VM_H */
