#ifndef KOBJECT_H
#define KOBJECT_H

#include <stdint.h>

#define KOBJ_MAX 32  /* max simultaneous kernel objects */
/* Invalid handle sentinel */
#define KOBJ_INVALID ((uint32_t)0xFFFFFFFFu)

typedef uint32_t kobj_handle_t;
typedef enum { KOBJ_QUEUE=0, KOBJ_SEM, KOBJ_MUTEX } kobj_type_t;

/* Return a generation-tagged 32-bit handle, or KOBJ_INVALID on failure. */
kobj_handle_t k_obj_alloc(void *ptr, kobj_type_t type);
void *k_obj_get(kobj_handle_t handle, kobj_type_t type);
void k_obj_free(kobj_handle_t handle);

#endif /* KOBJECT_H */