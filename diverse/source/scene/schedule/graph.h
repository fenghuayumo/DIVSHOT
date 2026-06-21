#pragma once

#include "system_descriptor.h"
#include "system_condition.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <algorithm>
#include <stack>
#include <queue>
#include <map>

namespace diverse::schedule
{

    /**
     * @brief Directed acyclic graph (DAG) for system dependencies.
     *
     * Manages system execution order through dependency tracking.
     * Provides topological sorting and parallel execution stage detection.
     * Systems are organized by stage (PreStartup -> Startup -> PostStartup
     * -> PreUpdate -> Update -> PostUpdate -> PreRender -> Render -> PostRender -> Last).
     */
    class DependencyGraph
    {
    public:
        DependencyGraph() = default;
        ~DependencyGraph() = default;

        /**
         * @brief Add a system node to the graph
         *
         * @param id Unique identifier for the system
         * @param descriptor System metadata
         */
        void add_node(size_t id, const SystemDescriptor& descriptor)
        {
            m_nodes[id] = Node{ id, descriptor, {}, {} };
            m_dirty = true;
        }

        /**
         * @brief Add a dependency edge (from -> to means 'from' must execute before 'to')
         *
         * @param from System ID that must execute first
         * @param to System ID that depends on 'from'
         */
        void add_dependency(size_t from, size_t to)
        {
            if (m_nodes.count(from) && m_nodes.count(to))
            {
                m_nodes[from].edges_to.push_back(to);
                m_nodes[to].edges_from.push_back(from);
                m_dirty = true;
            }
        }

        /**
         * @brief Build the dependency graph and validate it
         *
         * @return true if build succeeded (no cycles), false otherwise
         */
        bool build()
        {
            if (!m_dirty && !m_execution_order.empty())
            {
                return true;  // Already built and valid
            }

            m_execution_order.clear();
            m_parallel_stages.clear();

            // Check for cycles
            if (detect_cycle())
            {
                return false;
            }

            // Perform topological sort
            topological_sort();

            // Calculate parallel stages
            calculate_parallel_stages();

            m_dirty = false;
            return true;
        }

        /**
         * @brief Check if the graph has a cycle
         *
         * @return true if a cycle was detected
         */
        bool has_cycle() const
        {
            return detect_cycle();
        }

        /**
         * @brief Get the execution order (topologically sorted)
         *
         * @return Vector of system IDs in execution order
         */
        const std::vector<size_t>& get_execution_order() const
        {
            return m_execution_order;
        }

        /**
         * @brief Get parallel execution stages
         *
         * Each stage contains systems that can be executed in parallel.
         * Systems in stage N must complete before stage N+1 begins.
         *
         * @return Vector of stages, each stage is a vector of system IDs
         */
        const std::vector<std::vector<size_t>>& get_parallel_stages() const
        {
            return m_parallel_stages;
        }

        /**
         * @brief Get all node IDs
         */
        std::vector<size_t> get_node_ids() const
        {
            std::vector<size_t> ids;
            ids.reserve(m_nodes.size());
            for (const auto& [id, node] : m_nodes)
            {
                ids.push_back(id);
            }
            return ids;
        }

        /**
         * @brief Clear the graph
         */
        void clear()
        {
            m_nodes.clear();
            m_execution_order.clear();
            m_parallel_stages.clear();
            m_dirty = true;
        }

        /**
         * @brief Get the number of nodes in the graph
         */
        size_t size() const
        {
            return m_nodes.size();
        }

        /**
         * @brief Check if graph is empty
         */
        bool empty() const
        {
            return m_nodes.empty();
        }

    private:
        struct Node
        {
            size_t id;
            SystemDescriptor descriptor;
            std::vector<size_t> edges_to;     // Outgoing edges (dependencies)
            std::vector<size_t> edges_from;  // Incoming edges (dependents)
        };

        // Color enum for DFS cycle detection
        enum class Color { White, Gray, Black };

        /**
         * @brief Detect cycles using DFS with coloring
         *
         * @return true if cycle detected
         */
        bool detect_cycle() const
        {
            std::unordered_map<size_t, Color> colors;

            // Initialize all nodes as white (unvisited)
            for (const auto& [id, node] : m_nodes)
            {
                colors[id] = Color::White;
            }

            // DFS from each unvisited node
            for (const auto& [id, node] : m_nodes)
            {
                if (colors[id] == Color::White)
                {
                    if (dfs_visit(id, colors))
                    {
                        return true;  // Cycle found
                    }
                }
            }

            return false;  // No cycle
        }

        /**
         * @brief DFS visit for cycle detection
         *
         * @return true if back edge found (cycle)
         */
        bool dfs_visit(size_t node_id, std::unordered_map<size_t, Color>& colors) const
        {
            colors[node_id] = Color::Gray;  // Mark as gray (visiting)

            const auto& node = m_nodes.at(node_id);

            // Visit all neighbors
            for (size_t neighbor : node.edges_to)
            {
                auto it = colors.find(neighbor);
                Color neighbor_color = (it != colors.end()) ? it->second : Color::White;

                if (neighbor_color == Color::Gray)
                {
                    // Back edge found - cycle detected
                    return true;
                }
                if (neighbor_color == Color::White)
                {
                    if (dfs_visit(neighbor, colors))
                    {
                        return true;
                    }
                }
            }

            colors[node_id] = Color::Black;  // Mark as black (visited)
            return false;
        }

        /**
         * @brief Perform Kahn's algorithm for topological sort
         *
         * Sorts systems by stage first, then by dependencies within each stage.
         */
        void topological_sort()
        {
            // Group systems by stage
            std::map<int, std::vector<size_t>> systems_by_stage;
            for (const auto& [id, node] : m_nodes)
            {
                int stage = static_cast<int>(node.descriptor.stage);
                systems_by_stage[stage].push_back(id);
            }

            // Process each stage in order
            for (auto& [stage_int, systems] : systems_by_stage)
            {
                // Create subgraph for this stage
                std::unordered_set<size_t> stage_systems(systems.begin(), systems.end());
                std::unordered_map<size_t, size_t> in_degree;

                // Calculate in-degrees considering only edges within this stage
                for (size_t id : systems)
                {
                    size_t degree = 0;
                    const auto& node = m_nodes.at(id);
                    for (size_t from : node.edges_from)
                    {
                        if (stage_systems.count(from))
                        {
                            degree++;
                        }
                    }
                    in_degree[id] = degree;
                }

                // Initialize queue with nodes that have no incoming edges within this stage
                std::queue<size_t> queue;
                for (size_t id : systems)
                {
                    if (in_degree[id] == 0)
                    {
                        queue.push(id);
                    }
                }

                // Process nodes within this stage
                while (!queue.empty())
                {
                    size_t current = queue.front();
                    queue.pop();
                    m_execution_order.push_back(current);

                    // Reduce in-degree for all neighbors within this stage
                    for (size_t neighbor : m_nodes.at(current).edges_to)
                    {
                        if (stage_systems.count(neighbor))
                        {
                            in_degree[neighbor]--;
                            if (in_degree[neighbor] == 0)
                            {
                                queue.push(neighbor);
                            }
                        }
                    }
                }
            }
        }

        /**
         * @brief Calculate parallel execution stages
         *
         * Groups systems into stages where systems within a stage
         * can run in parallel, but stages must execute sequentially.
         */
        void calculate_parallel_stages()
        {
            if (m_execution_order.empty())
            {
                return;
            }

            // Calculate the longest path length for each node
            std::unordered_map<size_t, size_t> depth;
            for (const auto& [id, node] : m_nodes)
            {
                depth[id] = 0;
            }

            // Process nodes in topological order
            for (size_t node_id : m_execution_order)
            {
                const auto& node = m_nodes.at(node_id);
                for (size_t dep : node.edges_from)
                {
                    depth[node_id] = std::max(depth[node_id], depth[dep] + 1);
                }
            }

            // Group by depth
            size_t max_depth = 0;
            for (const auto& [id, d] : depth)
            {
                max_depth = std::max(max_depth, d);
            }

            m_parallel_stages.resize(max_depth + 1);
            for (size_t node_id : m_execution_order)
            {
                m_parallel_stages[depth[node_id]].push_back(node_id);
            }
        }


        std::unordered_map<size_t, Node> m_nodes;
        std::vector<size_t> m_execution_order;
        std::vector<std::vector<size_t>> m_parallel_stages;
        bool m_dirty = true;
    };

} // namespace diverse::schedule
