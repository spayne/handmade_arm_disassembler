.globl _start
_start:
mov r0, #0x2100
mov r0, r1
movs r0, r1
movs r0, #1
movs r0, #0x2100
mov r1, r0
str r1, [r2]   
strb r1, [r2]
str r1, [r2]
streq r1,[r2]
strne r1,[r2]
strcs r1,[r2]
strcc r1,[r2]
strmi r1,[r2]
strpl r1,[r2]
strvs r1,[r2]
strvc r1,[r2]
strhi r1,[r2]
strls r1,[r2]
strge r1,[r2]
strlt r1,[r2]
strgt r1,[r2]
strle r1,[r2]
str r1, [r2, #+0x123] 
str r1, [r2, #-0x123] 

strb r1, [r2]
strb r1, [r2, #0x123]
strb r1, [r2, #-0x123]

strb r1, [r2]!
strb r1, [r2, #0x123]!
strb r1, [r2, #-0x123]!

strb r1, [r2], #0x123
strb r1, [r2], #-0x123


adc r1,r2,#0x2
adc r1,r2,#0x2100
adcs r1,r2,#0x2

adcs r1,r2, LSL #1
adcs r1,r2, LSR #2
adcs r1,r2, ASR #3
adcs r1,r2, ROR #4
adcs r1,r2, RRX
adc r1,r2, RRX

adcs r1,r2, LSL r3
adc r1,r2, LSL r3

add r1,r2, #0x5
add r1,r2, #0x2100
adds r1,r2, #0x5

add r1,r2,r3,LSL #1
adds r1,r2,r3,LSL #1

add r1,r2,r3,ASR r4
adds r1,r2,r3,ASR r4

add r1,sp,#3
// 8.8.11
add r1,sp,r2, LSL #1
// 8.8.12
_label_before:
adr r1,_label_after
nop

// todo: enable this once I've impemented sub
// adr r1,_label_before
_label_after:

//  8.8.13
and r1,r2, #0x12

//  8.8.14
and r1,r2, r3, lsl #0x12

//  8.8.15
and r1,r2, r3, lsl r4

// 8.8.16
asr r1,r2,#1
// 8.8.17
asr r1,r2,r3

// 8.8.18
b _start
b _b_future
nop
_b_future:
nop

// 8.8.19
bfc r1, #0x1, #0x2

bfi r1, r2, #0x1, #0x2

bic r1,r2,#0x1
bics r1,r2,#0x1

// test arm modified immediate constants (todo)
bic r1,r2,#0x2100
bic r1,r2,r3, lsl #0x1
bic r1,r2,r3, lsl r4
bkpt #0x1234

here:
bl _start
blx _start
there:
blx here
// test instructions with 2 byte offsets
blx thumb1 
blx thumb2

blx r1
bx r1
bxj r1

cdp p0,#0,cr1,cr2,cr3,#4
cdp2 p0,#0,cr1,cr2,cr3,#4

clrex
clz r1,r2
cmn r1,#0x2100
cmn r1,r2,lsl #0x1
cmn r1,r2,lsl r3

cmp r1,#0x2100
cmp r1,r2, LSL #0x1
cmp r1,r2,lsl r3

cps #1
cps #2
cps #3
cps #4

cpsie a,#0
cpsie a,#1
cpsie a,#31
cpsie ai,#1
cpsie ia,#1
cpsie aif,#1
cpsie fia,#1
cpsie ai,#1
cpsie ia,#1
cpsie aif,#1
cpsie fia,#1
cpsie ai,#1
cpsie ia,#1
cpsie aif,#1
cpsie fia,#1
cpsid a,#1
cpsid ai,#1
cpsid ia,#1
cpsid aif,#1
cpsid fia,#1

dbg #15
dmb sy
dmb st 
dmb ish 
dmb ishst 
dmb nsh 
dmb nshst 
dmb osh 
dmb oshst 
dmbge oshst 

.code 16
thumb1:
nop
thumb2:
nop

