/* leak-control.c - negative control for the sanitizer gate.
 *
 * This program MUST leak. If the leak check reports a leak here, the harness
 * works; if it reports a leak in dsaudio and not here, the check is broken.
 *
 *   cc -fsanitize=address -o leak-control tests/leak-control.c
 *   ./leak-control; echo $?
 */
#include <stdlib.h>

int main(void) {
    void *p = malloc(1024);   /* never freed */
    *(char *)p = 'x';
    return 0;
}
