#include <stdio.h>

int main(int argc, char **argv) {
    int n = (argc>1) ? atoi(argv[1]) : 10000;
    FILE *fp = fopen("send.txt", "w");
    for(int i=0;i<n;i++) fprintf(fp, "Word %d\n", i+1);
    fclose(fp);
    return 0;
}