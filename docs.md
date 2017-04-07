# Documentation

Really just a bunch of notes to myself right now as I work.

## Principles

### Predictability

Two cases where we favor explicitness and predictability: tail calls
and multiple-argument-functions.

For tail calls, we prefer to introduce a new keyword that makes the
call tail-recursive. This makes it easier to see when a function will
be consuming stack vs. when it won't be.

For multiple-argument-functions, we prefer to pass tuples
explicitly. This makes it easier to see when a closure will or will
not be allocated.

The argument for the other side in both of these decisions is that
this is optimization work the compiler could do for you. But these
optimizations make such a big difference in the performance of a
program that it's very important to be able to control these things.

The nature of a compiler-driven optimization is that it's silent and
invisible to the semantics of the program. This means they're
*fragile*-- a small change to the program may cause your large
optimization to no longer work, and your program may actually fail as
a result.

(Actually, now that I think about it, tail-call-optimization is *not*
an optimization, since it is semantic bearing, and controls whether a
loop may safely run forever or not. Even more reason to be explicit.)
