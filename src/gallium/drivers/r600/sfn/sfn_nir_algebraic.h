#ifndef R600_NIR_ALGEBRAIC_H
#define R600_NIR_ALGEBRAIC_H

#include <nir.h>

#ifdef __cplusplus
extern "C" {
#endif

bool r600_nir_opt_fsub_fmul(nir_shader *shader);

#ifdef __cplusplus
}
#endif

#endif // R600_NIR_ALGEBRAIC_H
