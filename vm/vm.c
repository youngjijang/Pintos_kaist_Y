/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/mmu.h"
#include "vm/uninit.h"
#include "vm/file.h"
#include "userprog/process.h"

struct mmap_file;

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void vm_init(void)
{
	vm_anon_init();
	vm_file_init();
#ifdef EFILESYS /* For project 4 */
	pagecache_init();
#endif
	register_inspect_intr();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */

	//1번 - frame talble 추가
	list_init(&frame_table);
	list_init(&swap_table);
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type(struct page *page)
{
	int ty = VM_TYPE(page->operations->type);
	switch (ty)
	{
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

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`.
 * 
 * 커널이 새로운 페이지 요청을 받게되면 불린다. 
 * 페이지 구조체를 할당하고 페이지 종류에 따라 적절한 초기화를 진행한다.
 * 
 * load_segment가 실행되며, 해당 스레드의 spt를 활용해, 각 page가 필요한 시점에 물리메모리에
 * load 될 수 있도록 처리해주어야 한다. 
 * 커널이 새 페이지 요청을 수신할 때 호출됩니다. 이니셜라이저는 페이지 구조를 할당하고 페이지 유형에 따라 
 * 적절한 이니셜라이저를 설정하여 새 페이지를 초기화하고 컨트롤을 다시 사용자 프로그램으로 반환합니다.
 *  사용자 프로그램이 실행되면서 어느 시점에서 프로그램이 소유하고 있다고 생각하지만 페이지에 아직 내용이 없는 페이지에 액세스하려고 하기 때문에 페이지 폴트가 발생합니다
 *  */
bool vm_alloc_page_with_initializer(enum vm_type type, void *upage, bool writable,
									vm_initializer *init, void *aux)
{
	
	ASSERT(VM_TYPE(type) != VM_UNINIT);

	struct supplemental_page_table *spt = &thread_current()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page(spt, upage) == NULL)
	{
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		typedef bool (*initializeFunc)(struct page*, enum vm_type, void *);
		initializeFunc initializer = NULL;

		struct page* page = (struct page*)malloc(sizeof(struct page));
		switch(VM_TYPE(type)){
			case VM_ANON:
			// case VM_ANON|VM_MARKER_0:
				initializer = anon_initializer;
				break;
			case VM_FILE:
				initializer = file_backed_initializer;
				break;
		}

		uninit_new(page, upage, init, type, aux, initializer);

		page->writable = writable;
		// page->page_cnt = -1;
		/* TODO: Insert the page into the spt. */
		return spt_insert_page(spt,page);
	}
err:
	return false;
}

/* 1번 추가 - 이게맞아?
	Find VA from spt and return page. On error, return NULL.
	주어진 spt에서 va에 해당하는 struct page를 찾는다.
 */
struct page *
spt_find_page(struct supplemental_page_table *spt UNUSED, void *va UNUSED)
{
	//struct page *page = NULL;
	/* TODO: Fill this function. */
	struct page *page = (struct page *)malloc(sizeof(struct page));
	struct hash_elem *e;
	page->va = pg_round_down(va);
	e = hash_find(&spt->spt_hash, &page->hash_elem);

	free(page);
	return e != NULL ? hash_entry(e, struct page, hash_elem) : NULL;
}

/*
	1번 추가 - 불확실
	Insert PAGE into spt with validation.ß
   struct page를 주어진 spt에 넣는다.
   이 함수는 가상 주소가 주어진 Spt에 없는지 확인해야 한다.
 */
bool spt_insert_page(struct supplemental_page_table *spt UNUSED,
					 struct page *page UNUSED)
{
	int succ = false;
	/* TODO: Fill this function. */
	// if (spt_find_page(spt, page->va) == NULL)
	// {
	// 	return succ;
	// }
	if(hash_insert(&spt->spt_hash, &page->hash_elem) == NULL){
		succ = true;
	}
	return succ;
}

void spt_remove_page(struct supplemental_page_table *spt, struct page *page)
{
	vm_dealloc_page(page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim(void)
{
	struct frame *victim = NULL;
	/* TODO: The policy for eviction is up to you. */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame(void)
{
	struct frame *victim UNUSED = vm_get_victim();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

/*
 * 1번 - 추가 물리메모리 할당받는 함수???
 * palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.
 * palloc_get_page를 통해 유저풀에서 새로운 물리 페이지를 할당 받는다. user pool에서 할당받는데 성공했다면 frame을 할당 받고 멤버를 초기화 하고 반환한다.
 * vm_get_frame을 구현한 이후에는 모든 유저 스페이스의 페이지들을 이 함수를 통해 할당 받아야 한다.
 * page allocation에 실패한 경우 swap out을 구현할 필요는 없다 PANIC(”todo”)로 마킹만 해 두고 나중에 구현하자.
 * */
static struct frame *
vm_get_frame(void)
{
	/* TODO: Fill this function. */
	struct frame *frame = malloc(sizeof(struct frame)); //프레임 생성 - 커널영역에 할당
	frame->kva = palloc_get_page(PAL_USER); //페이지를 할당 받아서 연결???
	
	if(frame->kva == NULL)
		PANIC("todo");
		
	list_push_back(&frame_table,&frame->frame_elem);

	frame->page = NULL; //추가
	
	ASSERT(frame != NULL);
	ASSERT(frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth(void *addr UNUSED)
{
	// printf("들어온다????????????? addr : %p\n\n", addr);
	if(vm_alloc_page(VM_ANON | VM_MARKER_0,addr,true)){
		vm_claim_page(addr);
		thread_current()->stack_bottom -= PGSIZE;
	}
	
	// // exit(-1);
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp(struct page *page UNUSED)
{
}

/* Return true on success 
page fault 발생시 불림
 유효한 페이지폴트인지 확인 (유효한 page fault == 잘못된 접근)
 만약 유효한 페이지폴트가 아니라면  페이지에 필요한 내용을 불러오고 유저 프로그램으로 제어를 넘겨주어야 한다. 

 스택 증가를 식별한다. 식별 후 vm_stack_growth를 호출하여 스택을 증가ㅅ시켜야한디ㅏ. 
 스택 성장에 유효한 경우인지 확인 -> 스택 증가로 결함을 처리할 수 있음을 확인한 경우 결함이 있는 주소로 vm_stack_growth 
*/
bool vm_try_handle_fault(struct intr_frame *f UNUSED, void *addr UNUSED,
						 bool user UNUSED, bool write UNUSED, bool not_present UNUSED)
{

	struct supplemental_page_table *spt UNUSED = &thread_current()->spt;
	struct page *page = spt_find_page(spt,addr);
	// puts("모르겠는데?");
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	void* stack_bottom = (void *)(((uint8_t *)thread_current()->stack_bottom) - PGSIZE);
	// printf("rsp : %p\n adr: %p\n",f->rsp,rsp);
	if (page && not_present){ //수정 - not_present 추가 
		// printf("lazy\n");
		return vm_do_claim_page(page);
	}
	else if ( not_present && write && (f->rsp-8 == addr) && stack_bottom > USER_STACK - 0X100000){
		// printf("grow\n");
		vm_stack_growth(stack_bottom);
		return true;
	}
	else {
		// printf("실패\n\n");
		return false;
	}
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void vm_dealloc_page(struct page *page)
{
	destroy(page);
	free(page);
}

/* 1번 추가 페이지랑 물리메모리 매핑?
	Claim the PAGE and set up the mmu.
	va를 할당할 페이지를 claim한다.
	페이지를 얻어오고 vm_do_claim_page를 수행한다.
 */
bool vm_claim_page(void *va UNUSED)
{
	/* TODO: Fill this function */
	// printf("page fualt wwwwwww: %p\n",va);
	struct page *page = spt_find_page(&thread_current()->spt,va);
	if (page == NULL)
		return false;
	return vm_do_claim_page(page);
}

/* 1번 추가
 	Claim the page that allocate on VA.
	페이지 claim(물리 프레임을 할당 받는 것을 말한다.)
	vm_get_frame을 통해 프레임을 얻어와서 MMU를 설정해준더=ㅏ.
	가상 주소에서 물리 주소로의 매핑을 페이지 테이블에 추가한다. 성공/실패
*/
static bool
vm_do_claim_page(struct page *page)
{
	struct frame *frame = vm_get_frame();
	
	/* Set links */
	frame->page = page; 
	page->frame = frame;

	struct thread *t = thread_current();
	if (!(pml4_get_page(t->pml4, page->va) == NULL && pml4_set_page(t->pml4, page->va, frame->kva, page->writable)))
    	return false;
	// printf("여기?????: page : %p //// %p\n",page, frame->kva);
	// 페이지 테이블에 물리 주소와 가상주소를 맵핑 시켜주는 함수(install_page)
	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	return swap_in(page, frame->kva); 
}

/* Initialize new supplemental page table
 1번 추가
 spt 초기화를 수행한다.
 spt를 위한 자료구조를 선택한다. init, do_fork()에서 호출된다.
 */
void supplemental_page_table_init(struct supplemental_page_table *spt UNUSED)
{
	hash_init(&spt->spt_hash, page_hash, page_less, NULL);
}

/* Copy supplemental page table from src to dst
 scr에서 dst로 spt 복사
 */
bool supplemental_page_table_copy(struct supplemental_page_table *dst UNUSED,
								  struct supplemental_page_table *src UNUSED)
{
	struct hash *dst_h = &dst->spt_hash;
	struct hash *src_h = &src->spt_hash;
	struct hash_iterator i;
	hash_first(&i, src_h);
	while (hash_next(&i))
	{
		struct page *p = hash_entry(hash_cur (&i), struct page, hash_elem);
		enum vm_type type = p->operations->type;
		void *addr = p->va;
		bool writable = p->writable;
		struct page *n_p;
		struct file_info *file_info;
		switch (VM_TYPE(type))
		{
		case VM_UNINIT : 
			//lazy load가 되기 위해 기다리는 페이지 만들기
			file_info = (struct file_info *)malloc(sizeof(struct file_info));
			memcpy(file_info, (struct file_info *)p->uninit.aux,sizeof(struct file_info));
			vm_alloc_page_with_initializer(VM_ANON, addr, writable, p->uninit.init,file_info); //혜진 수정 - type uninit으로 하면 assert에 걸림
			break;
		case VM_ANON :
			//setup_stack 과 비슷하게 처리 
			if(!(vm_alloc_page(type,addr,writable)))
				return false;
			n_p = spt_find_page(dst,addr);
			if(!(vm_claim_page(addr)))
				return false;
			memcpy(n_p->frame->kva,p->frame->kva,PGSIZE);
			break;
		case VM_FILE :
			//둘이 먼차이? 
			if(!(vm_alloc_page(type,addr,writable)))
				return false;
			n_p = spt_find_page(dst,addr);
			if(!(vm_claim_page(addr)))
				return false;
			memcpy(n_p->frame->kva,p->frame->kva,PGSIZE);
			break;
		}
	}
	return true;
}

/* Free the resource hold by the supplemental page table */
void supplemental_page_table_kill(struct supplemental_page_table *spt UNUSED)
{
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	hash_destroy(&spt->spt_hash, page_destroy);
}

/* Returns a hash value for page p.
addr을 키로 사용하는 해시함수 */
unsigned
page_hash(const struct hash_elem *p_, void *aux UNUSED)
{
	const struct page *p = hash_entry(p_, struct page, hash_elem);
	return hash_bytes(&p->va, sizeof p->va);
}

/* Returns true if page a precedes page b. 
addr을 키로 사용하는 비교함수
*/
bool page_less(const struct hash_elem *a_,
			   const struct hash_elem *b_, void *aux UNUSED)
{
	const struct page *a = hash_entry(a_, struct page, hash_elem);
	const struct page *b = hash_entry(b_, struct page, hash_elem);

	return a->va < b->va;
}

/*
	hash_destroy()에 두 번째 인자로 전달할 함수
	hash_elem로 page를 찾아 page를 destroy
*/
void *page_destroy(struct hash_elem *h_elem, void *aux UNUSED){
	struct page *p = hash_entry(h_elem, struct page, hash_elem);
	vm_dealloc_page(p);
}


// //mapped_hash
// /* Returns a hash value for page p.
// addr을 키로 사용하는 해시함수 */
// unsigned
// mapped_hash(const struct hash_elem *f_, void *aux UNUSED)
// {
// 	const struct mmap_file *mpfile = hash_entry(f_, struct mmap_file, hash_elem);
// 	return hash_bytes(&mpfile->va, sizeof mpfile->va);
// }

// /* Returns true if page a precedes page b. 
// addr을 키로 사용하는 비교함수
// */
// bool mapped_less(const struct hash_elem *a_,
// 			   const struct hash_elem *b_, void *aux UNUSED)
// {
// 	const struct mmap_file *a = hash_entry(a_, struct mmap_file, hash_elem);
// 	const struct mmap_file *b = hash_entry(b_, struct mmap_file, hash_elem);

// 	return a->va < b->va;
// }
