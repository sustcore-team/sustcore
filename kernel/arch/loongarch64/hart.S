#include <arch/loongarch64/ctxlayout.h>

.macro locate_hart_context hart_id_reg, tmp
    li.d \tmp, HART_CONTEXT_SIZE
    mul.d \tmp, \hart_id_reg, \tmp
    la.local $tp, __hart_context
    add.d $tp, $tp, \tmp
.endm

.macro set_hart_id hart_id_reg
    st.d \hart_id_reg, $tp, 0
.endm
