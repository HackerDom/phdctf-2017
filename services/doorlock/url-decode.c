#include <stdio.h>
#include <string.h>

void url_decode(char *s) {
    int n = strlen(s);
    int src = 0, dst = 0;
    while (src < n) {
        if (s[src] == '%' && src+2 < n) {
            s[dst] = ((s[src+1]-'0')<<4) | (s[src+2]-'0');
            src += 3;
        }
        else {
            s[dst] = s[src];
            src += 1;
        }
        dst++;
    }
    s[dst] = 0;
}

int main(int argc, char ** argv) {
    if (argc != 2) {
        printf("need argument\n");
        return 1;
    }

    char s[64];
    memset(s, 0, sizeof(s));
    strcpy(s, argv[1]);

    printf("s = %s\n", s);
    url_decode(s);
    printf("s = %s\n", s);

    return 0;
}
