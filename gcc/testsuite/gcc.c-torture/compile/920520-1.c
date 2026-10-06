/* { dg-do compile } */
/* { dg-skip-if "no scalar floating-point register operand" { stm8-*-* } } */
/* { dg-skip-if "" { pdp11-*-* } } */
/* { dg-additional-options "-std=gnu89" } */

f(){asm("%0"::"r"(1.5F));}g(){asm("%0"::"r"(1.5));}
