# FAB#

*Programming Language Documentation · Version 0.2*

> [!WARNING]
> FAB# is under active development. In version 0.2, the syntax and feature set are still subject to change, so please check the documentation when updating the compiler.

## Sections

- [Introduction](#introduction)
- [Quick Start](#quick-start)
- [Output](#output)
- [Variables and Types](#variables-and-types)
- [Operators](#operators)
- [Conditionals](#conditionals)
- [Loops](#loops)
- [Functions](#functions)
- [Input](#input)
- [Type Methods](#type-methods)
- [Lists](#lists)
- [Dictionaries](#dictionaries)
- [Libraries and Modules](#libraries-and-modules)
- [Mini-project: FizzBuzz](#mini-project-fizzbuzz)
- [Cheat Sheet](#cheat-sheet)
- [Common Errors](#common-errors)
- [What's Missing](#whats-missing)
- [Known Issues](#known-issues)
- [Version History](#version-history)

## Introduction

FAB# is a compiled programming language written in C++ using LLVM. You write your program in a `.fab` file, the compiler turns it into a standard executable file, and it runs standalone, without an interpreter or unnecessary dependencies.

The idea is simple: minimal boilerplate. No mandatory headers, classes, or "magic" templates: just open a file, write a line, and you have a program.

| Feature | Details |
|---|---|
| File extension | `.fab` |
| Language version | 0.2 |
| Platforms | Windows and Linux (x86-64) |
| Compilation result | Standalone executable file |
| Entry point | `main` function or code directly in the file |
| Statement terminator | `;` |
| Code blocks | `{ ... }` |
| Comments | `// line` and `/* block */` |

## Quick Start

Three steps from an empty folder to a working program:

1. Create a file named `hello.fab` with the code from the example below.
2. Compile it in the terminal: `fab hello.fab`
3. Run the result: `./hello` on Linux or `hello.exe` on Windows.

```cpp
writeln "Hello, World!";
```

The executable file will appear next to the source file. The compiler automatically deletes the intermediate object file.

Compiler options:

| Command | Action |
|---|---|
| `fab file.fab` | Compiles the program |
| `fab --help` or `fab -h` | Shows help |
| `fab --version` or `fab -v` | Shows version |

> [!TIP]
> Want a "proper" program with a `main` function? Check out the [Functions](#functions) section. However, it's not required for short scripts: you can write code directly in the file, and it will execute from top to bottom.

## Output

There are two commands for output. They are almost identical, differing only by one thing: the line break.

| Command | Behavior |
|---|---|
| `writeln` | Prints the value and moves to a new line |
| `write` | Prints the value and stays on the same line |

```cpp
write "Hello, ";
write "World!\n";
writeln "Bye!";
```

Result:

```
Hello, World!
Bye!
```

### What it can print

| Value | Example | Output |
|---|---|---|
| Integer | `writeln 42;` | `42` |
| Floating-point number | `writeln 0.5;` | `0.5` (up to 17 significant digits, trailing zeros are not printed) |
| Boolean | `writeln true;` | `1` (`false` prints as `0`) |
| Character | `writeln 'A';` | `A` |
| String | `writeln "Привет";` | `Привет` |
| List | `writeln [1, 2];` | see [Lists](#lists) |
| Dictionary | `writeln d;` | see [Dictionaries](#dictionaries) |

> [!NOTE]
> Floating-point numbers are printed at full precision: `double` up to 17 significant digits, `float` up to 9. So `writeln 4.0;` prints `4`, while `writeln 3.14;` may show `3.1400000000000001`. That is a property of binary floating point, not a bug. To print a boolean as a word, use `flag.to_str()`.

### Special characters in strings and chars

| Notation | Meaning |
|---|---|
| `\n` | Newline |
| `\t` | Tab |
| `\r` | Carriage return |
| `\"` | Quotation mark inside a string |
| `\'` | Apostrophe inside a char |

### Comments

Anything the compiler doesn't need to know can be hidden in a comment:

```cpp
// Single-line comment

/*
   And this one can span
   any number of lines
*/
writeln "Comments don't interfere with the code";
```

## Variables and types

A variable is a named box containing a value. The box's type is defined once upon creation.

| Type | Size | What it stores | Example |
|---|---|---|---|
| `int` | 32 bits | integers from −2,147,483,648 to 2,147,483,647 | `int x = 42;` |
| `long` | 64 bits | very large integers | `long big = 100;` |
| `short` | 16 bits | small integers | `short s = 7;` |
| `double` | 64 bits | fractional numbers (about 15–17 significant digits) | `double d = 3.14159;` |
| `float` | 32 bits | fractional numbers with lower precision | `float f = 1.5;` |
| `bool` | 1 bit | `true` or `false` | `bool ready = true;` |
| `char` | 8 bits | a single character | `char c = 'a';` |
| `string` | — | text | `string name = "FAB#";` |
| `list<T>` | — | a list of values of type `T` | `list<int> nums = [1, 2, 3];` |
| `dict<K,V>` | — | a "key → value" dictionary | `dict<string, int> ages = {"Anna": 20};` |
| `void` | — | "nothing": only used as a function return type | `define f() -> void` |

### Creation and assignment

```cpp
int age = 20;
double pi = 3.14159;
string name = "FAB#";
bool ready = true;
char grade = 'A';
int color = #FF8800;    // hexadecimal number starts with #
int counter;            // no value: defaults to 0

age = 21;               // new value
age += 1;               // age becomes 22
```

Shorthand assignment forms:

| Notation | Equivalent to |
|---|---|
| `x += 5;` | `x = x + 5;` |
| `x -= 5;` | `x = x - 5;` |
| `x *= 5;` | `x = x * 5;` |
| `x /= 5;` | `x = x / 5;` |
| `x %= 5;` | `x = x % 5;` |

> [!NOTE]
> If you create a numeric or boolean variable without a value, it will default to a zero value (`0` or `false`), and a list or dictionary will be empty. It is best to avoid doing this with strings; always assign them an initial value, e.g., `string s = "";`.

### Constants

The keyword `const` makes a value immutable. A constant must be assigned a value immediately upon declaration. Only simple types can be constants (`int`, `double`, `bool`, `string`, etc.), not lists or dictionaries.

```cpp
const int MAX_PLAYERS = 4;
writeln MAX_PLAYERS; // 4

// MAX_PLAYERS = 10; // compilation error: cannot change a constant
```

### Mixing number types

If you assign a floating-point number to an integer variable (or vice versa), the compiler handles the conversion automatically:

```cpp
double d = 5; // becomes 5
int n = 3.99; // becomes 3: fractional part is discarded, no rounding
```

For explicit conversions, every type has methods such as `to_int()` and `to_double()`; see [Type Methods](#type-methods).

### Kinds of values in code

| What | Appearance | Examples |
|---|---|---|
| integer | digits | `0`, `42`, `1000` |
| floating-point number | digits with a dot | `3.14`, `0.5` |
| hexadecimal | `#` and digits/letters a–f | `#FF`, `#1a2b` |
| boolean | keywords | `true`, `false` |
| character | in single quotes | `'a'`, `'\n'` |
| string | in double quotes | `"Hello"` |
| list | values in square brackets | `[1, 2, 3]` |
| dictionary | `key: value` pairs in curly braces | `{"a": 1, "b": 2}` |
| null value | keyword | `null` (converts to zero of the appropriate type, or to an empty list/dictionary) |

> [!NOTE]
> List and dictionary literals know their type only when they are put into a variable with an explicit type: `list<int> a = [1, 2];`. A bare `[1, 2]` cannot be passed to a function, for example.

### Where a variable "lives"

A variable created inside `{ ... }` ceases to exist after the closing brace.

```cpp
int x = 1;
if x == 1
{
    int y = 2;           // y is visible only inside these braces
    writeln x + y;       // 3
}
```

## Operators

### Arithmetic

| Operator | Function | Example | Result |
|---|---|---|---|
| `+` | addition | `7 + 2` | `9` |
| `-` | subtraction | `7 - 2` | `5` |
| `*` | multiplication | `7 * 2` | `14` |
| `/` | division | `7 / 2` | `3` (integer divided by integer yields an integer) |
| `/` | division | `7.0 / 2` | `3.5` |
| `%` | remainder | `7 % 2` | `1` |

When an expression contains both integers and floating-point numbers, the result is a floating-point number.

Strings can be added together: this concatenates them.

```cpp
string greeting = "Hello, " + "World!";
writeln greeting; // Hello, World!
```

To attach a number to a string, convert it to a string first:

```cpp
int n = 5;
writeln "n = " + n.to_str();    // n = 5
```

Unary operators: `-x` changes the sign, `+x` does nothing, `!flag` inverts the boolean value.

### Comparison

| Operator | Meaning |
|---|---|
| `==` | equal to |
| `!=` | not equal to |
| `<` | less than |
| `>` | greater than |
| `<=` | less than or equal to |
| `>=` | greater than or equal to |

Characters are compared like numbers: `c == 'a'`, `c >= '0'`.

### Logic

| Operator | Meaning | Example |
|---|---|---|
| `&&` | AND: true if both parts are true | `x > 0 && x < 10` |
| `\|\|` | OR: true if at least one part is true | `x < 0 \|\| x > 10` |
| `!` | NOT: inverts the value | `!ready` |

Evaluation is "lazy": if the answer is already clear from the left part, the right part is not even evaluated.

### Operator Precedence

From highest to lowest:

| Level | Operators |
|---|---|
| 1 | unary `-`, `+`, `!` |
| 2 | `*`, `/`, `%` |
| 3 | `+`, `-` |
| 4 | `<`, `>`, `<=`, `>=` |
| 5 | `==`, `!=` |
| 6 | `&&` |
| 7 | `\|\|` |

Not sure about the order? Use parentheses: `(a + b) * c`.

> [!WARNING]
> There is no exponentiation operator `^` in version 0.2. For powers, use the `pow` function from the `math` library: [Libraries and Modules](#libraries-and-modules). The `++` and `--` operators are also missing for now; write `i += 1;` instead.

## Conditionals

The classic "if... else":

```cpp
int x = 15;

if x == 15
{
    writeln "exactly fifteen";
}
else
{
    writeln "something else";
}
```

If there are more than two options, chain `else if`:

```cpp
int score = 72;

if score >= 90
{
    writeln "excellent";
}
else if score >= 60
{
    writeln "good";
}
else
{
    writeln "needs improvement";
}
```

A few rules:

- Parentheses around the condition are **optional**: you can use `if x > 5` or `if (x > 5)`.
- Conditions can be combined: `if x > 0 && x < 10`.
- A number also works as a condition: `0` means "false," while anything else is "true."
- If a branch contains only one statement, curly braces can be omitted.

## Loops

| Loop | When to use |
|---|---|
| `while` | repeat while the condition is true |
| `do-while` | same, but the body executes at least once |
| `for` | when the number of repetitions is known |

> [!IMPORTANT]
> In loops, parentheses around the condition are **mandatory**, unlike in `if` statements.

### while

```cpp
int i = 0;
while (i < 3)
{
    writeln i;
    i += 1;
}
```

Result: `0`, `1`, `2`.

### do-while

Action first, then the check. Therefore, the body executes at least once, even if the condition is false from the start:

```cpp
int i = 10;
do
{
    writeln i;
    i += 1;
} while (i < 5);
```

Result: `10`. Note the `;` after the closing parenthesis of the condition.

### for

Three parts inside the parentheses: what to do before starting, when to continue, and what to do after each step.

```cpp
for (int i = 1; i <= 5; i += 1)
{
    writeln i;
}
```

Result: `1`, `2`, `3`, `4`, `5`.

This is a handy way to walk through a list:

```cpp
list<string> names = ["Anna", "Boris", "Vika"];
for (int i = 0; i < names.length(); i += 1)
{
    writeln names[i];
}
```

### break and continue

| Command | What it does |
|---|---|
| `break;` | immediately exits the loop |
| `continue;` | skips the rest of the current step and proceeds to the next one |

```cpp
int i = 0;
while (true)
{
    i += 1;
    if i % 2 == 0 { continue; }   // skip even numbers
    if i > 7 { break; }           // exit after 7
    writeln i;
}
```

Result: `1`, `3`, `5`, `7`.

> [!TIP]
> In a `for` loop, the `continue` command won't get stuck in an infinite loop: it proceeds to the `i += 1` step rather than jumping straight to the condition check.

## Functions

A function is a named block of code that can be called any number of times.

```cpp
define name(type argument1, type argument2) -> resultType
{
    // body
}
```

- `define` declares the function.
- The result type is written after the arrow `->`. If there is no arrow, the function returns nothing (`void`).
- The result is returned using the `return` command.
- Any types may be used for arguments and results, including `list<int>` and `dict<string, int>`.

### Function without a result

```cpp
define greet(string name)
{
    writeln "Hello, " + name + "!";
}

greet("FAB#"); // Hello, FAB#!
```

### Function with a result

```cpp
define square(int x) -> int
{
    return x * x;
}

writeln square(7); // 49
```

If the body consists of a single statement, curly braces are not required:

```cpp
define twice(int x) -> int
    return x * 2;
```

### Multiple arguments

```cpp
define cool_print(string label, int value)
{
    writeln label + " done";
    writeln value;
}

cool_print("Task", 42);
```

### Recursion

A function can call itself:

```cpp
define factorial(int n) -> int
{
    if n <= 1 { return 1; }
    return n * factorial(n - 1);
}

writeln factorial(5); // 120
```

### Overloading

You can create multiple functions with the same name, provided they have different argument types. The compiler will automatically select the appropriate one:

```cpp
define show(int x)
{
    writeln x;
}

define show(string s)
{
    writeln "text: " + s;
}

show(5); // 5
show("hi"); // text: hi
```

> [!WARNING]
> Argument types during the call must match the declaration exactly. If only `square(int x)` exists, calling `square(2.5)` will not work: `2.5` is a `double`. Declare a `square(double x)` version or pass an `int`. (Standard library functions are more forgiving: if there is no exact match, the compiler tries widening numbers to `double`, so `sin(1)` works.)

### The `main` function

There are two ways to write a program:

| Method | What it looks like |
|---|---|
| Without `main` | commands sit directly in the file and execute from top to bottom |
| With `main` | the entire program lives inside `main`, just like in major languages |

```cpp
define main() -> int
{
    writeln "Hello from main!";
    return 0;
}
```

Writing `return 0;` at the end of `main` is optional: if you forget it, the compiler will return `0` automatically.

> [!IMPORTANT]
> If a file contains `main`, only function declarations (`define`) are allowed outside of it. Standard commands like `writeln` must reside inside functions. Functions must be declared **before** the point where they are called.

## Input

The `input_in` command reads a value from the keyboard directly into a variable. The variable must be created beforehand.

```cpp
int n;
write "Enter a number: ";
input_in n;
writeln n * 2;
```

What can be entered:

| Variable type | What is read |
|---|---|
| `int`, `short`, `long` | an integer |
| `float`, `double` | a fractional number (with a dot) |
| `bool` | a number: `0` is `false`, anything else is `true` |
| `char` | a single character (whitespace and newlines before it are skipped) |
| `string` | one word up to the first space, at most 255 characters |

> [!NOTE]
> Reading a whole line with spaces is not possible yet. Lists and dictionaries cannot be entered with `input_in`.

## Type methods

Values have **methods**: small functions called with a dot, `value.method(arguments)`. Methods work on variables and on literals, and they can be chained:

```cpp
string s = "  fab#  ";
writeln s.trim().to_upper();      // FAB#
writeln 'x'.to_upper();           // X
writeln "42".to_int() + 1;        // 43
```

Methods of `string`, `char`, `bool`, `int`, `short`, `long`, `float`, `double`, `list` and `dict` are always available; no `use` is needed. Below, `T` is the list element type, and `K`/`V` are the dictionary key and value types.

### string

| Method | Result | What it does |
|---|---|---|
| `length()` | `int` | string length |
| `empty()` | `bool` | is the string empty? |
| `at(i)` | `char` | character by index (out of range gives `'\0'`) |
| `first()`, `last()` | `char` | first and last character |
| `find(x)` | `int` | position of the first occurrence of string or char `x`, `-1` if not found |
| `rfind(s)` | `int` | position of the last occurrence, `-1` if none |
| `contains(s)` | `bool` | contains the substring? |
| `count(s)` | `int` | how many times the substring occurs |
| `start_with(s)`, `ends_with(s)` | `bool` | starts / ends with `s` |
| `substr(start, length)` | `string` | a piece of the string |
| `replace(a, b)` | `string` | replaces every `a` with `b` |
| `to_upper()`, `to_lower()` | `string` | letter case |
| `capitalize()` | `string` | first letter uppercase |
| `trim()` | `string` | strips whitespace at the edges |
| `reverse()` | `string` | reverses the string |
| `repeat(n)` | `string` | repeats it `n` times |
| `pad_left(width, fill)`, `pad_right(width, fill)` | `string` | pads with the `fill` char up to `width` |
| `compare(s)` | `int` | like `strcmp`: negative, zero or positive |
| `equals_ignore_case(s)` | `bool` | equality ignoring case |
| `is_blank()` | `bool` | whitespace only |
| `is_numeric()` | `bool` | digits only |
| `to_int()`, `to_long()`, `to_double()` | number | parses a number from the string |
| `to_bool()` | `bool` | `true` only if the string equals `"true"` |
| `split(sep)` | `list<string>` | splits the string by a separator |

A character of a string can also be read with square brackets: `s[0]`.

### char

| Method | Result | What it does |
|---|---|---|
| `is_digit()`, `is_alpha()`, `is_alnum()` | `bool` | digit / letter / letter or digit |
| `is_upper()`, `is_lower()` | `bool` | uppercase / lowercase |
| `is_space()`, `is_punct()` | `bool` | whitespace / punctuation |
| `is_hex()`, `is_printable()`, `is_ascii()` | `bool` | hex digit / printable / ASCII |
| `to_upper()`, `to_lower()`, `swap_case()` | `char` | letter case |
| `to_int()`, `to_short()`, `to_long()`, `to_float()`, `to_double()` | number | character code |
| `to_digit()` | `int` | `'0'..'9'` → `0..9`, otherwise `-1` |
| `to_hex()` | `int` | `'0'..'9'`, `'a'..'f'`, `'A'..'F'` → `0..15`, otherwise `-1` |
| `to_str()` | `string` | the character as a string |
| `repeat(n)` | `string` | a string of `n` such characters |
| `compare(c)` | `int` | difference of the codes |
| `equals_ignore_case(c)` | `bool` | equality ignoring case |

### int, short, long

| Method | Result | What it does |
|---|---|---|
| `to_str()` | `string` | the number as a string |
| `is_positive()`, `is_negative()`, `is_zero()` | `bool` | sign |
| `is_even()`, `is_odd()` | `bool` | even / odd |
| `abs()` | same type | absolute value |
| `sign()` | `int` | `-1`, `0` or `1` |
| `to_bool()`, `to_char()` | `bool` / `char` | conversion |
| `to_short()`, `to_int()`, `to_long()`, `to_float()`, `to_double()` | number | conversion (not needed for the type itself) |

### float, double

| Method | Result | What it does |
|---|---|---|
| `to_str()` | `string` | the number as a string |
| `is_positive()`, `is_negative()`, `is_zero()` | `bool` | sign |
| `is_nan()`, `is_inf()`, `is_finite()` | `bool` | special values |
| `abs()` | same type | absolute value |
| `sign()` | `int` | `-1`, `0` or `1` |
| `floor()`, `ceil()`, `trunc()`, `round()` | same type | rounding |
| `to_bool()`, `to_char()`, `to_short()`, `to_int()`, `to_long()` | value | conversion; `NaN` gives `0`, values that are too large are clamped to the type's limit |
| `to_float()` / `to_double()` | floating-point | conversion between `float` and `double` |

### bool

| Method | Result | What it does |
|---|---|---|
| `to_str()` | `string` | `"true"` or `"false"` |
| `to_bin()` | `string` | `"1"` or `"0"` |
| `to_int()`, `to_short()`, `to_long()`, `to_float()`, `to_double()` | number | `0` or `1` |
| `negate()` | `bool` | NOT |
| `and(b)`, `or(b)`, `xor(b)` | `bool` | AND, OR, exclusive OR |
| `nand(b)`, `nor(b)`, `xnor(b)` | `bool` | negations of the above |
| `implies(b)` | `bool` | "if a, then b" |
| `compare(b)` | `int` | `false < true`: `-1`, `0` or `1` |

> [!NOTE]
> Unlike `&&` and `||`, the `and` and `or` methods always evaluate both sides.

### Limit constants

These constants are available without `use`:

| Constant | Type | Value |
|---|---|---|
| `INT_MIN`, `INT_MAX` | `int` | `int` limits |
| `SHORT_MIN`, `SHORT_MAX` | `short` | `short` limits |
| `LONG_MIN`, `LONG_MAX` | `long` | `long` limits |
| `CHAR_MIN`, `CHAR_MAX` | `int` | `-128` and `127` |
| `FLOAT_MIN`, `FLOAT_MAX`, `FLOAT_EPSILON` | `float` | smallest and largest positive value, machine epsilon |
| `DOUBLE_MIN`, `DOUBLE_MAX`, `DOUBLE_EPSILON` | `double` | the same for `double` |

## Lists

A list stores a sequence of values of one type. Elements can be `int`, `long`, `short`, `char`, `float`, `double`, `string` and `bool`.

```cpp
list<int> nums = [5, 2, 9];
nums.addend(7);                // append to the end
nums.sort();

writeln nums;                  // [2, 5, 7, 9]
writeln nums.length();         // 4
writeln nums[0];               // 2
writeln nums.sum();            // 23
```

An empty list is created without a value (or with `null`):

```cpp
list<string> words;
words.addend("one");
words.addend("two");
writeln words.join(", ");      // one, two
```

> [!TIP]
> Don't write `>` and `=` together when closing a type: the lexer reads `list<int>= ...` as `>=`. Put a space or go straight to the variable name: `list<int> nums = ...`.

> [!IMPORTANT]
> List methods never fail because of a bad index: `get(100)` on a short list returns the type's "zero" value (`0`, `false` or an empty string), and `set`, `insert` and `remove_at` with a bad index simply do nothing. Watch the bounds yourself.

### List methods

| Method | Result | What it does |
|---|---|---|
| `addend(x)` | — | adds `x` **to the end** |
| `add(x)` | — | adds `x` **to the beginning** |
| `insert(i, x)` | — | inserts `x` at position `i` |
| `extend(other)` | — | appends all elements of another list |
| `length()` | `int` | number of elements |
| `empty()` | `bool` | is the list empty? |
| `capacity()`, `reserve(n)` | `int` / — | capacity and reserving space |
| `get(i)`, `set(i, x)` | `T` / — | read / write an element |
| `first()`, `last()` | `T` | first and last element |
| `remove_at(i)` | — | removes the element at position `i` |
| `remove(x)` | `bool` | removes the first `x`; `true` if found |
| `pop()`, `pop_first()` | `T` | takes and removes the last / first element |
| `clear()` | — | empties the list |
| `contains(x)` | `bool` | is `x` present? |
| `index_of(x)` | `int` | position of the first `x`, or `-1` |
| `count(x)` | `int` | how many times `x` occurs |
| `reverse()` | — | reverses in place |
| `swap(i, j)` | — | swaps two elements |
| `sort()`, `sort_desc()` | — | sort ascending / descending |
| `min()`, `max()` | `T` | smallest / largest |
| `sum()` | `T` | sum (concatenation for strings); not available for `char` and `bool` |
| `copy()` | `list<T>` | an independent copy |
| `slice(start, length)` | `list<T>` | a piece of the list |
| `join(sep)` | `string` | joins the elements into a string with a separator |

An element can also be read with square brackets: `nums[2]`.

## Dictionaries

A dictionary stores "key → value" pairs and remembers the order in which keys were added. Keys can be `int`, `long`, `short`, `char` and `string`; values can be `int`, `long`, `short`, `char`, `float`, `double`, `string` and `bool`.

```cpp
dict<string, int> ages = {"Anna": 20, "Boris": 31};
ages.set("Vika", 25);

writeln ages.get("Anna");         // 20
writeln ages.get_or("Dima", -1);  // -1
writeln ages.contains("Boris");   // 1
writeln ages.length();            // 3
writeln ages;                     // {Anna: 20, Boris: 31, Vika: 25}
```

A `{...}` literal knows its type only when it initializes a variable with an explicit `dict<K, V>` type. An empty dictionary is created without a value: `dict<int, string> d;`.

To walk through all pairs, use `key_at` and `value_at`:

```cpp
for (int i = 0; i < ages.length(); i += 1)
{
    writeln ages.key_at(i) + ": " + ages.value_at(i).to_str();
}
```

### Dictionary methods

| Method | Result | What it does |
|---|---|---|
| `set(k, v)` | — | adds a pair or replaces the value of an existing key |
| `merge(other)` | — | copies all pairs of another dictionary into this one |
| `get(k)` | `V` | value by key; the type's "zero" value if the key is missing |
| `get_or(k, def)` | `V` | value by key, or `def` if the key is missing |
| `key_at(i)`, `value_at(i)` | `K` / `V` | key / value by position |
| `contains(k)` | `bool` | is the key present? |
| `contains_value(v)` | `bool` | is the value present? |
| `length()`, `empty()`, `capacity()` | `int` / `bool` | size |
| `remove(k)` | `bool` | removes a pair; `true` if the key existed |
| `pop(k)` | `V` | removes a pair and returns its value |
| `clear()` | — | empties the dictionary |
| `keys()`, `values()` | `list<K>` / `list<V>` | lists of keys and values |
| `copy()` | `dict<K,V>` | an independent copy |

## Libraries and modules

### Importing a library

The `use` command imports a library. It must be imported **before** its functions are used for the first time.

```cpp
use math;

writeln sqrt(16.0); // 4
writeln pow(2, 10); // 1024
writeln floor(3.7); // 3
writeln round(2.5); // 3
writeln PI;         // 3.1415926535897931
```

### The `math` library

Constants: `PI`, `TAU`, `E`, `PHI`, `SQRT2`, `LN2`, `LN10` (all `double`).

Functions are chosen by argument types: `pow(2, 10)` calculates an integer result, while `pow(2.0, 10.0)` calculates a floating-point result. This is the same kind of overloading found in your own functions.

| Group | Functions | Arguments → result |
|---|---|---|
| Absolute value | `abs(x)` | `int`, `long`, `double` → same type |
| | `fabs(x)` | `float`, `double` → same type |
| Powers and roots | `pow(x, n)` | `int`, `long`, `float`, `double` → same type (integer power with a negative `n` gives `0`) |
| | `sqrt(x)` | `int` → `double`; `float` → `float`; `double` → `double` |
| | `cbrt(x)` | `float`, `double` → same type |
| | `hypot(a, b)` | `float`, `double` → same type |
| Rounding | `ceil(x)`, `floor(x)`, `round(x)`, `trunc(x)` | `float`, `double` → same type |
| Remainder | `fmod(a, b)` | `float`, `double` → same type (sign of `a`) |
| | `mod(a, b)` | `int` → `int` (sign of `b`) |
| Trigonometry | `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `sinh`, `cosh`, `tanh` | `float`, `double` → same type |
| | `atan2(y, x)` | `float`, `double` → same type |
| Exponent and logarithms | `exp`, `log`, `log2`, `log10` | `float`, `double` → same type |
| | `log(x, base)` | `double, double` → `double` |
| Min, max | `min(a, b)`, `max(a, b)` | `int`, `long`, `float`, `double` → same type |
| | `clamp(x, lo, hi)` | `int`, `long`, `float`, `double` → same type |
| | `sign(x)` | `int`, `long`, `float`, `double` → `int` (`-1`, `0`, `1`) |
| Integer math | `gcd(a, b)`, `lcm(a, b)` | `int`, `long` → same type |
| | `factorial(n)` | `int` → `long` (gives `-1` for `n < 0` or `n > 20`) |
| | `is_prime(n)` | `int`, `long` → `bool` |
| Checks | `is_nan(x)`, `is_inf(x)`, `is_finite(x)` | `float`, `double` → `bool` |
| | `inf()`, `nan()` | → `double` |
| Angles | `to_rad(x)`, `to_deg(x)` | `float`, `double` → same type |
| Interpolation | `lerp(a, b, t)` | `float`, `double` → same type |

> [!TIP]
> If there is no exact version of a function for the argument types, the compiler tries widening numbers to `double`: `sin(1)` and `exp(2)` work even though the argument is an integer.

### Including a custom file

You can split a large program across multiple files. To include another `.fab` file, specify the path in quotes:

```cpp
use "libs/helpers.fab";
```

Each file is included only once, even if you write `use` multiple times.

> [!NOTE]
> In version 0.2, the path in `use "..."` is relative to the folder containing the compiler itself, not the folder containing the script.

## Mini-project: FizzBuzz

Time to put it all together. A classic problem: iterate through numbers from 1 to 15 and print `Fizz` for multiples of 3, `Buzz` for multiples of 5, and `FizzBuzz` for multiples of both.

```cpp
for (int i = 1; i <= 15; i += 1)
{
    if i % 15 == 0
    {
        writeln "FizzBuzz";
    }
    else if i % 3 == 0
    {
        writeln "Fizz";
    }
    else if i % 5 == 0
    {
        writeln "Buzz";
    }
    else
    {
        writeln i;
    }
}
```

This program demonstrates the use of a `for` loop, an `else if` chain, the modulo operator `%`, and output operations.

**Try it yourself:**

- Change the loop limit to run FizzBuzz up to 100.
- Move the check into a function `define label(int n) -> string` and return the appropriate word.
- Print the sum of all numbers from 1 to 100 using a loop and an accumulator variable.
- Collect the FizzBuzz answers in a `list<string>` and print them on one line with `join(" ")`.

## Cheat Sheet

| Requirement | Syntax |
|---|---|
| Output with newline | `writeln expression;` |
| Output without newline | `write expression;` |
| Variable | `int x = 5;` |
| Character | `char c = 'a';` |
| Constant | `const int N = 5;` |
| Assignment | `x = 10;`, `x += 1;` |
| Input | `input_in x;` |
| List | `list<int> a = [1, 2, 3];` |
| Dictionary | `dict<string, int> d = {"a": 1};` |
| List element / string character | `a[0]`, `s[0]` |
| Method | `value.method(arguments)` |
| Number to string | `n.to_str()` |
| Condition | `if a > b { ... } else { ... }` |
| Conditional chain | `else if ... { ... }` |
| While loop | `while (condition) { ... }` |
| Do-while loop | `do { ... } while (condition);` |
| For loop | `for (int i = 0; i < n; i += 1) { ... }` |
| Exit loop | `break;` |
| Next loop iteration | `continue;` |
| Function | `define f(int a) -> int { return a; }` |
| Import library | `use math;` |
| Import file | `use "path/file.fab";` |
| Comments | `// ...` and `/* ... */` |

## Common errors

The compiler reports errors in English (prefixed with `Compile error:`). Here is what they mean:

| Message | What happened | How to fix |
|---|---|---|
| `You miss the ;` | missing semicolon | add `;` at the end of the statement |
| `Variable {x} not found!` | variable not declared or out of scope | declare it earlier or within the appropriate braces |
| `Cannot assign to const variable 'x'!` | attempt to modify a constant | remove `const` or do not change the value |
| `Cannot assign to library const 'X'!` | attempt to modify a library constant (`PI`, `INT_MAX`...) | library constants cannot be changed |
| `Cannot declare a variable of type 'void'!` | a variable of type `void` | choose a real type |
| `'break' outside of a loop!` | `break` used outside a loop | use it only inside loops |
| `'continue' outside of a loop!` | `continue` used outside a loop | same as above |
| `Unknown function or overload for codegen: name(...)` | function does not exist or argument types do not match | check the name, `use` statement, and argument types (the passed types are shown in parentheses) |
| `Type 'X' has no method 'name'` | the type has no such method | check the tables in the "Type methods" section |
| `Type 'X' has no method 'name' with these arguments` | the method exists but with different arguments | check the number and types of arguments |
| `Library '...' not found!` | library file missing | check the name in `use` and the `lib` folder next to the compiler |
| `Token ... does not match ...` | wrong symbol found where another was expected | usually a missing bracket, e.g., in a `while` loop |
| `Unknown expression!` | The compiler did not understand the expression | Check for typos and extra characters |
| `Unknown type for codegen: ...` | no such type | check the type name (e.g., `byte` does not exist) |
| `Dict literal needs a type context: dict<K,V> x = {...};` | a `{...}` dictionary written outside a variable declaration | declare `dict<K, V> x = {...};` first |
| `List literal can only initialize a list<T>!` | a `[...]` list put into a variable of another type | declare the variable as `list<T>` |
| `Type '...' does not support indexing!` | square brackets on a type that does not support them | only lists and strings can be indexed |
| `Only '+' is supported for strings!` | an operator other than `+` used with a string | strings can only be concatenated |
| `Unsupported type for input_in codegen!` | `input_in` for a list or dictionary | enter values one by one |
| `Link failed!` | the executable could not be linked | make sure the `lib` and `tools` folders are next to the compiler |

## What is not yet supported

Version 0.2 is not the end of the road. The following are not yet supported:

- the exponentiation operator `^` (use `pow` from `math`);
- the `++` and `--` operators;
- string comparison using `==` and `<` (use `compare()` and `equals_ignore_case()`);
- concatenating a string with a number (`"x = " + 5`; use `5.to_str()`);
- assigning to a list element with `a[i] = x` (use `a.set(i, x)`);
- reading a dictionary value with `d[key]` (use `d.get(key)`);
- nested lists and dictionaries (`list<list<int>>`);
- the `byte` type;
- structures and classes;
- input of whole lines with spaces;
- multiple files sharing state outside of `use "..."`.

## Known issues

- Strings are printed as-is through the system `printf`, so a `%` character inside a printed string may produce odd results for now. Avoid `%` in text for `write` and `writeln`.
- `write` (no newline) currently works correctly with strings, `int`, `double` and `float`. For `char`, `long`, lists and dictionaries, use `writeln`.

## Version history

| Version | What's new |
|---|---|
| 0.2 | `char`, `list<T>` and `dict<K,V>` types with `[...]` and `{...}` literals; methods on all types; list and string indexing `a[i]`; `input_in` for all simple types and strings; extended `math` library (constants, trigonometry, logarithms, `min`/`max`/`clamp`, `gcd`/`lcm`, value checks and more); limit constants such as `INT_MAX` |
| 0.1 | Initial version: data types, variables and constants, conditionals, loops, overloaded functions, input/output, `math` library, module importing, compilation for Windows and Linux |