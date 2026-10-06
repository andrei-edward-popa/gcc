/* { dg-skip-if "no 64-bit register group" { stm8-*-* } } */
/* PR c/123365 */

void
foo ()
{
  __asm__ volatile ("" : "+r" ((long long[]) { 0 }));
}
