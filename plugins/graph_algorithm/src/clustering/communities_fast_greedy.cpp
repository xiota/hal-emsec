#include "graph_algorithm/plugin_graph_algorithm.h"
#include "hal_core/netlist/gate.h"
#include "hal_core/netlist/net.h"
#include "hal_core/netlist/netlist.h"
#include "hal_core/plugin_system/plugin_manager.h"
#include "hal_core/utilities/log.h"

#include <igraph/igraph.h>
#include <tuple>

namespace hal
{
    std::map<int, std::set<Gate*>> GraphAlgorithmPlugin::get_communities_fast_greedy(Netlist* nl)
    {
        if (nl == nullptr)
        {
            log_error(this->get_name(), "{}", "parameter 'nl' is nullptr");
            return std::map<int, std::set<Gate*>>();
        }
        // get igraph
        igraph_t graph;
        std::map<int, Gate*> vertex_to_gate = get_igraph_directed(nl, &graph);

        igraph_vector_int_t membership, modularity;
        igraph_matrix_t merges;

        igraph_to_undirected(&graph, IGRAPH_TO_UNDIRECTED_MUTUAL, 0);

        igraph_vector_int_init(&membership, 1);

        igraph_community_fastgreedy(&graph,
                                    nullptr, /* no weights */
                                    nullptr,
                                    nullptr,
                                    &membership);

        // map back to HAL structures
        std::map<int, std::set<Gate*>> community_sets;
        for (igraph_integer_t i = 0; i < igraph_vector_int_size(&membership); i++)
        {
            community_sets[VECTOR(membership)[i]].insert(vertex_to_gate[i]);
        }
        //igraph_vector_destroy(&membership);

        igraph_destroy(&graph);
        igraph_vector_int_destroy(&membership);

        return community_sets;
    }
}    // namespace hal
