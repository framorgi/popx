#include "brain.h"

#include "neuron.h"

#include <Eigen/Dense>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <vector>

// ---------------------------------------------------------------------------
// Construction / allocation
// ---------------------------------------------------------------------------

Brain::Brain(std::shared_ptr<IConfig> config) : config_(std::move(config)) {
    size_s_ = config_->get_brain_size_s();
    size_n_ = config_->get_brain_size_n();
    size_y_ = config_->get_brain_size_y();
    num_hidden_ = config_->get_brain_num_hidden_layers();
    num_layers_ = num_hidden_ + 2;
    allocate();
}

void Brain::allocate() {
    M_.clear();
    M_.resize(num_layers_ * num_layers_);
    eligibility_.assign(num_layers_ * num_layers_, {});
    learning_stats_ = {};
    activations_.clear();
    for (unsigned src = 0; src < num_layers_; ++src) {
        for (unsigned dst = src + 1; dst < num_layers_; ++dst) {
            M_[src * num_layers_ + dst] =
                Eigen::SparseMatrix<float>(static_cast<int>(layer_size(dst)), static_cast<int>(layer_size(src)));
        }
    }
}

unsigned Brain::layer_size(unsigned k) const {
    if (k == 0)
        return size_s_;
    if (k == num_layers_ - 1)
        return size_y_;
    return size_n_;
}

// ---------------------------------------------------------------------------
// IBrain interface
// ---------------------------------------------------------------------------

void Brain::wire(const Genome& genome) {
    // Reset Hebbian-learning state; will be repopulated on the next hebbian_update().
    learned_genome_ = nullptr;
    learned_genome_dirty_ = false;
    silent_genes_.clear();
    discarded_genes_.clear();

    using Triplet = Eigen::Triplet<float>;

    const unsigned nl = num_layers_;
    std::vector<std::vector<Triplet>> triplets(nl * nl);

    for (const Gene& g : genome.get_genes()) {
        const unsigned src_type = g.get_source_type(); // 0-7
        const unsigned snk_type = g.get_sink_type();   // 0-7

        // Gene type encoding:
        //   0      = SENSOR  (layer 0)
        //   1..H   = HIDDEN_1..H  (layers 1..H)
        //   7      = OUTPUT  (layer H+1, always fixed at value 7)
        //   H+1..6 = unused/silent for current H

        // Validate source: must be sensor(0) or an active hidden layer(1..H)
        const bool src_ok = (src_type == 0) || (src_type >= 1 && src_type <= num_hidden_);
        if (!src_ok) {
            discarded_genes_.push_back(g);
            continue;
        }

        const unsigned src_layer = src_type;

        // Validate sink: active hidden layer(1..H) or output(7)
        unsigned dst_layer;
        if (snk_type == 7) {
            dst_layer = num_hidden_ + 1;
        } else if (snk_type >= 1 && snk_type <= num_hidden_) {
            dst_layer = snk_type;
        } else {
            discarded_genes_.push_back(g);
            continue; // sensor(0) or unused hidden layer as sink
        }

        // Enforce feedforward: sink must be strictly deeper than source
        if (dst_layer <= src_layer) {
            discarded_genes_.push_back(g);
            continue;
        }

        // Reject genes whose num field exceeds the actual layer size.
        // Using modulo would create a coverage bias: e.g. with sizeY=19 and
        // a domain of 32 indices, indices 0-12 would be selected twice as
        // often as 13-18.  Rejection keeps the distribution uniform.
        // Index-overflow genes are preserved as silent genes: they may become
        // active in future generations if layer sizes grow via structural mutation.
        const unsigned raw_src = g.get_source_num();
        if (raw_src >= layer_size(src_layer)) {
            silent_genes_.push_back(g);
            continue;
        }

        const unsigned raw_snk = g.get_sink_num();
        if (raw_snk >= layer_size(dst_layer)) {
            silent_genes_.push_back(g);
            continue;
        }

        const int src_neuron = static_cast<int>(raw_src);
        const int dst_neuron = static_cast<int>(raw_snk);

        triplets[src_layer * nl + dst_layer].emplace_back(dst_neuron, src_neuron, g.get_weight_as_float());
    }

    allocate();
    for (unsigned src = 0; src < nl; ++src) {
        for (unsigned dst = src + 1; dst < nl; ++dst) {
            auto& t = triplets[src * nl + dst];
            if (!t.empty())
                M_[src * nl + dst].setFromTriplets(t.begin(), t.end());
            eligibility_[src * nl + dst].assign(M_[src * nl + dst].nonZeros(), 0.0f);
        }
    }
    std::size_t count = 0, zeros = 0;
    double absolute = 0;
    for (const auto& matrix : M_)
        for (Eigen::Index i = 0; i < matrix.nonZeros(); ++i) {
            const float weight = matrix.valuePtr()[i];
            absolute += std::abs(weight);
            zeros += weight == 0;
            ++count;
        }
    learning_stats_.mean_abs_weight = count ? absolute / count : 0;
    learning_stats_.zero_fraction = count ? double(zeros) / count : 0;
}

int Brain::feed_forward(const std::vector<float>& sensor_values) {
    const unsigned nl = num_layers_;
    std::vector<Eigen::VectorXf> h(nl);

    // Layer 0: apply sensor inputs
    h[0] = Eigen::VectorXf::Zero(static_cast<int>(size_s_));
    for (unsigned i = 0; i < size_s_ && i < sensor_values.size(); ++i)
        h[0](static_cast<int>(i)) = sensor_values[i];

    // Layers 1..H+1: sum weighted contributions from all prior layers, apply sigmoid
    for (unsigned dst = 1; dst < nl; ++dst) {
        Eigen::VectorXf x = Eigen::VectorXf::Zero(static_cast<int>(layer_size(dst)));
        for (unsigned src = 0; src < dst; ++src) {
            const auto& M = M_[src * nl + dst];
            if (M.nonZeros() > 0)
                x += M * h[src];
        }
        for (int i = 0; i < x.size(); ++i)
            x(i) = sigmoid(x(i));
        h[dst] = std::move(x);
    }
    // Store activations for potential Hebbian updates after action execution
    activations_ = h;
    // A neutral or tied output is not a request to move forward.
    const auto& out = h[nl - 1];
    const int idle = static_cast<int>(Action::DO_NOTHING);
    if (out.size() == 0 || !out.allFinite() || (out.array() == 0.5f).all() || (out.array() == 0.0f).all())
        return idle;
    int max_index = -1;
    float max_value = 0.0f;
    bool tied = false;
    for (int i = 0; i < out.size(); ++i) {
        if (max_index < 0 || out(i) > max_value) {
            max_value = out(i);
            max_index = i;
            tied = false;
        } else if (out(i) == max_value)
            tied = true;
    }
    return tied ? idle : max_index;
}

void Brain::resize(unsigned size_s, unsigned size_n, unsigned size_y, unsigned num_hidden) {
    size_s_ = size_s;
    size_n_ = size_n;
    size_y_ = size_y;
    num_hidden_ = num_hidden;
    num_layers_ = num_hidden_ + 2;
    allocate();
    learned_genome_ = nullptr;
    learned_genome_dirty_ = false;
    silent_genes_.clear();
    discarded_genes_.clear();
}

// Legacy entry points retained for source compatibility. Only explicit snapshots persist.
bool Brain::remove_serialization(const std::string&) {
    return true;
}
void Brain::serialize(const std::string&, unsigned) const {}
nlohmann::json Brain::snapshot() const {
    using json = nlohmann::json;
    json doc;
    doc["sizeS"] = size_s_;
    doc["sizeN"] = size_n_;
    doc["sizeY"] = size_y_;
    doc["num_hidden_layers"] = num_hidden_;
    const auto encoding = Genome::encoding_from_config(*config_);
    doc["gene_encoding"] = {{"version", 2},
                            {"index_bits", 32},
                            {"source_index_count", encoding.source_index_count},
                            {"sink_index_count", encoding.sink_index_count}};

    json connections = json::array();
    const unsigned nl = num_layers_;
    for (unsigned src = 0; src < nl; ++src) {
        for (unsigned dst = src + 1; dst < nl; ++dst) {
            const auto& M = M_[src * nl + dst];
            if (M.nonZeros() == 0)
                continue;
            json conn;
            conn["from_layer"] = src;
            conn["to_layer"] = dst;
            json entries = json::array();
            for (int k = 0; k < M.outerSize(); ++k) {
                for (Eigen::SparseMatrix<float>::InnerIterator it(M, k); it; ++it) {
                    entries.push_back({{"row", it.row()}, {"col", it.col()}, {"value", it.value()}});
                }
            }
            conn["entries"] = std::move(entries);
            connections.push_back(std::move(conn));
        }
    }
    doc["connections"] = std::move(connections);

    doc["schema_version"] = 1;
    doc["activations"] = json::array();
    for (const auto& layer : activations_) {
        std::vector<float> values(layer.data(), layer.data() + layer.size());
        doc["activations"].push_back(values);
    }
    doc["silent_genes"] = json::array();
    doc["discarded_genes"] = json::array();
    for (const auto& g : silent_genes_)
        doc["silent_genes"].push_back(
            {g.get_source_type(), g.get_source_num(), g.get_sink_type(), g.get_sink_num(), g.get_weight()});
    for (const auto& g : discarded_genes_)
        doc["discarded_genes"].push_back(
            {g.get_source_type(), g.get_source_num(), g.get_sink_type(), g.get_sink_num(), g.get_weight()});
    return doc;
}

// ---------------------------------------------------------------------------
// Getters
// ---------------------------------------------------------------------------

unsigned Brain::get_size_s() const {
    return size_s_;
}
unsigned Brain::get_size_n() const {
    return size_n_;
}
unsigned Brain::get_size_y() const {
    return size_y_;
}
unsigned Brain::get_num_hidden() const {
    return num_hidden_;
}

std::size_t Brain::get_total_connections() const {
    std::size_t total = 0;
    const unsigned nl = num_layers_;
    for (unsigned src = 0; src < nl; ++src) {
        for (unsigned dst = src + 1; dst < nl; ++dst) {
            total += static_cast<std::size_t>(M_[src * nl + dst].nonZeros());
        }
    }
    return total;
}

std::size_t Brain::get_useful_connection_count(float epsilon) const {
    std::size_t useful = 0;
    const unsigned nl = num_layers_;
    const bool has_activations = activations_.size() == nl;
    for (unsigned src = 0; src < nl; ++src) {
        for (unsigned dst = src + 1; dst < nl; ++dst) {
            const auto& M = M_[src * nl + dst];
            if (M.nonZeros() == 0) {
                continue;
            }
            for (int k = 0; k < M.outerSize(); ++k) {
                for (Eigen::SparseMatrix<float>::InnerIterator it(M, k); it; ++it) {
                    float score = 0.0f;
                    if (has_activations) {
                        score = std::abs(it.value() * activations_[src](it.col()) * activations_[dst](it.row()));
                    }
                    if (score >= epsilon) {
                        ++useful;
                    }
                }
            }
        }
    }
    return useful;
}

std::vector<BrainConnectionActivity> Brain::get_top_active_connections(float epsilon, std::size_t limit) const {
    std::vector<BrainConnectionActivity> all;
    if (limit == 0) {
        return all;
    }

    const unsigned nl = num_layers_;
    const bool has_activations = activations_.size() == nl;
    if (!has_activations) {
        return all;
    }

    for (unsigned src = 0; src < nl; ++src) {
        for (unsigned dst = src + 1; dst < nl; ++dst) {
            const auto& M = M_[src * nl + dst];
            if (M.nonZeros() == 0) {
                continue;
            }
            for (int k = 0; k < M.outerSize(); ++k) {
                for (Eigen::SparseMatrix<float>::InnerIterator it(M, k); it; ++it) {
                    const float score =
                        std::abs(it.value() * activations_[src](it.col()) * activations_[dst](it.row()));
                    if (score < epsilon) {
                        continue;
                    }
                    all.push_back(BrainConnectionActivity{src, static_cast<unsigned>(it.col()), dst,
                                                          static_cast<unsigned>(it.row()), it.value(), score});
                }
            }
        }
    }

    std::sort(all.begin(), all.end(), [](const BrainConnectionActivity& a, const BrainConnectionActivity& b) {
        return a.activity_score > b.activity_score;
    });

    if (all.size() > limit) {
        all.resize(limit);
    }
    return all;
}

std::vector<bool> Brain::get_connected_sensors() const {
    std::vector<bool> connected(size_s_, false);
    const unsigned nl = num_layers_;
    for (unsigned dst = 1; dst < nl; ++dst) {
        const auto& M = M_[0 * nl + dst];
        if (M.nonZeros() == 0)
            continue;
        for (int k = 0; k < M.outerSize(); ++k)
            for (Eigen::SparseMatrix<float>::InnerIterator it(M, k); it; ++it)
                if (static_cast<unsigned>(it.col()) < size_s_)
                    connected[static_cast<unsigned>(it.col())] = true;
    }
    return connected;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

float Brain::sigmoid(float x) const {
    if (x >= 0.0f)
        return 1.0f / (1.0f + std::exp(-x));
    const float ex = std::exp(x);
    return ex / (1.0f + ex);
}

void Brain::record_action(int action) {
    if (activations_.size() != num_layers_)
        return;
    // One action every three phases: normalized accumulating trace, bounded for sustained activity.
    const float decay = static_cast<float>(config_->get_learning_trace_decay());
    const float injection = 1.0f - decay * decay * decay;
    for (unsigned src = 0; src < num_layers_; ++src) {
        for (unsigned dst = src + 1; dst < num_layers_; ++dst) {
            const auto& matrix = M_[src * num_layers_ + dst];
            auto& trace = eligibility_[src * num_layers_ + dst];
            std::size_t index = 0;
            for (int k = 0; k < matrix.outerSize(); ++k) {
                for (Eigen::SparseMatrix<float>::InnerIterator it(matrix, k); it; ++it, ++index) {
                    // Only the chosen actuator receives new output credit. Hidden activity is a local approximation.
                    if (dst == num_layers_ - 1 && it.row() != action)
                        continue;
                    const float pre = activations_[src](it.col());
                    const float post = activations_[dst](it.row());
                    // Chosen-output local gradient remains responsive around sigmoid 0.5.
                    // Hidden credit remains a local centered approximation, not backpropagation.
                    const float activity =
                        dst == num_layers_ - 1 ? pre * post * (1.0f - post) : (pre - 0.5f) * (post - 0.5f);
                    trace[index] = std::clamp(trace[index] + injection * activity, -0.25f, 0.25f);
                }
            }
        }
    }
}

void Brain::hebbian_update(float reward) {
    if (activations_.size() != num_layers_ || !std::isfinite(reward))
        return;
    const float eta = static_cast<float>(config_->get_learning_rate());
    const float trace_decay = static_cast<float>(config_->get_learning_trace_decay());
    double changes = 0, plasticity = 0, decay_sum = 0, traces = 0, weights = 0;
    std::size_t count = 0, zeros = 0;
    for (unsigned src = 0; src < num_layers_; ++src) {
        for (unsigned dst = src + 1; dst < num_layers_; ++dst) {
            auto& matrix = M_[src * num_layers_ + dst];
            auto& trace = eligibility_[src * num_layers_ + dst];
            std::size_t index = 0;
            for (int k = 0; k < matrix.outerSize(); ++k) {
                for (Eigen::SparseMatrix<float>::InnerIterator it(matrix, k); it; ++it, ++index) {
                    const float weight = it.value();
                    const float hebbian = eta * reward * trace[index];
                    const float decay =
                        std::copysign(static_cast<float>(config_->get_learning_weight_decay()) *
                                          std::max(0.0f, std::abs(weight) - MinDecayingSynapseMagnitude),
                                      weight);
                    const float updated = std::clamp(weight + hebbian - decay, -4.0f, 4.0f);
                    changes += std::abs(updated - weight);
                    plasticity += std::abs(hebbian);
                    decay_sum += std::abs(decay);
                    traces += std::abs(trace[index]);
                    weights += std::abs(updated);
                    zeros += updated == 0;
                    ++count;
                    it.valueRef() = updated;
                    trace[index] *= trace_decay;
                }
            }
        }
    }
    ++learning_stats_.updates;
    learning_stats_.abs_weight_change_sum += changes;
    learning_stats_.abs_plasticity_sum += plasticity;
    learning_stats_.mean_abs_delta = count ? changes / count : 0;
    learning_stats_.mean_abs_plasticity = count ? plasticity / count : 0;
    learning_stats_.mean_abs_decay = count ? decay_sum / count : 0;
    learning_stats_.mean_abs_trace = count ? traces / count : 0;
    learning_stats_.mean_abs_weight = count ? weights / count : 0;
    learning_stats_.zero_fraction = count ? double(zeros) / count : 0;
    if (config_->get_hebbian_inheritance())
        learned_genome_dirty_ = true;
}

std::shared_ptr<const Genome> Brain::get_learned_genome() const {
    if (config_->get_hebbian_inheritance() && learned_genome_dirty_) {
        const_cast<Brain*>(this)->rebuild_genome_from_weights();
        const_cast<Brain*>(this)->learned_genome_dirty_ = false;
    }
    return learned_genome_;
}

void Brain::rebuild_genome_from_weights() {
    const unsigned nl = num_layers_;
    std::vector<Gene> genes;
    genes.reserve(get_total_connections() + silent_genes_.size() + discarded_genes_.size());

    // For every active synapse, create one gene encoding the current learned weight.
    for (unsigned src = 0; src < nl; ++src) {
        for (unsigned dst = src + 1; dst < nl; ++dst) {
            const auto& W = M_[src * nl + dst];
            if (W.nonZeros() == 0)
                continue;

            // Gene sink_type encoding: hidden layers use their layer index, output layer always encodes as 7.
            const uint16_t sink_type = (dst == nl - 1) ? 7u : static_cast<uint16_t>(dst);

            for (int k = 0; k < W.outerSize(); ++k) {
                for (Eigen::SparseMatrix<float>::InnerIterator it(W, k); it; ++it) {
                    // Round to the nearest genetic quantum without erasing a nonzero learned sign.
                    // Exact zeros remain zero; silent/discarded genes are copied unchanged below.
                    const float clamped = std::clamp(it.value() * 8192.0f, -32768.0f, 32767.0f);
                    auto w_int = static_cast<int16_t>(std::round(clamped));
                    if (w_int == 0 && it.value() != 0.0f)
                        w_int = it.value() > 0 ? 1 : -1;

                    genes.emplace_back(static_cast<uint16_t>(src), static_cast<uint32_t>(it.col()), sink_type,
                                       static_cast<uint32_t>(it.row()), w_int);
                }
            }
        }
    }

    // Append silent genes (index-overflow rejections from wire()).
    // These are carried forward unchanged so mutations can reactivate them if layer sizes grow.
    for (const Gene& g : silent_genes_)
        genes.push_back(g);

    // Append genes discarded for type/sink/order constraints.
    // Keeping them avoids irreversible genetic collapse when hebbian inheritance is enabled.
    for (const Gene& g : discarded_genes_)
        genes.push_back(g);

    learned_genome_ = std::make_shared<Genome>(std::move(genes), Genome::encoding_from_config(*config_));
    learned_genome_dirty_ = false;
}
