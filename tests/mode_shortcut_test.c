#include <assert.h>
#include "mode_shortcut.h"
static void hold(struct mode_shortcut *s, int target, uint64_t start) {
    for (uint64_t t=start;t<start+2000000;t+=100000)
        assert(mode_shortcut_update(s,true,true,target,t)==-1);
    assert(mode_shortcut_update(s,true,true,target,start+2000000)==target);
    assert(mode_shortcut_update(s,true,true,target,start+2100000)==-1);
}
int main(void) {
    const uint8_t keys[] = {0x28,0x48,0x18,0x88,0,2,4};
    for (int i=0;i<256;i++) {
        int expected=-1;
        for (int j=0;j<7;j++) if(i==keys[j]) expected=j;
        assert(mode_shortcut_target((uint8_t)i)==expected);
    }
    for (int mode=0;mode<7;mode++) {
        struct mode_shortcut s = {0};
        for (uint64_t t=1;t<3000000;t+=100000)
            assert(mode_shortcut_update(&s,true,true,mode,t)==-1);
        assert(mode_shortcut_update(&s,false,false,-1,3000000)==-1);
        hold(&s,mode,3100000);
        assert(mode_shortcut_update(&s,false,true,mode,5300000)==-1);
        assert(mode_shortcut_update(&s,true,true,mode,5400000)==-1);
        assert(mode_shortcut_update(&s,false,false,-1,5500000)==-1);
        assert(mode_shortcut_update(&s,true,true,mode,5600000)==-1);
        assert(mode_shortcut_update(&s,true,true,mode,8000000)==-1);
        assert(mode_shortcut_update(&s,true,true,mode,8100000)==-1);
    }
    struct mode_shortcut s = {0};
    assert(mode_shortcut_update(&s,false,false,-1,1)==-1);
    for(uint64_t t=100000;t<2000000;t+=100000)
        assert(mode_shortcut_update(&s,true,true,0,t)==-1);
    hold(&s,5,2000000); /* Changing target restarts the full hold. */
    assert(mode_shortcut_update(&s,false,false,-1,4200000)==-1);
    assert(mode_shortcut_update(&s,true,true,3,4300000)==-1);
    assert(mode_shortcut_update(&s,true,true,-1,4400000)==-1);
    hold(&s,3,4500000); /* Invalid selection also restarts the hold. */
    return 0;
}
