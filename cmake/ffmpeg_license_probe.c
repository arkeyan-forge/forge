/* Configure-time probe: prints the licence and the build configuration compiled into libavcodec. */
#include <libavcodec/avcodec.h>
#include <stdio.h>

int main(void)
{
    printf("license=%s\n", avcodec_license());
    printf("configuration=%s\n", avcodec_configuration());
    return 0;
}
