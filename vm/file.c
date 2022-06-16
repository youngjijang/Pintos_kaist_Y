/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"


static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void
vm_file_init (void) {
	hash_init(&thread_current()->mmap_hash,mapped_hash,mapped_less,NULL);
}

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Do the mmap 
	파일이 매핑된 가상 주소 반환
*/
void *
do_mmap (void *addr, size_t length, int writable,struct file *file, off_t offset) {

	struct thread* curr = thread_current();
	struct mmap_file *mmap_file = (struct mmap_file *)malloc(sizeof(struct mmap_file));
	list_init(&mmap_file->page_list);
	mmap_file->file = file;
	while (length > 0)
	{
		size_t page_read_bytes = length < PAGE_SIZE ? length : PAGE_SIZE;
		size_t page_zero_bytes = PAGE_SIZE - page_read_bytes;

		// printf("do_mmap\n");
		struct file_info *file_info = (struct file_info *)malloc(sizeof(struct file_info));
		file_info->file = file;
		file_info->page_read_bytes = page_read_bytes;
		file_info->page_zero_bytes = page_zero_bytes;
		file_info->ofs = offset; //key : 페이지 마다 파일을 찾아야하니까 ofs도 갱신해서 저장해주기!!!!!!

		// void *aux = file_info;
		if (!vm_alloc_page_with_initializer(VM_FILE, addr,
											writable, lazy_load_file, file_info))
			return NULL;

		struct page *p = spt_find_page(&curr->spt,addr);
		list_push_back (&mmap_file->page_list, &p->file.mmap_elem);
		 
		/* Advance. */
		length -= page_read_bytes;
		addr += PAGE_SIZE;
		offset += page_read_bytes;
	}
	struct hash_elem *e = hash_insert(&curr->mmap_hash,&mmap_file->hash_elem);
	ASSERT(e!=NULL);
	return mmap_file->va = mmap_file;
}

/* Do the munmap */
void
do_munmap (void *addr) {
	
}


static bool
lazy_load_file(struct page *page, void *aux)
{
	struct file_info *file_info = (struct file_info *)aux;
	struct file *file = file_info->file;
	off_t ofs = file_info->ofs;
	size_t page_read_bytes = file_info->page_read_bytes;
	size_t page_zero_bytes = PAGE_SIZE - page_read_bytes;

	if (page == NULL){
		return false;
	}
	/* Load this page. */
	if ((file_read_at(file,page->frame->kva,page_read_bytes,ofs)) != (int)page_read_bytes)
	{
		vm_dealloc_page(page);
		return false;
	}
	memset((page->frame->kva)+page_read_bytes, 0, page_zero_bytes); // 기록 - 여기 수정
	/* Add the page to the process's address space. */
	free(aux); //fork_read에서 터짐
	return true;
}