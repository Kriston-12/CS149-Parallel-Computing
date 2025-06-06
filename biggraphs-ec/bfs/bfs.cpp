#include "bfs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cstddef>
#include <omp.h>
#include <vector>
#include <cstring> 
#include <memory>

#include "../common/CycleTimer.h"
#include "../common/graph.h"

#define ROOT_NODE_ID 0
#define NOT_VISITED_MARKER -1

void vertex_set_clear(vertex_set* list) {
    list->count = 0;
}

void vertex_set_init(vertex_set* list, int count) {
    list->max_vertices = count;
    list->vertices = (int*)malloc(sizeof(int) * list->max_vertices);
    vertex_set_clear(list);
}

void vertex_set_free(vertex_set* list) {
    free(list->vertices);
    list->vertices = NULL;
}

// Take one step of "top-down" BFS.  For each vertex on the frontier,
// follow all outgoing edges, and add all neighboring vertices to the
// new_frontier.
void top_down_step(
    Graph g,
    vertex_set* frontier,
    vertex_set* new_frontier,
    int* distances)
{

    for (int i=0; i<frontier->count; i++) {

        int node = frontier->vertices[i];

        int start_edge = g->outgoing_starts[node]; // node = 0, outgoingstarts[0] = 0 = starEdge, outgoing_edges[0] = 1
        int end_edge = (node == g->num_nodes - 1)
                           ? g->num_edges
                           : g->outgoing_starts[node + 1];

        // attempt to add all neighbors to the new frontier
        for (int neighbor=start_edge; neighbor<end_edge; neighbor++) {
            int outgoing = g->outgoing_edges[neighbor];
            if (distances[outgoing] == NOT_VISITED_MARKER) {
                distances[outgoing] = distances[node] + 1;
                int index = new_frontier->count++;
                new_frontier->vertices[index] = outgoing;
            }
        }
    }
}

void top_down_step_parallelize(
    Graph g,
    vertex_set* frontier,
    vertex_set* new_frontier,
    int* distances)
{   
    #pragma omp parallel
    {
        #pragma omp for schedule(dynamic, 100)
        for (int i = 0; i < frontier->count; i++) {
            int node = frontier->vertices[i];

            int start_edge = g->outgoing_starts[node];
            int end_edge = (node == g->num_nodes - 1)
                            ? g->num_edges
                            : g->outgoing_starts[node + 1];

            for (int neighbor = start_edge; neighbor < end_edge; neighbor++) {
                int outgoing = g->outgoing_edges[neighbor];

                // version of not using compare and swap
                // if (distances[outgoing] == NOT_VISITED_MARKER) {
                //     distances[outgoing] = distances[node] + 1;
                //     #pragma omp atomic
                //     int index = new_frontier->count++;
                //     new_frontier->vertices[index] = outgoing;
                // }

                // version using comparing and swap
                if(__sync_bool_compare_and_swap(&distances[outgoing], NOT_VISITED_MARKER, distances[node] + 1)) {
                    int index = __sync_fetch_and_add(&new_frontier->count, 1);
                    new_frontier->vertices[index] = outgoing;
                }
                
            }
        }
    }
}

void top_down_step_parallelize1(
    Graph g,
    vertex_set* frontier,
    vertex_set* new_frontier,
    int* distances)
{   
    const int num_threads = omp_get_max_threads();

    // Allocate per-thread local buffer for new frontier
    std::vector<std::vector<Vertex>> local_frontiers(num_threads);
    #pragma omp parallel for
    for (int i = 0; i < num_threads; ++i) {
        local_frontiers[i].reserve(frontier->count); // buffer size 可调
    }
    const int new_dist = distances[frontier->vertices[0]] + 1;

    #pragma omp parallel
    {   
        int tid = omp_get_thread_num();
        std::vector<Vertex>& buffer = local_frontiers[tid];

        #pragma omp for schedule(dynamic, 64)
        for (int i = 0; i < frontier->count; i++) {

            int node = frontier->vertices[i];
            const Vertex* startEdge = outgoing_begin(g, node);
            const Vertex* endEdge = outgoing_end(g, node);

            for (const Vertex* neighbor = startEdge; neighbor < endEdge; ++neighbor) {
                int target = *neighbor;

                // Use CAS to ensure only one thread updates the distance
                if (__sync_bool_compare_and_swap(&distances[target], NOT_VISITED_MARKER, new_dist)) {
                    buffer.emplace_back(target);  // Local write: cache-friendly
                }

            }
        }   
    }
    // Merge all local buffers into the global new_frontier
    int total = 0;
    for (int t = 0; t < num_threads; ++t) {
        total += local_frontiers[t].size();
    }

    // Reserve space for new frontier
    int start_index = __sync_fetch_and_add(&new_frontier->count, total);

    // Copy each thread's results into global frontier
    #pragma omp parallel for
    for (int t = 0; t < num_threads; ++t) {
        const std::vector<Vertex>& buffer = local_frontiers[t];
        int offset = 0;

        
        // Compute thread's offset within global frontier
        for (int i = 0; i < t; ++i) {
            offset += local_frontiers[i].size();
        }

        std::memcpy(
            new_frontier->vertices + start_index + offset,
            buffer.data(),
            buffer.size() * sizeof(Vertex));
    }
    
}

void top_down_step2(Graph g, vertex_set* frontier, vertex_set* new_frontier,
                   int* distances) {
  const int new_dist = distances[frontier->vertices[0]] + 1;

//   const int max_frontier_size = frontier->count;

#pragma omp parallel
  {

    // RAII
    std::unique_ptr<Vertex[]> buffer(new Vertex[g->num_nodes]);
    int buffer_size = 0;

    #pragma omp for schedule(dynamic, 512)
    for (int i = 0; i < frontier->count; i++) {
      const int node = frontier->vertices[i];

      for (const Vertex* x = outgoing_begin(g, node), * limit = outgoing_end(g, node); x < limit; x++) {

        if (__sync_bool_compare_and_swap(distances + *x, NOT_VISITED_MARKER, new_dist)) {
            buffer[buffer_size++] = *x;
        }
      }
    }

    
    int index = __sync_fetch_and_add(&new_frontier->count, buffer_size);
    std::memcpy(new_frontier->vertices + index, buffer.get(),
                buffer_size * sizeof(Vertex));
    } 
}

void top_down_step3(Graph g, vertex_set* frontier, vertex_set* new_frontier, int* distances, int level) {
#pragma omp parallel 
{
    std::unique_ptr<Vertex[]> buffer(new Vertex[g->num_nodes]);
    int buffer_size = 0;

    #pragma omp for schedule(dynamic, 512)
    for (int i = 0; i < frontier->count; i++) {
        
        const int node = frontier->vertices[i];
        for (const Vertex* x = outgoing_begin(g, node), *limit = outgoing_end(g, node); x < limit; x++) {
            // if (__sync_bool_compare_and_swap(distances + *x, NOT_VISITED_MARKER, level)) {
            //     buffer[buffer_size++] = *x;
            // }
            if (distances[*x] == NOT_VISITED_MARKER) { // This has comparable speed as the compare_and_swap above
                distances[*x] = level;
                buffer[buffer_size++] = *x;
            }
        }
    }
    int index = __sync_fetch_and_add(&new_frontier->count, buffer_size);
    std::memcpy(new_frontier->vertices + index, buffer.get(), buffer_size * sizeof(Vertex));
}
}


// Implements top-down BFS.
//
// Result of execution is that, for each node in the graph, the
// distance to the root is stored in sol.distances.
void bfs_top_down(Graph graph, solution* sol) {

    vertex_set list1;
    vertex_set list2;
    vertex_set_init(&list1, graph->num_nodes);
    vertex_set_init(&list2, graph->num_nodes);

    vertex_set* frontier = &list1;
    vertex_set* new_frontier = &list2;

    // initialize all nodes to NOT_VISITED
    #pragma omp parallel for
    for (int i=0; i<graph->num_nodes; i++)
        sol->distances[i] = NOT_VISITED_MARKER;

    // setup frontier with the root node
    frontier->vertices[frontier->count++] = ROOT_NODE_ID;
    sol->distances[ROOT_NODE_ID] = 0;

    int level = 1;
    while (frontier->count != 0) {
        // printf("frontier->count = %d\n", frontier->count);
#ifdef VERBOSE
        double start_time = CycleTimer::currentSeconds();
#endif

        vertex_set_clear(new_frontier);

        // top_down_step(graph, frontier, new_frontier, sol->distances);
        // top_down_step_parallelize(graph, frontier, new_frontier, sol->distances);
        // top_down_step2(graph, frontier, new_frontier, sol->distances);
        top_down_step3(graph, frontier, new_frontier, sol->distances, level);
        // top_down_step_parallelize1(graph, frontier, new_frontier, sol->distances);
        level++;


#ifdef VERBOSE
    double end_time = CycleTimer::currentSeconds();
    printf("frontier=%-10d %.4f sec\n", frontier->count, end_time - start_time);
#endif

        // swap pointers
        vertex_set* tmp = frontier;
        frontier = new_frontier;
        new_frontier = tmp;
    }

    vertex_set_free(&list1);
    vertex_set_free(&list2);
}


void bottomUpParallel(Graph g, vertex_set* frontier, vertex_set* new_frontier, int* distances, int currentLevel, int nextLevel)  
{
#pragma omp parallel
{   
    // printf("Enter bottomUp");
    Vertex* buffer = new Vertex[g->num_nodes];
    int buffer_size = 0;
    
    #pragma omp for schedule(dynamic, 512)
    for (int v = 0; v < g->num_nodes; v++) { // loop through 所有node，
        if (distances[v] != NOT_VISITED_MARKER) {continue;}

        for (const Vertex* u = incoming_begin(g, v), *end = incoming_end(g, v); u < end; ++u) {
            if (distances[*u] == currentLevel) {
                distances[v] = nextLevel;
                buffer[buffer_size++] = v;
                break;
            }
        }
        
    }

    int index = __sync_fetch_and_add(&new_frontier->count, buffer_size);
    // printf("Reach memcpy");
    memcpy(new_frontier->vertices + index, buffer, buffer_size * sizeof(Vertex));
    // printf("after memcpy");
    delete[] buffer;
}

}


void bfs_bottom_up(Graph graph, solution* sol)
{
    // CS149 students:
    //
    // You will need to implement the "bottom up" BFS here as
    // described in the handout.
    //
    // As a result of your code's execution, sol.distances should be
    // correctly populated for all nodes in the graph.
    //
    // As was done in the top-down case, you may wish to organize your
    // code by creating subroutine bottom_up_step() that is called in
    // each step of the BFS process.
    vertex_set list1;
    vertex_set list2;
    
    vertex_set_init(&list1, graph->num_nodes);
    vertex_set_init(&list2, graph->num_nodes); //Here i did &list1, graph->num_nodes, which means I set init list1 twice and did not initialize list2, causing seg fault

    vertex_set* frontier = &list1;
    vertex_set* new_frontier = &list2;

    #pragma omp parallel for
    for (int i=0; i < graph->num_nodes; i++)
        sol->distances[i] = NOT_VISITED_MARKER;

    frontier->vertices[frontier->count++] = ROOT_NODE_ID;
    sol->distances[ROOT_NODE_ID] = 0;

    int currentLevel = 0;
    int nextLevel = 1;
    while (frontier->count != 0) {
        vertex_set_clear(new_frontier);

        bottomUpParallel(graph, frontier, new_frontier, sol->distances, currentLevel, nextLevel);

        vertex_set* temp = frontier;
        frontier = new_frontier;
        new_frontier = temp;
        currentLevel++;
        nextLevel++;
    }

    vertex_set_free(&list1);
    vertex_set_free(&list2);
}



void bfs_hybrid(Graph graph, solution* sol)
{
    // CS149 students:
    //
    // You will need to implement the "hybrid" BFS here as
    // described in the handout.

    vertex_set list1;
    vertex_set list2;
    
    vertex_set_init(&list1, graph->num_nodes);
    vertex_set_init(&list2, graph->num_nodes);

    vertex_set* frontier = &list1;
    vertex_set* new_frontier = &list2;
    
    int totalNodes = graph->num_nodes;

    #pragma omp parallel for
    for (int i=0; i < totalNodes; i++)
        sol->distances[i] = NOT_VISITED_MARKER;

    frontier->vertices[frontier->count++] = ROOT_NODE_ID;
    sol->distances[ROOT_NODE_ID] = 0;

    int currentLevel = 0;
    int nextLevel = 1;
    while (frontier->count != 0) {
        vertex_set_clear(new_frontier);

        if (frontier->count < 0.05 * totalNodes) {
            top_down_step3(graph, frontier, new_frontier, sol->distances, nextLevel);
        }
        else {
            bottomUpParallel(graph, frontier, new_frontier, sol->distances, currentLevel, nextLevel);
        }
        
        vertex_set* temp = frontier;
        frontier = new_frontier;
        new_frontier = temp;
        currentLevel++;
        nextLevel++;
    }

    vertex_set_free(&list1);
    vertex_set_free(&list2);
}
