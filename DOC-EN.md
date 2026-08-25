# FAB# is a programming language written in C++!

## Sections:
- [HelloWorld](#hello-world)
- [Typing](#typing)
- [Conditions](#conditions)
- [Arithmetic](#arithmetic)
- [Loops](#loops)
- [Functions](#functions)

## Hello World
``` c++
writeln "Hello World!";
```
- There's also a write command, an alternative to writeln, but without line breaks.
``` c++
write "Hello World!\n";
```

- **The code may change, so this isn't permanent!**

## Typing
|Type|Size|Range/Description| Example |
|---|---|---|---|
| `int` | 32 bits | -2,147,483,648 … 2,147,483,647 | `int x = 42;` |
| `double` | 64 bits | ~15–17 significant digits | `double d = 3.14159;` |
| `bool` | — | `true` / `false` | `bool b = true;` |
| `string` | — | Character string | `string s = "Hello";` |

- The language is still under development! *Names* or *number* of types may **change**!

## Conditions
``` c++
if condition
{
// body
}
else
{
// body
}
```

- Parentheses are optional in conditions.
- Also note that the else if command is not yet supported!

### Usage example:
``` c++
int x = 15;

if x == 15
{
writeln "true";
}
else
{
writeln "false";
}
```

- Conditional operators: ==, !=, <, >, <=, >=, &&(and), and ||(or).

## Arithmetic
| Operator | Description | Example |
|-----|------------------------------|---------------|
| `+` | Addition operator | `4 + 2 == 6` |
| `-` | Subtraction operator | `4 - 2 == 2` |
| `*` | Multiplication operator | `4 * 2 == 8` |
| `/` | Division operator | `4 / 2 == 2` |
| `^` | Exponentiation operator | `4 ^ 2 == 16` |

## Loops
### While:
- While - conditional loop
``` c++
while (condition)
{
// body
}
```
Usage example:
``` c++
int i = 0;
while (i < 10)
{
writeln i;
i = i + 1;
}
```

### For:
- For - loop with counter
``` c++
for (initialization; condition; increment)
{
// body
}
```
Usage example:
``` c++
for (int i = 0; i < 10; i = i + 1)
{
writeln i;
}
```

### do-while:
- do-while - a loop with a condition, but it will execute once, even if the condition is initially false.
``` c++
do
{
// body
} while (condition);
```
Usage example:
``` c++
int i = 1;
do
{
writeln i;
i = i + 1;
} while (i <= 5); // Output: 1, 2, 3, 4, 5
```
``` c++
int i = 1;
do
{
writeln i;
i = i + 1;
} while (i <= 0); // Output: 1
```
- The following commands are also supported: *continue* and *break*.

## Functions
- Functions are predefined commands that can be called any number of times during program execution.

Here's how functions are declared in FAB#:
``` c++
define name(type arg)
{
// body
}
```
Usage example:
``` c++
define cool_print(string name, string text)
{
writeln name + " = " + text;
}

cool_print("2 ^ 8 * 3", 2 ^ 8 * 3); // Output: 2 ^ 8 * 3 = 768.000000
```
``` c++
define pow(double x, double n)
return x ^ n;

writeln pow(2, 5); // Output: 32.000000
```
- The language is still under development! *Argument passing* and *syntax* may *change*!