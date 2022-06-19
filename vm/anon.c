/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"
#include "lib/kernel/bitmap.h"

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);

int swap_cnt;
struct bitmap *swap_table;
// const size_t SECTORS_PER_PAGE = PAGE_SIZE / DISK_SECTOR_SIZE;
// 한페이지에 들어갈수있는 disk sector의 개수 - 하나의 페이지를 저장하기위한 디스트 섹터의 공간
#define SECTOR_PAGE_SIZE 8

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

/* Initialize the data for anonymous pages */
void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */
	swap_disk = disk_get(1, 1); //디스크에서 swap을 위한 영역 받아오기
	swap_cnt = (int)(disk_size(swap_disk) / SECTOR_PAGE_SIZE); // 스왑영역에 가용한 페이지수 계산하기
	swap_table = bitmap_create(swap_cnt); //swap 영역에 가용한 페이지 수 계산하기
}

/* Initialize the file mapping 

*/
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	
	/* page struct 안의 Union 영역은 현재 uninit page이다.
		 ANON page를 초기화해주기 위해 해당 데이터를 모두 0으로 초기화해준다.
		 Q. 이렇게 하면 Union 영역은 모두 다 0으로 초기화되나? -> 맞다. */
	struct uninit_page *uninit = &page->uninit;
	memset(uninit, 0, sizeof(struct uninit_page));
	
	/* Set up the handler */ 
	page->operations = &anon_ops;
	struct anon_page *anon_page = &page->anon;
	anon_page->swap_index = -1; //초기 물리메모리 위에 올라와있기 때문에 -1

	return true;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;

	int swap_sec = anon_page->swap_index;
	// int swap_slot_idx = swap_sec / 8;
	// // printf("swap_sec: %d, swap_slot_idx: %d\n", swap_sec, swap_slot_idx);
	if (bitmap_test(swap_table, swap_sec) == 0)
		// return false;
		PANIC("(anon swap in) Frame not stored in the swap slot!\n");

	// page->frame->kva = kva;



	// // ASSERT(is_writable(kva) != false);

	// // disk_read is done per sector; repeat until one page is read
	for (int sec = 0; sec < 8; sec++)
		disk_read(swap_disk, ( swap_sec*8 ) + sec, kva + DISK_SECTOR_SIZE * sec);

	bitmap_set(swap_table, swap_sec, 0);

	// // restore vaddr connection
	// pml4_set_page(thread_current()->pml4, page->va, kva, true); // writable true, as we are writing into the frame

	// anon_page->swap_index = -1;
	return true;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;

	// // Find free slot in swap disk
	// // Need at least PGSIZE to store frame into the slot
	// // size_t free_idx = bitmap_scan(swap_table, 0, SECTORS_IN_PAGE, 0);
	size_t free_idx = bitmap_scan(swap_table, 0, 1, false);

	if (free_idx == BITMAP_ERROR)
		PANIC("(anon swap-out) No more free swap slots!\n");

	int swap_sec = free_idx * 8;

	// // disk_write is done per sector; repeat until one page is written
	for (int sec = 0; sec < 8; sec++)
		disk_write(swap_disk, swap_sec + sec, page->va + DISK_SECTOR_SIZE * sec);

	// // access to page now generates fault
	bitmap_set(swap_table,free_idx,true);
	pml4_clear_page(thread_current()->pml4, page->va);

	anon_page->swap_index = swap_sec;

	// // page->frame->page = NULL;
	// // page->frame = NULL;

	return true;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;
}
