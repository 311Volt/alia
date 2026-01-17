#ifndef MDSPAN_CB4CFFE5_D7E9_43B4_B4B3_7E246EAA5522
#define MDSPAN_CB4CFFE5_D7E9_43B4_B4B3_7E246EAA5522

#ifdef ALIA_USE_KOKKOS_MDSPAN

#define MDSPAN_IMPL_STANDARD_NAMESPACE stdx
#include <mdspan/mdspan.hpp>

#else

#include <mdspan>

namespace stdx = std;

#endif


#endif /* MDSPAN_CB4CFFE5_D7E9_43B4_B4B3_7E246EAA5522 */
