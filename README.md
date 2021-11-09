autocleanup
===========

Utility macros for typesafe RAII-style scoped cleanups for variables.

**Note:** This requires using GCC or Clang.


Installation
------------

Install with [clib](https://github.com/clibs/clib):

```sh
clib install aperezdc/autocleanup --save
```

Or just copy [autocleanup.h](autocleanup.h) inside your project.


Usage
-----

1. [Defining a new type](#defining-a-new-type)
2. [Transferring ownership](#transferring-ownership)
3. [Clearing pointers](#clearing-pointers)
4. [More idioms](#more-idioms)
5. [Other pointers](#other-pointers)
6. [Working with handles](#working-with-handles)


### Defining a new type

Let's see how to add automatic cleanup support for an hypothetical `Person`
opaque type, with its API defined as follows:

```c
typedef struct _Person Person;

Person*     person_new  (const char *name);
void        person_free (Person *person);
const char* person_name (Person *person);

PTR_AUTO_DEFINE (Person, person_free)  /* Magic */
```

Note how the `PTR_AUTO_DEFINE` macro is used to specify that the
`person_free()` function is to be invoked on instances of `Person*` values.
Typically the above would be used in the header which defines the type. Code
which uses this type can now rely on values being freed when they go out of
scope:

```c
void handle_person_named (const char *name) {
    ptr_auto(Person) person = person_new (name);

    _Bool valid = person_validate (person);
    if (!valid) return;

    person_handle (person);
}
```

In the function above, the instance pointed by the `person` variable will
be automatically freed when the function returns, regardless of whether it
executes till the end or an early return is done.


### Transferring ownership

Continuing with the example above, whenever a function wants to return an
instance of `Person` to an outer scope, it needs to set the variable which
holds the pointer to `NULL`, and then return the pointer:

```c
Person* person_create_myself (void) {
    ptr_auto(Person) person = person_new ("Adrian Perez");

    person_customize_myself (person);

    Person *return_value = person;  /* Keep around the pointer value, */
    person = NULL;                  /* avoid the automatic cleanup,   */
    return return_value;            /* ...and return the pointer      */
}
```

Because the pattern above is so common, the `ptr_steal()` type-safe function
is provided, which “steals” the pointer from a variable and returns it, and
is essentially equivalent to the last three lines from the function above.
Thus, it can rewritten as:

```c
Person* person_create_myself (void) {
    ptr_auto(Person) person = person_new ("Adrian Perez");

    person_customize_myself (person);

    return ptr_steal (&person);  /* Clears “person”, returns pointer. */
}
```

As a bonus, `ptr_steal()` can be used in any pointer value, regardless of
whether it has been declared with `ptr_auto(T)` or not, and it is encouraged
to use the function to explicitly mark transfers of pointer ownerships.


### Clearing pointers

Another common pattern is to check whether a pointer is `NULL` to determine
whether some cleanup needs to be done:

```c
static Person *person = NULL;  /* May be created during execution. */

int main (int argc, char *argv[]) {
    do_things ();  /* Might assign a non-NULL value to “person”... */

    if (person) {
        person_free (person);
        person = NULL;  /* Catch use-after-free situations better. */
    }

    do_more_things ();
    return 0;
}
```

The above idiom can be replace with `ptr_clear()`, which behaves exactly
as the `if` block above:

```c
int main (int argc, char *argv[]) {
    do_things ();  /* Might assign a non-NULL value to “person”... */

    ptr_clear (&person, person_free);  /* How convenient is this?  */

    do_more_things ();
    return 0;
}
```

### More idioms

Of course, it is also possible to use `ptr_auto()` and friends in the
implementation of `Person` as well. One good pattern to follow is have
the functions that create instances use a scoped pointer, and then
[transfer its ownership](#transferring-ownership) to the caller. This ensures
that early returns, e.g. from failures during initialization, will not
leak the instance being constructed. For example, `person_new()` could
be written as:

```c
Person* person_new (const char *name) {
    ptr_auto(Person) p = calloc (1, sizeof Person);
    p->name = strdup (name);

    /* More code here that might “return NULL” early on errors.  */

    return ptr_steal (&p);  /* Transfer ownership on completion. */
}
```

Of course, following the pattern in the example above needs that calling
`person_free()` (the cleanup function) on half-initialized instances behaves
safely. It is a good practice to first initialize the allocated memory in a
way that checks can be added in the cleanup function. For example using
`calloc()` above ensures that the allocated memory is cleared before the
initialization continues.

A convenient way of making cleanup functions safe to use on half-initialized
instances is to [clear pointers](#clearing-pointers) instead of manually
writing all the needed code to do e.g. `NULL`-checks:

```c
void person_free (Person *p) {
    ptr_clear (&p->name, free);  /* No-op if “p->name” is NULL. */
    free (p);
}
```


### Other pointers

For pointers returned by functions which allocate memory using `malloc()`
by libraries which have not been modified to support automatic cleanups,
the `ptr_autofree` macro is provided, which can be assign a generic cleanup
function to the pointer that just calls `free()`.

This can go a long way to ease usage of string functions provided by the C
library:

```c
char* build_path (const char *progname) {
    ptr_autofree char *prefix = config_get ("prefix-path");
    ptr_autofree char *cache = xdg_get_cache_path ();

    char *s;
    return (asprintf (&s, "%s/%s/%s", prefix, cache, progname) == -1) ? NULL : s;
}
```

By default the function called for pointers marked with `ptr_auto` is the
`free()` function from the C library. This can be overriden by setting the
`AUTOCLEANUP_FREE_FUNC` macro to the name of a different function at build
time before including the header.


### Working with handles

Automatic cleanups for non-pointer variables can be very useful as well, and
facilities for working with such types [are provided as well](#handles). The
prime example is file descriptors, which on Unix-like systems are of type
`int`.

In the same way as for pointers, a cleanup function can be associated with
each handle type, but as many handle types may have the same base type, it may
be needed to define a type alias for each type of handle that uses a different
cleanup function. Continuing wit the file descriptor example, the following
enables automatically invoking `close()` when they go out of scope, as long
as they are valid different from `-1` (which we call the “nil value” for the
handle type):

```c
typedef int FdHandle;

/* Whenever a FdHandle goes out of scope, call close(), unless it's -1. */
HANDLE_AUTO_DEFINE (FdHandle, close, -1)
```

Next, use `handle_auto(T)` to declare an scoped handle. With definition
above file descriptors are automatically closed at the end of the code
block where they are declared, which means file descriptors are never
leaked in the function below regardless of which branch of the code is
taken to return from it:

```c
_Bool copy_file (const char *path_in, const char *path_out) {
    handle_auto(FdHandle) fd_in = open (path_in, O_RDONLY, 0);
    if (fd == -1) return false;

    struct stat sb;
    if (fstat (fd_in, &sb) == -1) return false;

    handle_auto(FdHandle) fd_out = open (path_out, O_RDWR, 0666);
    if (fd == -1) return false;

    return splice (fd_in, NULL, fd_out, NULL, sb.st_size, 0) != -1;
}
```

Conversely, there are also `handle_clear()` and `handle_steal()` functions
which work on handles. Note that it is needed to specify the “nil value”
for the handle type when using them. As an example, the following function
will automatically close the file descriptor being opened in case of errors,
and transfer its ownership to the caller on success:

```c
int open_read_async (const char *path) {
    handle_auto(FdHandle) fd = open (path, O_RDONLY, 0);
    if (fd != -1) return -1;

    int flags = fcntl (fd, F_GETFL);
    if (flags == -1) return  -1;

    flags |= (FD_CLOEXEC | O_ASYNC);
    if (fcntl (fd, F_SETFL, flags) == -1) return -1;

    return handle_steal (&fd, -1);
}
```


API
---

Note that `T` below refers to any type valid in the context of the caller.
The C language is not polymorphic, yet the implementation is typesafe due to
judicious usage of compiler extensions, preprocessor macros, and inline
functions. Arguments to function-like macros are guaranteed to be evaluated
only once and have no undersirable side-effects other than the documented
behavior.


### Pointers

```c
#define PTR_AUTO_DEFINE(T, t_ptr_free) /* ... */

#define ptr_auto(T)    /* ... */
#define ptr_autofree   /* ... */

void ptr_clear(T** pp, void (*f)(T*));
T*   ptr_steal(T** pp);
```


```c
#ifndef AUTOCLEANUP_FREE_FUNC
#define AUTOCLEANUP_FREE_FUNC free
#endif

#define ptr_autofree  /* ... */
```


### Handles

```c
#define HANDLE_AUTO_DEFINE(T, t_handle_free)  /* ... */

#define handle_auto(T)  /* ... */

void handle_clear(T* hp, void (*f)(T*), T nil_value);
T    handle_steal(T* hp, T nil_value);
```


License
-------

Distributed under the terms of the [MIT/X11
license](https://opensource.org/licenses/MIT)
