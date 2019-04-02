#include <sys/socket.h>
#include <netinet/in.h>

#define LENGTH 32

typedef struct list{
    char val[LENGTH];
    struct list* next;
} list_t;

//if element is presented, 1. Otherwise, 0
short find_item(list_t * head, char* val){
    if(head -> next == NULL) return 0;
    list_t * current = head -> next; 
    int equals = 1;

    while(((equals = strcmp(current -> val, val)) != 0) && current->next != NULL){
        current = current->next;
    }

    if(equals == 0) return 1;
    return 0;

}

void push(list_t * head, char* val) {
    if(find_item(head, val)) return;

    list_t * current = head;
    
    while (current->next != NULL) {
        current = current->next;
    }

    current->next = malloc(sizeof(list_t));
    strcpy(current->next->val, val);
    current->next->next = NULL;
}



