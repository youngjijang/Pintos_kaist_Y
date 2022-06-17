#ifndef VM_FILE_H
#define VM_FILE_H
#include "filesys/file.h"
#include "vm/vm.h"
#include "kernel/hash.h"

#define PGBITS  12                         /* Number of offset bits. */
#define PAGE_SIZE  (1 << PGBITS)  

struct page;
enum vm_type;

struct file_page {
};

/*mmap_file*/
struct mmap_file {
	void *va; //mmap() 성공시 리턴된 mapping id
	struct file* file; //매핑되는 파일 오브젝트
	struct hash_elem hash_elem; //mmap_file들의 리스트 연결을 위한 구조체
	struct list page_list; // mmap_file에 해당하는 모든 page들의 리스트
};

void vm_file_init (void);
bool file_backed_initializer (struct page *page, enum vm_type type, void *kva);
void *do_mmap(void *addr, size_t length, int writable,
		struct file *file, off_t offset);
void do_munmap (void *va);
static bool lazy_load_file(struct page *page, void *aux);
#endif
