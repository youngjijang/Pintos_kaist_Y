#ifndef VM_ANON_H
#define VM_ANON_H
#include "vm/vm.h"
struct page;
enum vm_type;

struct anon_page {
    struct page_operations *stack_op; //?? 몰라 .. page type으로 스택을 식별?
    // struct list_elem swap_elem; 
};

void vm_anon_init (void);
bool anon_initializer (struct page *page, enum vm_type type, void *kva);

#endif
 