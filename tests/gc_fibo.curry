-- This test fills up the work queue with deterministic computations that each
-- trigger garbage collection.  It is intended to check for dangling pointers
-- in after the computation is split in two (when a choice reaches the top of a
-- computation.
fib n | n < 2 = n
      | n > 1 = fib (n-1) + fib (n-2)

main = (((fib 15 ? fib 14) ? fib 13) ? fib 12) ? fib 11

------ CORRECT ANSWER BELOW GENERATED BY cytest.py using pakcs ------
--> 144
--> 233
--> 377
--> 610
--> 89
