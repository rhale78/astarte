# Astarte Programming Language

> **Original implementation by Dr. Karl Abrahamson** (karl@cs.ecu.edu)  
> East Carolina University

---

## Table of Contents

1. [What is Astarte?](#what-is-astarte)
2. [Installation](#installation)
3. [Commands and Usage](#commands-and-usage)
   - [Compiling with `astc`](#compiling-with-astc)
   - [Running with `astr`](#running-with-astr)
4. [Language Overview](#language-overview)
   - [Package Structure](#package-structure)
   - [Syntax Basics](#syntax-basics)
   - [Comments](#comments)
   - [Type System](#type-system)
5. [Programming Paradigms](#programming-paradigms)
   - [Functional Programming](#functional-programming)
   - [Logic Programming](#logic-programming)
   - [Procedural Programming](#procedural-programming)
   - [Object-Oriented Programming](#object-oriented-programming)
   - [Concurrent / Multithreaded Programming](#concurrent--multithreaded-programming)
6. [Sample Programs](#sample-programs)
7. [Standard Library](#standard-library)
8. [Troubleshooting](#troubleshooting)
9. [Copyright and License](#copyright-and-license)

---

## What is Astarte?

Astarte is a **multiparadigm programming language** created by **Dr. Karl Abrahamson** at East Carolina University. It supports multithreaded functional, logic, procedural, and object-oriented programming paradigms — with particularly strong support for functional programming.

Astarte is a typed language with a **polymorphic type system** and supports **type inference**, meaning you can write programs without specifying type information and the compiler will infer it for you.

Key features include:

- **Multiparadigm**: seamlessly blend functional, logic, procedural, and object-oriented styles
- **Polymorphic type system** with full type inference
- **Pattern matching** with arithmetic patterns
- **Lazy evaluation** and infinite data structures
- **Higher-order functions** and currying
- **Multithreading** and or-parallel logic programming
- **Backtracking** and failure handling
- **Garbage-collected** runtime

The implementation is a compiler (`astc`) that produces bytecode, and an interpreter (`astr`) that executes the bytecode. Both tools run on Unix/Linux systems. The source code is written in C.

Full online documentation is available at: http://www.cs.ecu.edu/astarte

---

## Installation

### Prerequisites

- A Unix/Linux system
- A C compiler (the GNU C compiler, `gcc`, is recommended; ANSI C support is required)
- `make`
- Optionally: `bison` and `flex` (for rebuilding the parser/lexer)

### Step-by-Step Instructions

1. **Choose an installation directory.** The suggested location is `/usr/local/lib/astarte`. Move (or copy) this distribution to that directory. It should contain the subdirectories:
   ```
   ast/
   htdoc/
   messages/
   src/
   test/
   ```

2. **Configure the base directory** (if you chose a directory other than `/usr/local/lib/astarte`):  
   Edit `src/misc/config.h` and update the definition of `BASE_DIR` to the directory containing this README and the subdirectories listed above.

3. **Configure the Makefile:**  
   Edit `src/exec/Makefile` and update the following variables if needed:
   - `TOPDIR` — the `src` directory (e.g., `/usr/local/lib/astarte/src`)
   - `BINDIR` — where the binaries should be installed (e.g., `/usr/local/bin`)
   - `CC` — name of your C compiler
   - `SHELL`, `RM`, `MV`, `TOUCH` — system utilities

   Make sure `BINDIR` is in your `PATH`.

4. **Build the compiler and interpreter:**
   ```sh
   cd src/exec
   make install
   ```

   > **Note:** If `make` tries to run `bison` or `flex` and you don't have them, run:
   > ```sh
   > cd src/exec
   > touch parser.c lexer.c
   > ```
   > This updates their modification times so `make` will not try to regenerate them.

5. **Build the standard library:**
   ```sh
   cd ast
   make all
   ```
   You may see some warnings; they can be safely ignored.

6. **Run the tests (optional):**
   ```sh
   cd test
   make all
   ```
   There will be output. Some complaints or mismatches are expected and can be ignored. You should **not** see a compiler or interpreter abort or core dump.

7. **Clean up (optional):** After a successful installation, you can delete the `src` and `test` directories. The `ast` and `messages` directories must be kept — they are used at runtime.

### Troubleshooting Installation

| Problem | Solution |
|---------|----------|
| `make` tries to rebuild `parser.c`/`lexer.c` and you don't have `bison`/`flex` | `cd src/exec && touch parser.c lexer.c` |
| `make` terminates prematurely | Restart `make` |
| Link error: `alloca` does not exist | In `src/misc/unixs.h`, comment out the line `#define USE_ALLOCA` |
| Interpreter complains about running out of memory | In `src/misc/options.h`, comment out the `#include "unixs.h"` line and include `unixl.h` or `unixm.h` instead. Then rebuild: `cd src/exec && make clean && make install` |

If problems persist, contact Dr. Karl Abrahamson at karl@cs.ecu.edu.

---

## Commands and Usage

### Compiling with `astc`

Astarte source files use the `.ast` extension. To compile `myprog.ast` into bytecode `myprog.aso`:

```sh
astc myprog
```

#### Most-Used `astc` Options

| Option | Description |
|--------|-------------|
| `--help` | Print a synopsis of command options |
| `-i`, `--no-check-indent` | Suppress indentation warnings |
| `-k`, `--no-check-exhaustive` | Suppress warnings about non-exhaustive patterns |
| `-l`, `--no-listing` | Do not print a listing to standard output |
| `-s`, `--show` | Dump type inference results on a type error |
| `-1` | Stop at the first declaration that causes an error |

#### Less-Used `astc` Options

| Option | Description |
|--------|-------------|
| `-a`, `--report-imports` | Show reports of definitions (including imports, excluding standard) |
| `-A`, `--Report-imports` | Show reports of all definitions including the standard package |
| `-d`, `--abbreviated-listing` | Suppress full listing, but show reported definitions |
| `-e`, `--expert` | Use short error messages |
| `-e<file>` | Append error messages to `<file>` |
| `-E<file>` | Write error messages to `<file>` (overwriting) |
| `-f`, `--full-listing` | Full listing (interface + implementation) |
| `-F`, `--Full-listing` | Very full listing (includes all imported files) |
| `-I<dir>`, `--import<dir>` | Add `<dir>` to the import search path |
| `-L<file>`, `--listing<file>` | Write listing to `<file>` |
| `-nw`, `--no-warn` | Suppress all warnings |
| `-w`, `--warn` | Override any suppressed warnings |
| `-pN` | Allow pattern match/expand substitutions to depth N |

#### Import Search Path

The compiler searches the current directory and the standard library directory. To add directories, use `-I<dir>` or set the environment variable `AST_IMPORTS` to a colon-separated list of directories. Use `$` to refer to the standard library directory:

```sh
setenv AST_IMPORTS .:~/astarte:$
```

---

### Running with `astr`

After compilation, run the program with `astr`:

```sh
astr myprog
```

The interpreter will ask if you want to compile a file that is out of date.

Command-line arguments to your program follow the program name:

```sh
astr -t myprog -f
```

Here, `-t` is an option to `astr`, and `myprog` runs with `commandLine = ["myprog", "-f"]`.

#### Most-Used `astr` Options

| Option | Description |
|--------|-------------|
| `--help` | Print a synopsis of command options |
| `-d`, `--debug-at-trap` | Start the debugger when an exception is trapped |
| `-i<file>`, `--stdin<file>` | Redirect standard input from `<file>` |
| `-m`, `--make` | Compile out-of-date files without asking |
| `-o<file>`, `--stdout<file>` | Redirect standard output to `<file>` |
| `-o!<file>`, `--stdout!<file>` | Redirect standard output, overwriting without prompting |
| `-t`, `--no-optimize` | Disable tail recursion optimization (useful for debugging) |
| `-v`, `--debug` | Enter the debugger on startup |

#### Less-Used `astr` Options

| Option | Description |
|--------|-------------|
| `-e<file>`, `--stderr<file>` | Redirect standard error to `<file>` |
| `-f`, `--no-check` | Do not query about out-of-date files; load silently |
| `-hN`, `--heap-limitN` | Set the soft heap limit to N kilobytes |
| `-h+`, `--heap-limit+` | Remove the heap soft limit entirely |
| `-n`, `--no-run` | Only load and link; do not run |
| `-pN`, `--precisionN` | Set initial floating-point precision to N digits |
| `--profile` | Produce a crude execution profile |
| `-rtN`, `--dump-sizeN` | Set dump size limit to N kilobytes |
| `-sN`, `--stack-sizeN` | Set the soft limit on stack frames to N |
| `-s+`, `--stack-size+` | Remove the stack soft limit entirely |

#### Import Path for `astr`

Set `AST_PATH` to a colon-separated list of directories to control where `astr` looks for `.aso` files:

```sh
setenv AST_PATH .:/usr/local/lib/astarte/ast
```

---

## Language Overview

### Package Structure

Every Astarte program lives in a **package**. A package has the form:

```astarte
Package MyPackage

  %% declarations go here

%Package
```

A typical package imports libraries, defines functions and types, and uses `Execute` declarations to run code.

### Syntax Basics

- **Bracketing:** Most constructs begin with a capitalized keyword (e.g., `If`, `While`, `Define`) and end with the same keyword preceded by `%` (e.g., `%If`, `%While`, `%Define`). There can be no space between `%` and the keyword.
- **Period shortcut:** A `.` (period) can be used as a wildcard end marker, but only when it occurs on the same line as the matching begin keyword.
- **Semicolons:** A `;` inside a declaration stands for `%W W` (ending and restarting the same construct), making it easy to chain definitions:
  ```astarte
  Define
    x = 1;
    y = 2
  %Define
  ```

### Comments

```astarte
%% This is a short comment (from %% to end of line)

<<<<<<<<<<<<<<<<<<<<<<<<<
This is a long comment.
It spans multiple lines.
>>>>>>>>>>>>>>>>>>>>>>>>>

======================================
%% A row of four or more = signs is also a comment (between declarations)
```

Inside a long comment, you can temporarily exit by using `%>` at the start of a line:

```astarte
<<<<<<<<<<<<<<<<
This is a comment.
%> Define foo = 42.    %% this line is actual code
Back to a comment.
>>>>>>>>>>>>>>>>
```

### Type System

Astarte uses a rich, statically-checked, polymorphic type system with full type inference.

**Species** (concrete types): `Natural`, `Integer`, `Rational`, `Real`, `Boolean`, `String`, `Char`

**Parameterized species** (families): `List(T)`, `(A, B)` (ordered pairs), `A -> B` (function types)

**Data abstractions** (genera): sets of related species, e.g.:
- `REAL` — contains `Natural`, `Integer`, `Rational`, `Real`
- `RRING` — contains `Integer`, `Rational`, `Real`
- `ORDER` — species that support comparison operators

**Type variables**: Written as `` `a ``, `` `b ``, or constrained as `REAL`a`, `ORDER`v`, etc.

Examples:

| Type | Meaning |
|------|---------|
| `Natural` | Non-negative integers |
| `[Natural]` | List of natural numbers |
| `` `a -> `a `` | Polymorphic identity function |
| `` [`a] -> `a `` | Polymorphic list-to-element function |
| `` REAL`a -> REAL`a `` | Numeric function, works on any real-valued species |
| `` (ORDER`a, ORDER`a) -> ORDER`a `` | Maximum of two ordered values |

Type inference means you generally **do not need to write type annotations** — the compiler figures out types automatically.

---

## Programming Paradigms

### Functional Programming

Astarte supports pure functional programming, where computation is performed by applying functions to values, with no side effects.

#### Defining Functions

A function with a single equation:

```astarte
Define sqr(?x) = x^2.
```

> **Note:** Parameters are prefixed with `?` in the function heading (to mark them as being "solved for"), but not in the body.

A function with multiple cases and guards:

```astarte
Define max by
  case max(?x, ?y) = x   when x >= y
  else max(?, ?y)  = y
%Define
```

#### Recursion

```astarte
Define factorial by
  case factorial(0)    = 1
  case factorial(?n+1) = (n+1) * factorial(n)
%Define
```

The pattern `?n+1` performs arithmetic pattern matching: to compute `factorial(4)`, the equation `n+1 = 4` is solved, giving `n = 3`.

#### List Operations

```astarte
%% Sum the members of a list
Define sum by
  case sum []        = 0
  case sum (?h :: ?t) = h + sum(t)
%Define

%% Reverse a list
Define rev by
  case rev []        = []
  case rev (?h :: ?t) = rev(t) ++ [h]
%Define
```

#### Higher-Order Functions

```astarte
%% Apply f to every element of a list
Define map by
  case map(?f, [])       = []
  case map(?f, ?h :: ?t) = f(h) :: map(f, t)
%Define
```

#### Curried Functions

Functions can be curried (taking arguments one at a time):

```astarte
Define add ?x ?y = x + y.

Let inc = add 1.    %% inc is a function that adds 1
```

#### List Comprehensions

```astarte
%% Squares of 1 to 10
[: x^2 | Let x = each(1 _upto_ 10). :]

%% Primes up to 20
[: x | Let x = each(1 _upto_ 20). {prime?(x)} :]
```

#### Quicksort (higher-order)

```astarte
Define quicksort by
  case quicksort ? []              = []
  case quicksort ?cmp (?pivot :: ?t) =
       quicksort cmp [: x | Let x = each t. {cmp(x, pivot)} :]
    ++ [pivot]
    ++ quicksort cmp [: x | Let x = each t. {not cmp(x, pivot)} :]
%Define

Define sort = quicksort(<).    %% sort ascending
```

#### Lazy Evaluation

Lazily evaluated expressions and infinite lists:

```astarte
%% Deferred expression (evaluated only when needed)
Define lazySqr(?x) = (:x*x:).

%% Infinite list: nats(n) = [n, n+1, n+2, ...]
Define nats(?n) = (:n :: nats(n+1):).

%% Alternative using Await then
Define nats by
  Await then
    case nats(?n) = n :: nats(n+1)
  %Await
%Define
```

---

### Logic Programming

Astarte supports Prolog-style logic programming with backtracking and unification. Import the logic package:

```astarte
Import "logic/logic".
```

#### Predicates

A predicate is a function that succeeds or fails (returns `()` or fails):

```astarte
Define Odd(?x). = {odd?(x)}.
```

#### Defining Facts and Rules

Facts are written with `<-` (implication, read backwards):

```astarte
Define Parent by all
  case Parent(?x, ?y). <- Mother(x, y).
  case Parent(?x, ?y). <- Father(x, y).
%Define

Define Mother by all
  case Mother("minnie", "joan"). <- ()
  case Mother("joan", "curly").  <- ()
  case Mother("joan", "larry").  <- ()
%Define
```

#### Recursive Rules

```astarte
Define Ancestor by
  case Ancestor(?x, ?y). <- Parent(x, y).
  case Ancestor(?x, ?y). <- Var{logic} z.
                             Parent(x, z).
                             Ancestor(z, y).
%Define
```

#### Unknowns and Unification

Create logic unknowns (variables to be bound by unification):

```astarte
Var{logic} who.
Grandparent("joan", who).    %% Finds who = "larry"
```

#### Logic Programming with Lists

```astarte
%% Append predicate
Define Append by all
  case Append(nil,     ?a, ?a).       <- ()
  case Append(?h :: ?t, ?a, ?h :: ?r). <- Append(t, a, r).
%Define

%% Remove prefix y from list x (blending functional and logic styles)
Define ?x _lminus_ ?y =
  Var{logic} result.
  Append(y, result, x).
  Value result.
%Define
```

#### Cuts

Use `Cut` to prune the search tree:

```astarte
Define Member by all
  CutHere
    case Member(?x, ?x :: ?y). <- Cut.
    case Member(?x, ?a :: ?y). <- Member(x, y).
  %CutHere
%Define
```

#### Or-Parallel Execution

Use `all mixed` to try branches in parallel:

```astarte
Define Member by all mixed
  case Member(?x, ?x :: ?y). <- ()
  case Member(?x, ?a :: ?y). <- Member(x, y).
%Define
```

---

### Procedural Programming

Astarte supports imperative/procedural programming with loops, mutable variables (boxes), and assignment.

#### Name Rebinding

```astarte
Define factorial ?n =
  Let r = 1.
  Let k = 0.
  While k < n do
    Relet k = k + 1.
    Relet r = r * k.
  %While
  Value r.
%Define
```

#### Procedures

A procedure is a function that returns `()`. By convention, procedure names start with a capital letter:

```astarte
Define PrintFactorials ?n. =
  Let k = 1.
  While k <= n do
    Writeln[$(factorial k)].
    Relet k = k + 1.
  %While
%Define

PrintFactorials 20.
```

#### Boxes (Mutable Variables)

A **box** is a mutable cell whose content can be changed. Use `@` to read the content:

```astarte
Var x!.              %% create a box named x!
Assign x! := 5.      %% set its content to 5
Writeln[$(@x!)].     %% print its content (5)
```

Factorial using boxes:

```astarte
Define factorial ?n =
  Var r!, k!.
  Assign r! := 1.
  Assign k! := 0.
  While @k! < n do
    Assign k! := @k! + 1.
    Assign r! := @r! * @k!.
  %While
  Value @r!.
%Define
```

#### Arrays

An array is a list of boxes. Subscript with `#`:

```astarte
Var arr(n).               %% create array of size n
Assign arr#0 := 42.       %% set element 0
Let v = @(arr#0).         %% read element 0
Let lst = contents(arr).  %% get all elements as a list
```

Insertion sort:

```astarte
Define Insert(?arr, ?k). =
  Let i = k--1.
  While i > 0 _and_ @(arr#i) > @(arr#(i+1)) do
    Swap(arr#i, arr#(i+1)).
    Relet i = i--1.
  %While
%Define

Define Sort ?arr. =
  For ?i from 2 _upto_ length(arr) do
    Insert(arr, i).
  %For
%Define
```

---

### Object-Oriented Programming

Astarte includes support for object-based and object-oriented programming through **contexts**, **classes**, and the type hierarchy.

- **Contexts** encapsulate state and provide an object-like interface.
- **Classes** support inheritance through the `Bring` declaration.
- The **species/family/genus/community** hierarchy models a class hierarchy with polymorphism.

Example: creating a simple counter using a box and a context:

```astarte
%% A simple counter object
Define makeCounter() =
  Var count!.
  Assign count! := 0.
  Value count!.
%Define

Execute
  Let c = makeCounter().
  Assign c := @c + 1.
  Writeln[$(@c)].   %% prints 1
%Execute
```

The type system also supports **extension declarations** to add operations to existing abstractions, and **bring declarations** for elementary inheritance.

---

### Concurrent / Multithreaded Programming

Astarte supports multithreaded computation. Multiple threads can run simultaneously, and Astarte provides mechanisms for creating, synchronizing, and terminating threads.

- **Fork**: Entering a backtracking expression or using `all mixed` can create multiple concurrent threads.
- **Thread management**: The `threadlib` library provides functions to manage threads.
- **Or-parallel logic programming**: Different cases of a logic predicate can be pursued in parallel.

Import the thread library:

```astarte
Import "thread/threadlib".
```

---

## Sample Programs

### Hello, World

```astarte
Package Hello

Execute
  Writeln["Hello, World!"].
%Execute

%Package
```

### Factorial Table

```astarte
Package FactTable

Import "collect/string".

Define factorial by
  case factorial(0)    = 1
  case factorial(?n+1) = (n+1) * factorial(n)
%Define

Define getN() =
  Assume n: Natural.
  Writeln["I will write a table of factorials"].
  Write["How high should the table go? "].
  Extract[$(?n)] from stdin!.
  Value n.
%Define

Execute
  Let n = getN().
  Writeln["  k        factorial(k)"].
  For ?k from 0 _upto_ n do
    Writeln[k $$ 3, factorial(k) $$ 20]
  %For
  Writeln[].
%Execute

%Package
```

### Sorting with Higher-Order Functions

```astarte
Package Sort

Define quicksort by
  case quicksort ? []               = []
  case quicksort ?cmp (?pivot :: ?t) =
       quicksort cmp [: x | Let x = each t. {cmp(x, pivot)} :]
    ++ [pivot]
    ++ quicksort cmp [: x | Let x = each t. {not cmp(x, pivot)} :]
%Define

Let sortAsc  = quicksort(<).
Let sortDesc = quicksort(>).

Execute
  Let lst = [5, 1, 4, 2, 8, 6, 3, 7].
  Writeln[$sortAsc(lst)].   %% [1, 2, 3, 4, 5, 6, 7, 8]
%Execute

%Package
```

### Logic Programming: Family Tree

```astarte
Package Family

Import "logic/logic".

Define Mother by all
  case Mother("minnie", "joan").  <- ()
  case Mother("joan",   "curly"). <- ()
  case Mother("joan",   "larry"). <- ()
%Define

Define Father by all
  case Father("larry", "mike"). <- ()
%Define

Define Parent by all
  case Parent(?x, ?y). <- Mother(x, y).
  case Parent(?x, ?y). <- Father(x, y).
%Define

Define Grandparent by all
  case Grandparent(?x, ?y). <-
    Var{logic} z.
    Parent(x, z).
    Parent(z, y).
%Define

Execute
  Var{logic} who.
  Grandparent("joan", who).
  Writeln["Joan's grandchild: ", who].   %% mike
%Execute

%Package
```

### Infinite List (Lazy Evaluation)

```astarte
Package Lazy

%% Infinite list of natural numbers starting at n
Define nats(?n) = (:n :: nats(n+1):).

%% Take the first n elements of a list
Define take by
  case take(0, ?)      = []
  case take(?n, ?h::?t) = h :: take(n-1, t)
%Define

Execute
  Let first10 = take(10, nats(0)).
  Writeln[$first10].   %% [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
%Execute

%Package
```

---

## Standard Library

The Astarte standard library is automatically available. Additional library packages must be imported explicitly.

| Library | Import | Description |
|---------|--------|-------------|
| (standard) | *(automatic)* | Basic types, arithmetic, I/O |
| Boolean/comparisons | *(standard)* | `and`, `or`, `not`, `<`, `>`, `==`, etc. |
| Characters | *(standard)* | Character operations |
| Numbers | *(standard)* | `+`, `-`, `*`, `/`, `^`, `mod`, numeric conversions |
| Collections | `Import "collect/..."` | Lists, sets, tables, strings |
| Strings | `Import "collect/string"` | String operations |
| Symbols | *(standard)* | `#"name"` symbol literals |
| Optional values | `Import "collect/optional"` | Values that may be absent |
| Input/Output | *(standard)* | `Writeln`, `Write`, `Extract`, `stdin!`, `stdout!` |
| System access | `Import "system/system"` | Command-line args, file system |
| Logic programming | `Import "logic/logic"` | Prolog-style logic programming |
| Random numbers | `Import "misc/random"` | Random number generation |
| Thread management | `Import "thread/threadlib"` | Multi-threaded computation |
| Tracing | `Import "misc/trace"` | Execution tracing |

### Commonly Used Built-in Operations

| Operation | Description |
|-----------|-------------|
| `$x` | Convert value `x` to a string |
| `x $$ n` | Format `x` as a string of width `n` |
| `x ++ y` | Append lists (or strings) |
| `h :: t` | Prepend element `h` to list `t` |
| `head(lst)` | First element of list |
| `tail(lst)` | List without its first element |
| `length(lst)` | Length of list |
| `@b` | Content of box `b` |
| `a _upto_ b` | List `[a, a+1, ..., b]` |
| `a _mod_ b` | Integer modulo |
| `fail exc` | Fail with exception `exc` |

---

## Troubleshooting

- **Type errors:** Use `astc -s` to dump type inference information. Use `Assume` declarations to add type annotations for pinpointing errors.
- **Out-of-memory errors:** Increase heap or stack limits with `-h` and `-s` options when running `astr`. Alternatively, edit `src/misc/options.h` to include `unixl.h` instead of `unixs.h` and rebuild.
- **Indentation warnings:** Suppress with `astc -i`. Astarte uses indentation to check block structure.
- **Pattern non-exhaustiveness:** Suppress with `astc -k`, or add missing cases to your pattern-matching functions.
- **Debugging:** Use `astr -v` to enter the debugger. Type `help` inside the debugger to see available commands.
- **Getting help:** Use the `asthelp` command for context-sensitive help about language features.

---

## Copyright and License

Copyright © 2000 **Karl Abrahamson**  
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice in the documentation and/or other materials provided with the distribution.
3. Redistribution **for profit is not permitted**.

This software is provided by the author **"as is"** and any express or implied warranties are disclaimed. The author is not liable for any damages arising from the use of this software. See the `COPYRIGHT` file for the full disclaimer.

---

*Astarte is under development by Dr. Karl Abrahamson at East Carolina University. Comments and bug reports can be sent to karl@cs.ecu.edu.*
