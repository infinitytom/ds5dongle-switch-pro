#include "switch_test.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void)
{
    struct switch_test t={0};uint8_t silence[64]={0};int8_t samples[64];
    assert(!switch_test_active(&t,0));
    for(unsigned side=1;side<=3;side++) {
        assert(switch_test_begin(&t,side,600,100));
        assert(switch_test_active(&t,100)&&switch_test_remaining(&t,100)==600);
        assert(switch_test_active(&t,699)&&!switch_test_active(&t,700));
        assert(switch_test_remaining(&t,700)==0);
        /* The renderer timeout is refreshed only while the lease is active. */
        t.player.rumble_ms=650;switch_pro_render(&t.player,650,1,samples,32);
        unsigned count[2]={0};for(unsigned i=0;i<32;i++)for(unsigned c=0;c<2;c++)count[c]+=samples[i*2+c]!=0;
        assert((count[0]>0)==((side&1)!=0));assert((count[1]>0)==((side&2)!=0));
        struct switch_test before=t;
        assert(!switch_test_begin(&t,4,600,0)&&!memcmp(&t,&before,sizeof(t)));
        assert(!switch_test_begin(&t,1,99,0)&&!memcmp(&t,&before,sizeof(t)));
        assert(!switch_test_begin(&t,1,1001,0)&&!memcmp(&t,&before,sizeof(t)));
        assert(switch_test_begin(&t,0,0,0)&&!switch_test_active(&t,0));
        switch_pro_render(&t.player,0,1,samples,32);assert(!memcmp(samples,silence,64));
    }
    assert(switch_test_begin(&t,3,100,0xfffffff0u));
    assert(switch_test_active(&t,0x20)&&switch_test_remaining(&t,0x20)==52);
    assert(!switch_test_active(&t,0x54));
    puts("Switch local HD test: low-level packet decoding, stereo isolation, bounded lease, stop, invalid requests and clock wrap passed.");
    return 0;
}
