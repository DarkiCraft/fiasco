#include <fiasco/fiasco.hpp>

#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>
#include <optional>
#include <string>
#include <vector>

// --- Request Body ------------------------------------------------------------
struct filter_cfg {
    std::optional<int> min_degree;       // exclude graph nodes below this
    std::optional<double> matrix_scale;  // scale all matrix values
    std::vector<std::string> tags;       // echoed back, sorted
};
FIASCO_MODEL(filter_cfg, min_degree, matrix_scale, tags);

struct request_body {
    std::string label;
    std::vector<int> weights;  // applied to matrix rows
    filter_cfg filters;
};
FIASCO_MODEL(request_body, label, weights, filters);

// --- Response Models ---------------------------------------------------------
struct vec3 {
    double x, y, z;
};
FIASCO_MODEL(vec3, x, y, z);

struct weighted_row {
    int index;
    std::vector<double> raw;
    std::vector<double> weighted;
    double raw_sum;
    double weighted_sum;
    double dot_with_unit;
};
FIASCO_MODEL(weighted_row, index, raw, weighted, raw_sum, weighted_sum, dot_with_unit);

struct prime_info {
    int value;
    bool is_prime;
    std::optional<int> next_prime;
    std::vector<int> factors;
    int factor_count;
};
FIASCO_MODEL(prime_info, value, is_prime, next_prime, factors, factor_count);

struct graph_node {
    int id;
    std::string label;
    std::vector<int> neighbors;
    int degree;
    bool is_leaf;
    std::optional<std::string> tag;  // assigned from body.filters.tags if available
};
FIASCO_MODEL(graph_node, id, label, neighbors, degree, is_leaf, tag);

struct graph_stats {
    int node_count;
    int edge_count;
    int max_degree;
    int min_degree;
    std::vector<int> leaf_ids;
    std::vector<int> hub_ids;  // degree > mean
};
FIASCO_MODEL(graph_stats, node_count, edge_count, max_degree, min_degree, leaf_ids, hub_ids);

struct char_freq {
    std::string ch;
    int count;
};
FIASCO_MODEL(char_freq, ch, count);

struct label_analysis {
    std::string original;
    int length;
    std::vector<char_freq> freq;
    bool is_palindrome;
    std::optional<std::string> longest_run;  // longest consecutive same-char run
};
FIASCO_MODEL(label_analysis, original, length, freq, is_palindrome, longest_run);

struct mode_info {
    int index;
    double value;
};
FIASCO_MODEL(mode_info, index, value);

struct statistics {
    double min;
    double max;
    double mean;
    double variance;
    double std_dev;
    mode_info mode;
    double weighted_mean;
};
FIASCO_MODEL(statistics, min, max, mean, variance, std_dev, mode, weighted_mean);

struct response {
    // path params echoed
    int seed;
    int multiplier;

    // body echoed + processed
    std::string label;
    std::vector<std::string> sorted_tags;
    std::optional<double> matrix_scale;

    // geometry
    vec3 unit_vector;

    // matrix
    std::vector<weighted_row> matrix;
    statistics stats;

    // primes
    std::vector<prime_info> primes;
    std::optional<std::string> secret;

    // graph
    std::vector<graph_node> graph;
    graph_stats g_stats;

    // label
    label_analysis analysis;

    // combined seed*multiplier derived value
    int combined;
    bool combined_is_prime;
    std::vector<int> combined_factors;
};
FIASCO_MODEL(response,
             seed,
             multiplier,
             label,
             sorted_tags,
             matrix_scale,
             unit_vector,
             matrix,
             stats,
             primes,
             secret,
             graph,
             g_stats,
             analysis,
             combined,
             combined_is_prime,
             combined_factors);

// --- Helpers -----------------------------------------------------------------
bool chk_prime(int n) {
    if (n < 2) {
        return false;
    }
    for (int i = 2; i <= (int)std::sqrt((double)n); ++i) {
        if (n % i == 0) {
            return false;
        }
    }
    return true;
}

std::optional<int> nxt_prime(int n) {
    for (int i = n + 1; i < n + 1000; ++i) {
        if (chk_prime(i)) {
            return i;
        }
    }
    return std::nullopt;
}

std::vector<int> factorize(int n) {
    std::vector<int> f;
    for (int i = 2; i <= n; ++i) {
        while (n % i == 0) {
            f.push_back(i);
            n /= i;
        }
    }
    return f;
}

int main() {
    fiasco::server app;

    app.post("/{seed}/{multiplier}", [](int seed, int multiplier, request_body body) -> response {
        double scale = body.filters.matrix_scale.value_or(1.0);

        // pad or trim weights to 4
        auto w = body.weights;
        w.resize(4, 1);

        // unit vector from seed
        double angle = seed * 0.1;
        vec3 uv{std::cos(angle), std::sin(angle), std::cos(angle) * std::sin(angle)};
        double mag = std::sqrt(uv.x * uv.x + uv.y * uv.y + uv.z * uv.z);
        uv.x /= mag;
        uv.y /= mag;
        uv.z /= mag;

        // 4x4 matrix
        std::vector<weighted_row> matrix;
        std::vector<double> flat_raw, flat_weighted;
        for (int i = 0; i < 4; ++i) {
            std::vector<double> raw, wted;
            for (int j = 0; j < 4; ++j) {
                double v = std::sin(seed * (i + 1.0) * (j + 1.0)) * scale;
                raw.push_back(v);
                wted.push_back(v * w[i] * multiplier);
            }
            double rsum = std::accumulate(raw.begin(), raw.end(), 0.0);
            double wsum = std::accumulate(wted.begin(), wted.end(), 0.0);
            double dot = raw[0] * uv.x + raw[1] * uv.y + raw[2] * uv.z;  // 3 components
            matrix.push_back({i, raw, wted, rsum, wsum, dot});
            for (auto v : raw) {
                flat_raw.push_back(v);
            }
            for (auto v : wted) {
                flat_weighted.push_back(v);
            }
        }

        // stats
        auto& fr = flat_raw;
        double fmin = *std::min_element(fr.begin(), fr.end());
        double fmax = *std::max_element(fr.begin(), fr.end());
        double fmean = std::accumulate(fr.begin(), fr.end(), 0.0) / fr.size();
        double var = 0;
        for (auto v : fr) {
            var += (v - fmean) * (v - fmean);
        }
        var /= fr.size();

        auto mode_it = std::min_element(fr.begin(), fr.end(), [&](double a, double b) {
            return std::abs(a - fmean) < std::abs(b - fmean);
        });
        int mode_idx = std::distance(fr.begin(), mode_it);

        double wmean =
            std::accumulate(flat_weighted.begin(), flat_weighted.end(), 0.0) / flat_weighted.size();

        statistics stats{fmin, fmax, fmean, var, std::sqrt(var), {mode_idx, *mode_it}, wmean};

        // primes around seed
        std::vector<prime_info> primes;
        for (int off = -2; off <= 2; ++off) {
            int v = std::abs(seed + off);
            auto f = factorize(v);
            primes.push_back({v, chk_prime(v), nxt_prime(v), f, (int)f.size()});
        }

        std::optional<std::string> secret;
        if (chk_prime(seed)) {
            secret = "seed " + std::to_string(seed) + " is prime. you found it.";
        }

        // graph — filter by min_degree if set
        int min_deg = body.filters.min_degree.value_or(0);
        auto& tags = body.filters.tags;

        std::vector<graph_node> graph;
        int edge_count = 0, max_deg = 0, min_deg_seen = INT_MAX;
        std::vector<int> leaf_ids, hub_ids;

        // build raw nodes first to compute mean degree for hub detection
        struct raw_node {
            int id;
            std::vector<int> neighbors;
        };
        std::vector<raw_node> raw_nodes;
        for (int i = 0; i < 5; ++i) {
            std::vector<int> neighbors;
            for (int j = 0; j < 5; ++j) {
                if (i != j && (seed * (i + 1) * (j + 1)) % 3 == 0) {
                    neighbors.push_back(j);
                }
            }
            raw_nodes.push_back({i, neighbors});
        }

        double mean_deg = 0;
        for (auto& n : raw_nodes) {
            mean_deg += n.neighbors.size();
        }
        mean_deg /= raw_nodes.size();

        for (auto& rn : raw_nodes) {
            int deg = (int)rn.neighbors.size();
            if (deg < min_deg) {
                continue;
            }

            std::optional<std::string> tag;
            if ((size_t)rn.id < tags.size()) {
                tag = tags[rn.id];
            }

            edge_count += deg;
            max_deg = std::max(max_deg, deg);
            min_deg_seen = std::min(min_deg_seen, deg);

            bool is_leaf = deg == 1;
            if (is_leaf) {
                leaf_ids.push_back(rn.id);
            }
            if (deg > mean_deg) {
                hub_ids.push_back(rn.id);
            }

            graph.push_back(
                {rn.id, "node_" + std::to_string(rn.id), rn.neighbors, deg, is_leaf, tag});
        }
        edge_count /= 2;  // undirected

        graph_stats g_stats{(int)graph.size(),
                            edge_count,
                            max_deg,
                            min_deg_seen == INT_MAX ? 0 : min_deg_seen,
                            leaf_ids,
                            hub_ids};

        // label analysis
        std::string lbl = body.label;
        std::map<char, int> freq_map;
        for (char c : lbl) {
            freq_map[c]++;
        }
        std::vector<char_freq> freq;
        for (auto& [c, cnt] : freq_map) {
            freq.push_back({std::string(1, c), cnt});
        }

        bool is_palindrome = lbl == std::string(lbl.rbegin(), lbl.rend());

        std::optional<std::string> longest_run;
        if (!lbl.empty()) {
            char cur = lbl[0];
            int run = 1, best = 1;
            char best_c = cur;
            for (size_t i = 1; i < lbl.size(); ++i) {
                if (lbl[i] == cur) {
                    ++run;
                    if (run > best) {
                        best = run;
                        best_c = cur;
                    }
                } else {
                    cur = lbl[i];
                    run = 1;
                }
            }
            longest_run = std::string(best, best_c);
        }

        label_analysis analysis{lbl, (int)lbl.size(), freq, is_palindrome, longest_run};

        // combined
        int combined = std::abs(seed * multiplier);
        auto cf = factorize(combined);

        // sorted tags
        auto sorted_tags = body.filters.tags;
        std::sort(sorted_tags.begin(), sorted_tags.end());

        return {seed,
                multiplier,
                body.label,
                sorted_tags,
                body.filters.matrix_scale,
                uv,
                matrix,
                stats,
                primes,
                secret,
                graph,
                g_stats,
                analysis,
                combined,
                chk_prime(combined),
                cf};
    });

    app.run(8080);
}