#ifndef __DECL_H_
# define __DECL_H_
# ifdef __cplusplus
#  define DECL_BEGIN() extern "C" {
#  define DECL_END() }
# else
#  define DECL_BEGIN()
#  define DECL_END()
# endif
#endif
