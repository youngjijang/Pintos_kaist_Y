/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "../include/userprog/process.h"

#define FISIZE (1 << 12)
static struct disk *file_disk;

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
	// file_disk = disk_get(0, 1);
}

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	
	// struct uninit_page *uninit = &page->uninit;
	// memset(uninit, 0, sizeof(struct uninit_page));

	/* Set up the handler */
	page->operations = &file_ops;
	struct file_page *file_page = &page->file;

	return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;

	if (page == NULL)
		return false;

	struct file *file = ((struct file_info *)page->file_inf)->file;
	off_t offset = ((struct file_info *)page->file_inf)->ofs;
	size_t page_read_bytes = ((struct file_info *)page->file_inf)->page_read_bytes;
	size_t page_zero_bytes = FISIZE - page_read_bytes;

	file_seek(file, offset); // file의 오프셋을 offset으로 바꾼다. 이제 offset부터 읽기 시작한다.

	off_t read_off = file_read(file, kva, page_read_bytes);
	if (read_off != (int)page_read_bytes)
	{
		// palloc_free_page(page->frame->kva);
		return false;
	}

	memset(kva + page_read_bytes, 0, page_zero_bytes);

	return true;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;

	if (page == NULL)
		return false;

	if (pml4_is_dirty(thread_current()->pml4, page->va)){
		file_write_at(page->file_inf->file, page->va, page->file_inf->page_read_bytes, page->file_inf->ofs);
		pml4_set_dirty(thread_current()->pml4, page->va, 0);
	}
	pml4_clear_page(thread_current()->pml4, page->va);
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;

	struct file_info *f_info = (struct file_info *)page->uninit.aux;
	if (pml4_is_dirty(thread_current()->pml4, page->va)){
		file_write_at(f_info->file, page->va, f_info->page_read_bytes, f_info->ofs);
		pml4_set_dirty(thread_current()->pml4, page->va, 0);
	}
}

/* Do the mmap 
	파일이 매핑된 가상 주소 반환
*/
void *
do_mmap (void *addr, size_t length, int writable,struct file *file, off_t offset) {

	struct file *m_file = file_reopen(file);
	//각 매핑에 대한 파일에 대한 개별적이고 독립적인 참조를 얻으려면 reopen 함수를 사용해야 합니다.
	// struct file *m_file = file_open(file);
	struct thread* curr = thread_current();
	struct mmap_file *mmap_file = (struct mmap_file *)malloc(sizeof(struct mmap_file));
	void *addr_ = addr;
	list_init(&mmap_file->page_list);
	mmap_file->file = m_file;
	size_t read_bytes = length > file_length(file) ? file_length(file) : length;
  	size_t zero_bytes = PAGE_SIZE - read_bytes % PAGE_SIZE;
	while (read_bytes > 0 || zero_bytes > 0) {
		size_t page_read_bytes = read_bytes < PAGE_SIZE ? read_bytes : PAGE_SIZE;
		size_t page_zero_bytes = PAGE_SIZE - page_read_bytes;

		// printf("do_mmap\n");
		struct file_info *file_info = (struct file_info *)malloc(sizeof(struct file_info));
		file_info->file = m_file;
		file_info->page_read_bytes = page_read_bytes;
		file_info->page_zero_bytes = page_zero_bytes;
		file_info->ofs = offset; //key : 페이지 마다 파일을 찾아야하니까 ofs도 갱신해서 저장해주기!!!!!!

		// void *aux = file_info;
		if (!vm_alloc_page_with_initializer(VM_FILE, addr,
											writable, lazy_load_file, file_info))
			return NULL;

		struct page *p = spt_find_page(&curr->spt,addr);
		// printf("page : %d\n\n",p);
		// printf("@@file : %d ofs : %d read byte : %d zero : %d\n\n",f->file,f->ofs,page_read_bytes,page_zero_bytes);
		if (p == NULL)
			return NULL;
			
		list_push_back (&mmap_file->page_list, &p->mmap_elem);
		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		addr += PAGE_SIZE;
		offset += page_read_bytes;
	}
	// struct hash_elem *e = hash_insert(&curr->mmap_hash,&mmap_file->hash_elem);
	// ASSERT(e!=NULL);
	// printf("rrrrrrrrrrrr\n\n");
	return addr_;
}

/* Do the munmap */
void
do_munmap (void *addr) {
	// printf("들어온다다아아아아아아아아아ㅏ아아아아\n\n");
	while(true){
		struct page* page = spt_find_page(&thread_current()->spt, addr);
		// printf("page : %d\n\n",page);
		if (page == NULL)
			break;
		
		struct file_info *f_info = (struct file_info *)page->uninit.aux;

		/* 수정된 페이지(더티 비트 1)는 파일에 업데이트 해 놓는다. 
		   그리고 더티 비트를 0으로 만들어둔다. */
		if (pml4_is_dirty(thread_current()->pml4, page->va)){
			file_write_at(f_info->file, addr, f_info->page_read_bytes, f_info->ofs);
			pml4_set_dirty(thread_current()->pml4, page->va, 0);
		}
		
		/* present bit을 0으로 만든다. */
		pml4_clear_page(thread_current()->pml4, page->va);
		addr += PAGE_SIZE;

		free(f_info); //맞나??
	}

}


static bool
lazy_load_file(struct page *page, void *aux)
{
	// printf("안녕여여여여여여ㅕ여영\n\n");
	struct file_info *file_info = (struct file_info *)aux;
	struct file *file = file_info->file;
	off_t ofs = file_info->ofs;
	size_t page_read_bytes = file_info->page_read_bytes;
	size_t page_zero_bytes = PAGE_SIZE - page_read_bytes;
	// printf("로드....page : %d ofs : %d read byte : %d zero : %d\n\n",file,ofs,page_read_bytes,page_zero_bytes);
	if (page == NULL){
		return false;
	}
	/* Load this page. */
	file_seek(file, ofs);
	// printf("됨\n");
	// (file_read_at(file,page->frame->kva,page_read_bytes,ofs))
	if (file_read(file, page->frame->kva, page_read_bytes) != (int)page_read_bytes)
	{	
		vm_dealloc_page(page);
		return false;
	}
	// printf("안녕여여여여여여ㅕ여영\n\n");
	memset((page->frame->kva)+page_read_bytes, 0, page_zero_bytes); // 기록 - 여기 수정
	/* Add the page to the process's address space. */
	
	return true;
}