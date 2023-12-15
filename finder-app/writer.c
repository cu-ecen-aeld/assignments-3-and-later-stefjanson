#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("./writer writefile writestr\n");
        return 1;
    }

    const char *writefile = argv[1];
    const char *writestr = argv[2];

    FILE *file = fopen(writefile, "w");
    if (file == NULL) {
        printf("File could not be created\n");
        return 1;
    }

    fprintf(file, "%s", writestr);
    fclose(file);

    return 0;
}