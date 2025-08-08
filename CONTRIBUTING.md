# How to contribute to PTTTL

If you want to contribute to the PTTTL project, you can open a pull request at https://github.com/eriknyquist/ptttl/pulls. Some technical requirements:

1. Changes to the PTTTL design description (the top-level README file) will not be accepted without the corresponding changes to the C reference
   implementation (the Python reference implementation is old, under-tested and I don't care too much about it).
1. Any changes to the C reference implementation should not break the existing test suite (`c_implementation/test/test_ptttl.c`), and should
   come with new tests if appropriate (`cd c_implementation && make tests` to run tests)
1. API changes to the C reference implementation should be documented with Doxygen comments, and the pull request should include any changes to
   the docs build output folder (`cd c_implementation && doxygen` to build the docs, which will be output to `docs/`)
1. There is no strictly/automatically enforced coding style, however any changes are expected to follow the general style of existing code. See
   "Coding style" section for more details.

# Coding style

This is by no means a comprehensive coding style guide, rather just a few key points that I felt worth pointing out.

1. Opening and closing braces for loops, functions, if-statements, and struct definitions should always be on a new line, e.g.:

    ```
    void myfunc(void)
    {
    }
    ```
    ```
    for (i = 0; i < 10; i++)
    {
    }
    ```
    ```
    struct mystruct
    {
        int a;
        int b;
    };
    ```
    
1. `if` and `while` statements should always use braces for the body, even if not required by the syntax (no "one-liners") e.g.:
   ```
   // No
   if (cond) do_thing();

   // Yes
   if (cond)
   {
       do_thing();
   }
   ```

1. Operators and operands should always be separated by 1 space character, e.g.:
   ```
   // No
   int x = a-b;

   // Yes
   int x = a - b;
   ```
1. Operator precedence rules should never be relied upon, no matter how obvious you may think it is-- always use parentheses. E.g:
   ```
   // No
   if (a < b || c < d + e) ...

    // Yes
    if ((a < b) || (c < (d + e))) ...
   ```
1. Indentation must be done with spaces, 4 spaces per level of indentation.

1. All symbols in API-facing header files must be fully documented with Doxygen comments. This includes function declarations, struct definitions, typedefs, enums, and preprocessor symbols.

1. API-facing function implementations must have a doxygen comment that points to the header file containing the API documentation, as follows;
```
// header file myfile.h

/**
 * Description of function
 *
 * @param x  Description of parameter
 * @param b  Description of parameter
 *
 * @return Description of return value
 */
int my_function(int x, float b);

// C file myfile.c

/**
 * @see myfile.h
 */
int my_function(int x, float b)
{
    // Implementation...
}
```

1. Static (private) functions must be fully documented with Doxygen comments.
