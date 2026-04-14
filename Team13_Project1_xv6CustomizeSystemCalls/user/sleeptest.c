#include "kernel/types.h"
#include "user/user.h"

int main() {
    printf("Before sleep\n");
    sleep(50);
    printf("After sleep\n");
    exit(0);
}