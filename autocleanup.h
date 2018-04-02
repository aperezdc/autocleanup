/*
 * autocleanup.h
 * Copyright (C) 2018 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef AUTOCLEANUP_H
#define AUTOCLEANUP_H

#define _AUTO_CLEANUP(_func) __attribute__((cleanup(_func)))

#define _PTR_AUTO_FNAME(_type_name)  _ptr_auto_cleanup_ ## _type_name
#define _PTR_AUTO_TNAME(_type_name)  _ptr_auto_type_ ## _type_name

#define PTR_AUTO_DEFINE(_type_name, _func)                             \
    typedef _type_name *_PTR_AUTO_TNAME(_type_name);                   \
    static inline void _PTR_AUTO_FNAME(_type_name) (_type_name **_ptr) \
    { if (_ptr) (_func) (*_ptr); }

#define ptr_auto(_type_name)                   \
    _AUTO_CLEANUP(_PTR_AUTO_FNAME(_type_name)) \
    _PTR_AUTO_TNAME(_type_name)

#define ptr_autofree _PTR_AUTO_CLEANUP(_ptr_auto_generic_free)

#ifndef AUTOCLEANUP_FREE_FUNC
#define AUTOCLEANUP_FREE_FUNC free
extern void free (void*);
#endif /* !AUTOCLEANUP_FREE_FUNC */

static inline void
_ptr_auto_generic_free (void **ptr)
{
    if (ptr) AUTOCLEANUP_FREE_FUNC (*ptr);
}


static inline void*
ptr_steal (void *pp)
{
    void **ptr = (void**) pp;
    void *ref = *ptr;
    *ptr = (void*) 0;
    return ref;
}

#define ptr_steal(_pp) ({               \
        __auto_type pp = (_pp);         \
        (__typeof(*pp)) ptr_steal (pp); \
    })


#define _HANDLE_AUTO_FNAME(_type_name)  _handle_auto_cleanup_ ## _type_name
#define _HANDLE_AUTO_TNAME(_type_name)  _handle_auto_type_ ## _type_name
#define _HANDLE_AUTO_VNIL(_type_name)   _handle_auto_nil_ ## _type_name

#define HANDLE_AUTO_DEFINE(_type_name, _func, _nil_value)                   \
    static const _type_name _HANDLE_AUTO_VNIL (_type_name) = (_nil_value);  \
    typedef _type_name _HANDLE_AUTO_TNAME (_type_name);                     \
    static inline void _HANDLE_AUTO_FNAME (_type_name) (_type_name *_ptr) { \
        if (_ptr) {                                                         \
            _type_name handle = *_ptr;                                      \
            if (handle != _HANDLE_AUTO_VNIL (_type_name)) {                 \
                *_ptr = _HANDLE_AUTO_VNIL (_type_name);                     \
                (_func) (handle);                                           \
            }                                                               \
        }                                                                   \
    }

#define handle_auto(_type_name)                   \
    _AUTO_CLEANUP(_HANDLE_AUTO_FNAME(_type_name)) \
    _HANDLE_AUTO_TNAME(_type_name)


#define handle_clear(_handle_ptr, _func, _nil_value)       \
    do {                                                   \
        __auto_type handle_ptr = (_handle_ptr);            \
        __auto_type handle = *handle_ptr;                  \
        static const __typeof(handle) vnil = (_nil_value); \
        if (handle != vnil) {                              \
            *handle_ptr = vnil;                            \
            (_func) (handle);                              \
        }                                                  \
    } while (0)

#define handle_steal(_handle_ptr, _nil_value)  ({ \
        __auto_type handle_ptr = (_handle_ptr);   \
        __auto_type handle = *handle_ptr;         \
        *handle_ptr = (_nil_value);               \
        handle;                                   \
    })

#define ptr_clear(_pp, _func)  handle_clear ((_pp), (_func), ((void*)0))

#endif /* !AUTOCLEANUP_H */
