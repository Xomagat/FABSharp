# FAB# is a programming language written in C++!

## Sections:
- [HelloWorld](##Hello World)
- [Typing](##Typing)
- [Conditions](##Conditions)
- [Arithmetic](##Arithmetic)

## Hello World
``` c++
writeln "Hello World!";
```
- There is also a write command, an alternative to writeln, but without the line break.
``` c++
write "Hello World!\n";
```

- **The code may change, so this is not permanent!**

## Typing
|Type|Size|Range/Description| Example   |
|---|---|---|---|
| `int` | 32 bits | −2,147,483,648 … 2,147,483,647 | `int x = 42;` |
| `double` | 64 bits | ~15–17 significant digits | `double d = 3.14159;` |
| `bool` | — | `true` / `false` | `bool b = true;` |
| `string` | — | String of characters | `string s = "Hello";` |

- The language is still under development! *Names* or *the number* of types may **change**!

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

- Brackets are not required in conditions.
- It’s also worth noting that the else if command is not yet supported!

### Example of usage:
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

- Conditional operators: ==, !=, <, >, <=, >=, && (and), and || (or).

## Arithmetic
| Operator | Description                      | Example        |
|-----|-------------------------------|---------------|
| `+` | Addition operator             | `4 + 2 == 6`  |
| `-` | Subtraction operator            | `4 - 2 == 2`  |
| `*` | Multiplication operator            | `4* 2 == 8`  |
| `/` | Division operator              | `4 / 2 == 2`  |
| `^` | Exponentiation operator | `4 ^ 2 == 16` |