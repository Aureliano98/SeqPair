// boost_test.cpp: tests implementation correctness of SeqPair project
//      with Boost.Test library.
// Author: LYL (Aureliano Lee)

#define BOOST_TEST_MODULE seqpair_tests

#include <boost/test/included/unit_test.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/dag_shortest_paths.hpp>
#include <boost/graph/dijkstra_shortest_paths.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/property_map/property_map.hpp>
#include "layout.h"
#include "pack_generator.h"

#define SERIALIZE_GENERATOR_BASE_TESTS

using namespace std;
namespace {
    template<typename GeneratorBase>
    struct DebugGenerator : public GeneratorBase {
        using base_t = GeneratorBase;

        template<typename... Types>
        DebugGenerator(Types &&...args) : GeneratorBase(std::forward<Types>(args)...) {}

        decltype(auto) sp_x() {
            return (base_t::_sp_x);
        }

        decltype(auto) sp_y() {
            return (base_t::_sp_y);
        }

        decltype(auto) widths() {
            return (base_t::_widths);
        }

        decltype(auto) heights() {
            return (base_t::_heights);
        }

        template<typename... Types>
        auto eval(Types &&...args) {
            return base_t::_eval(std::forward<Types>(args)...);
        }
    };
}

BOOST_AUTO_TEST_SUITE(seqpair_tests)
BOOST_AUTO_TEST_CASE(dijkstra_shortest_paths_test) {
    using namespace boost;
    typedef adjacency_list < listS, vecS, directedS,
        no_property, property < edge_weight_t, int > > graph_t;
    typedef graph_traits < graph_t >::vertex_descriptor vertex_descriptor;
    typedef std::pair<int, int> Edge;

    const int num_nodes = 5;
    enum nodes { A, B, C, D, E };
    char name[] = "ABCDE";
    Edge edge_array[] = { Edge(A, C), Edge(B, B), Edge(B, D), Edge(B, E),
        Edge(C, B), Edge(C, D), Edge(D, E), Edge(E, A), Edge(E, B)
    };
    int weights[] = { 1, 2, 1, 2, 7, 3, 1, 1, 1 };
    int num_arcs = sizeof(edge_array) / sizeof(Edge);
    graph_t g(edge_array, edge_array + num_arcs, weights, num_nodes);
    //property_map<graph_t, edge_weight_t>::type weightmap = 
    //    get(edge_weight, g);
    std::vector<vertex_descriptor> p(num_vertices(g));
    std::vector<int> d(num_vertices(g));
    vertex_descriptor s = vertex(A, g);

    dijkstra_shortest_paths(g, s,
        predecessor_map(boost::make_iterator_property_map(p.begin(), 
            get(boost::vertex_index, g))).
        distance_map(boost::make_iterator_property_map(d.begin(), 
            get(boost::vertex_index, g))));

    std::cout << "distances and parents:" << std::endl;
    graph_traits < graph_t >::vertex_iterator vi, vend;
    for (boost::tie(vi, vend) = vertices(g); vi != vend; ++vi) {
        std::cout << "distance(" << name[*vi] << ") = " << d[*vi] << ", ";
        std::cout << "parent(" << name[*vi] << ") = " << name[p[*vi]] << 
            std::endl;
    }
    std::cout << std::endl;
    BOOST_TEST(p[s] == s);
}

BOOST_AUTO_TEST_CASE(dag_shortest_paths_test) {
    using namespace boost;
    typedef adjacency_list<vecS, vecS, directedS,
        property<vertex_distance_t, int>, 
        property<edge_weight_t, int> > graph_t;
    graph_t g(6);
    enum verts { r, s, t, u, v, x };
    char name[] = "rstuvx";
    add_edge(r, s, 5, g);
    add_edge(r, t, 3, g);
    add_edge(s, t, 2, g);
    add_edge(s, u, 6, g);
    add_edge(t, u, 7, g);
    add_edge(t, v, 4, g);
    add_edge(t, x, 2, g);
    add_edge(u, v, -1, g);
    add_edge(u, x, 1, g);
    add_edge(v, x, -2, g);

    property_map<graph_t, vertex_distance_t>::type
        d_map = get(vertex_distance, g);

    dag_shortest_paths(g, s, distance_map(d_map));

    graph_traits<graph_t>::vertex_iterator vi, vi_end;
    for (boost::tie(vi, vi_end) = vertices(g); vi != vi_end; ++vi)
        if (d_map[*vi] == (std::numeric_limits<int>::max)())
            std::cout << name[*vi] << ": inifinity\n";
        else
            std::cout << name[*vi] << ": " << d_map[*vi] << '\n';
    
    BOOST_TEST(true);
}

BOOST_AUTO_TEST_CASE(make_left_inverse_test) {
    using rect_packing::detail::make_left_inverse;
    constexpr size_t test_size = 1 << 10;
    default_random_engine eng(random_device{}());

    vector<int> x(test_size), inv_x(test_size);
    iota(begin(x), end(x), 0);
    shuffle(begin(x), end(x), eng);
    make_left_inverse(begin(x), end(x), begin(inv_x));
    for (int i = 0; i != static_cast<int>(test_size); ++i)
        BOOST_TEST(inv_x[x[i]] == i);
}

BOOST_AUTO_TEST_CASE(make_match_test) {
    using rect_packing::detail::make_match;
    constexpr size_t test_size = 1 << 10;
    default_random_engine eng(random_device{}());

    vector<int> x(test_size), y(test_size), p(test_size), buffer(test_size);
    iota(begin(x), end(x), 0);
    std::copy(cbegin(x), cend(x), begin(y));
    shuffle(begin(x), end(x), eng);
    shuffle(begin(y), end(y), eng);
    make_match(begin(y), end(y), begin(x), begin(p), begin(buffer));
    for (int i = 0; i != static_cast<int>(x.size()); ++i)
        BOOST_TEST(x[i] == y[p[i]]);
}

BOOST_AUTO_TEST_CASE(eval_sp2_test) {
    vector<int> x{ 4,3,1,6,2,5 };
    vector<int> y{ 6,3,5,4,1,2 };
    vector<int> len{ 4,3,3,2,4,6 };
    // Should start with 0.
    for (auto &e : x)
        --e;
    for (auto &e : y)
        --e;
    auto pos = x;
    auto match = x;
    auto buffer = x;
    auto ans = rect_packing::detail::eval_sp2(begin(y), end(y), begin(x), begin(len),
        begin(pos), begin(buffer), begin(match), map<ptrdiff_t, ptrdiff_t>());
    vector<int> expected{ 3, 7, 0, 0, 6, 0 };
    BOOST_TEST(pos == expected);
    BOOST_TEST(ans == 10);
}

BOOST_AUTO_TEST_CASE(LcsGeneratorBase_test) {
    using namespace rect_packing;
    vector<pair<int, int>> components{
        {4, 6}, {3, 7}, {3, 3},
        {2, 3}, {4, 3}, {6, 4}
    };
    Layout<> layout;
    for (auto c : components)
        layout.push(c);
    
    DebugGenerator<detail::LcsPackGeneratorBase<>> lcs_gen;
    lcs_gen.widths() = layout.widths();
    lcs_gen.heights() = layout.heights();
    lcs_gen.sp_x() = { 4, 3, 1, 6, 2, 5 };
    lcs_gen.sp_y() = { 6, 3, 5, 4, 1, 2 };
    for (auto &e : lcs_gen.sp_x())
        --e;
    for (auto &e : lcs_gen.sp_y())
        --e;
    BOOST_TEST(lcs_gen.sp_x().size() == components.size());
    BOOST_TEST(lcs_gen.sp_y().size() == components.size());
    auto res = lcs_gen.make_resource();
    default_random_engine eng;
    int w, h;
    tie(w, h) = lcs_gen.eval(layout, eng, res, allocator<void>());
#ifdef SERIALIZE_GENERATOR_BASE_TESTS
    {
        ofstream out("LcsPackGeneratorBase_test.txt");
        out << layout.format(decltype(layout)::format_policy::no_delim) << endl;
    }
#endif
    BOOST_TEST(w == 10);
    BOOST_TEST(h == 10);
}

BOOST_AUTO_TEST_CASE(DagPackGeneratorBase_test) {
    using namespace rect_packing;
    vector<pair<int, int>> components{
        { 4, 6 },{ 3, 7 },{ 3, 3 },
    { 2, 3 },{ 4, 3 },{ 6, 4 }
    };
    Layout<> layout;
    for (auto c : components)
        layout.push(c);

    DebugGenerator<detail::DagPackGeneratorBase<>> dag_gen;
    dag_gen.widths() = layout.widths();
    dag_gen.heights() = layout.heights();
    dag_gen.sp_x() = { 4, 3, 1, 6, 2, 5 };
    dag_gen.sp_y() = { 6, 3, 5, 4, 1, 2 };
    for (auto &e : dag_gen.sp_x())
        --e;
    for (auto &e : dag_gen.sp_y())
        --e;
    BOOST_TEST(dag_gen.sp_x().size() == components.size());
    BOOST_TEST(dag_gen.sp_y().size() == components.size());
    auto res = dag_gen.make_resource();
    default_random_engine eng;
    int w, h;
    tie(w, h) = dag_gen.eval(layout, eng, res);
#ifdef SERIALIZE_GENERATOR_BASE_TESTS
    {
        ofstream out("DagPackGeneratorBase_test.txt");
        out << layout.format(decltype(layout)::format_policy::no_delim) << endl;
    }
#endif
    BOOST_TEST(w == 10);
    BOOST_TEST(h == 10);
}

BOOST_AUTO_TEST_SUITE_END()