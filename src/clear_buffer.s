#include "asm_mac.i"

func clear_buffer
    move.l 4(%sp),%a0           | a0 = buffer
    lea 3584(%a0),%a0           | a0 = buffer end

    movm.l %d2-%d7/%a2-%a6,-(%sp)

    moveq #0,%d0
    move.l %d0,%d1
    move.l %d0,%d2
    move.l %d0,%d3
    move.l %d0,%d4
    move.l %d0,%d5
    move.l %d0,%d6
    move.l %d0,%d7
    move.l %d0,%a1
    move.l %d0,%a2
    move.l %d0,%a3
    move.l %d0,%a4
    move.l %d0,%a5
    move.l %d0,%a6

    // clear 1792 words
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)
    movm.l %d0-%d7/%a1-%a6,-(%a0)

    movm.l (%sp)+,%d2-%d7/%a2-%a6
    rts