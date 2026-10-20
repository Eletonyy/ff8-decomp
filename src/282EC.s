.set noat
.set noreorder

.section .text
.align 2

.globl func_80037AEC
.type func_80037AEC, @function
func_80037AEC:
    lw $t0, 0x4($a0)
    nop
    addu $t1, $t0, $zero
    andi $t0, $t0, 0xFFFF
    addu $t0, $t0, $a0
    srl $t1, $t1, 16
    addu $t1, $t1, $a0
    addu $v0, $zero, $zero
.L80037B0C:
    lh $v1, 0x0($t0)
    addiu $t0, $t0, 0x2
    andi $v1, $v1, 0x7FFF
    sh $v1, 0x0($a1)
    addiu $a1, $a1, 0x2
    sltu $t2, $t0, $t1
    bnez $t2, .L80037B0C
    addi $v0, $v0, 0x1
    addiu $a1, $a1, 0x3
    addiu $at, $zero, -0x4
    and $a1, $a1, $at
    sw $a1, 0x0($a2)
    jr $ra
    nop
.size func_80037AEC, . - func_80037AEC

.globl func_80037B44
.type func_80037B44, @function
func_80037B44:
    lw $v0, 0x4($a0)
    nop
    addu $v1, $v0, $zero
    andi $v0, $v0, 0xFFFF
    addu $v0, $v0, $a0
    srl $v1, $v1, 16
    addu $v1, $v1, $a0
    addu $a2, $zero, $zero
.L80037B64:
    addiu $v0, $v0, 0x2
    sltu $a1, $v0, $v1
    bnez $a1, .L80037B64
    addiu $a2, $a2, 0x1
    jr $ra
    nop
.size func_80037B44, . - func_80037B44

.globl func_80037B7C
.type func_80037B7C, @function
func_80037B7C:
    lw $v0, 0x0($a0)
    nop
    lui $at, (0xFF000000 >> 16)
    and $a3, $v0, $at
    beqz $a3, .L80037BA0
    nop
    sll $v1, $v0, 8
    sra $v1, $v1, 8
    srl $v0, $v0, 24
.L80037BA0:
    sw $v0, 0x0($a1)
    sw $v1, 0x0($a2)
    jr $ra
    nop
.size func_80037B7C, . - func_80037B7C
