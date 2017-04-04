# Millie
*A practical little functional language*

Millie is a small, strict functional language in the style of the ML
family of languages.

Millie was started because of tweets:

<blockquote class="twitter-tweet" data-lang="en">
  <p lang="en" dir="ltr">
  I wish there were a well-supported, simple, easy to build,
  ML-descended static functional language. No shenanigans, like (e.g.)
  Lua.
  </p>
  &mdash; Gary Bernhardt (@garybernhardt)
  <a href="https://twitter.com/garybernhardt/status/841430367772930048">
  March 13, 2017
  </a>
</blockquote>
<script async src="//platform.twitter.com/widgets.js" charset="utf-8"></script>
<blockquote class="twitter-tweet" data-lang="en">
  <p lang="en" dir="ltr">
  The Lua repo is 35 .c files (21 KLOC), 26 .h files (4 KLOC), one
  makefile, and one &quot;bugs&quot; file. Nothing else; no
  directories. It&#39;s glorious.
  </p>
  &mdash; Gary Bernhardt (@garybernhardt)
  <a href="https://twitter.com/garybernhardt/status/841430971555561472">
    March 13, 2017
  </a>
</blockquote>
<script async src="//platform.twitter.com/widgets.js" charset="utf-8"></script>

Within that framework, Millie aims to be *useful* and *practical*, and is
intended to be a good language for doing software engineering both in
the large and in the small.

Some of Millie's priorities are:

- Generating *fast* code. In particular, this means that Millie
  provides good control over the memory layout of your data
  structures, so that you can build cache-aware algorithms, and
  providing good control over allocations, so that you can avoid
  spending time in the garbage collector.

- Generating code *fast*. In particular, I don't know why
  Damas-Hindley-Milner-derived type systems need to be slow as
  molasses. Compiler speed is a high priority.

## Building Millie

To build Millie on OS X, run

    ./build.sh

The current Millie implementation is built in C99 and so in theory
requires only the standard library and a modern C compiler, but I've
only actually tested it on OS X.

The test harness currently requires Python 3. To run the tests, run

    ./test.py

from the root of the project.

## Project State

Just started. Basic constructs exist and can be executed. The only
numeric type is a 64-bit integer. There are no composite types
yet. There is no garbage collector yet, so your programs will run out
of memory. There is no native code generation yet.
