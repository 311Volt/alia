#include "vertex_definition_registry_test_shared.hpp"

namespace alia::vertex_definition_registry_test {

    std::size_t shared_vertex_index_from_other_translation_unit() {
        return detail::vertex_definition_of<shared_vertex>().index;
    }

} // namespace alia::vertex_definition_registry_test
