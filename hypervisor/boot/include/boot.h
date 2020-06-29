#ifndef BOOT_H_
#define BOOT_H_

#include <types.h>

#ifndef ASSEMBLER

/* boot_regs store the multiboot info magic and address */
extern uint32_t boot_regs[2];

#endif	/* ASSEMBLER */

#endif /* BOOT_H_ */