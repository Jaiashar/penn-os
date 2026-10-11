#include "file_handle.h"
#include "pennfat_io.h"

void fh_incref(filehandle_t *file){
    file->refcount++;
}

int fh_close(filehandle_t *file) {
    return k_close(file);
}
