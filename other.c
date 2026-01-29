#include <stdio.h>

// Function: func1
// Called with: call test2.func1 def456
int func1(int argc, char *argv[]) {
    printf("=== func1() from other.so called ===\n");
    printf("Received %d arguments:\n", argc);
    
    for (int i = 0; i < argc; i++) {
        printf("  Parameter %d: %s\n", i, argv[i]);
    }
    
    printf("=== func1() completed ===\n\n");
    return 0;
}