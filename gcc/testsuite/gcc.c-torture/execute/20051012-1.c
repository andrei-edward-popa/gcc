/* { dg-additional-options "-std=gnu17" } */
/* { dg-require-effective-target untyped_assembly } */
extern void abort (void);

struct type 
{
  int *a;
  
  int b:16;
  unsigned int p:9;
} t;

unsigned int 
/* Match the actual call's arity, including on callee-pop ABIs.  */
foo (struct type unused)
{
  return t.p;
}

int 
main (void)
{
  t.p = 8;
  if (foo (t) != 8)
    abort ();
  return 0;
}
