#include "brain.h"

#include <Eigen/Dense>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
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
        if (!src_ok)
            continue;

        const unsigned src_layer = src_type;

        // Validate sink: active hidden layer(1..H) or output(7)
        unsigned dst_layer;
        if (snk_type == 7) {
            dst_layer = num_hidden_ + 1;
        } else if (snk_type >= 1 && snk_type <= num_hidden_) {
            dst_layer = snk_type;
        } else {
            continue; // sensor(0) or unused hidden layer as sink
        }

        // Enforce feedforward: sink must be strictly deeper than source
        if (dst_layer <= src_layer)
            continue;

        // Reject genes whose num field exceeds the actual layer size.
        // Using modulo would create a coverage bias:  e.g. with sizeY=19 and
        // a 5-bit num field (0-31), indices 0-12 would be selected twice as
        // often as 13-18.  Rejection keeps the distribution uniform.
        const unsigned raw_src = g.get_source_num();
        if (raw_src >= layer_size(src_layer))
            continue;

        const unsigned raw_snk = g.get_sink_num();
        if (raw_snk >= layer_size(dst_layer))
            continue;

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
        }
    }
}

int Brain::feed_forward(const std::vector<float>& sensor_values) {
    const unsigned nl = num_layers_;
    std::vector<Eigen::VectorXf> h(nl);

    // Layer 0: load sensor inputs
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

    // argmax of the output layer
    const auto& out = h[nl - 1];
    int max_index = -1;
    float max_value = 0.0f;
    for (int i = 0; i < out.size(); ++i) {
        if (out(i) > max_value) {
            max_value = out(i);
            max_index = i;
        }
    }
    return max_index;
}

void Brain::resize(unsigned size_s, unsigned size_n, unsigned size_y, unsigned num_hidden) {
    size_s_ = size_s;
    size_n_ = size_n;
    size_y_ = size_y;
    num_hidden_ = num_hidden;
    num_layers_ = num_hidden_ + 2;
    allocate();
}

void Brain::serialize(const std::string& pop_id, unsigned generation) const {
    using json = nlohmann::json;

    const auto timestamp_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();

    json doc;
    doc["pop_id"] = pop_id;
    doc["generation"] = generation;
    doc["timestamp_ms"] = timestamp_ms;
    doc["sizeS"] = size_s_;
    doc["sizeN"] = size_n_;
    doc["sizeY"] = size_y_;
    doc["num_hidden_layers"] = num_hidden_;

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

    const std::string dir = config_->get_nnets_dir();
    std::filesystem::create_directories(dir);

    const std::string filename = dir + pop_id + "_g" + std::to_string(generation) + ".json";
    std::ofstream file(filename);
    file << doc.dump(2);
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


//definendo std::vector<Eigen::VectorXf> activations_; in bran.h
/*Alla fine del feed-forward:
activations_ = h;*/

void Brain::hebbian_update(float reward)
{
    // ---------------------------------------------------------------------
    // Learning rate globale.
    //
    // Determina quanto rapidamente i pesi possono cambiare.
    // Valori troppo grandi rendono il comportamento instabile.
    // In genere conviene partire da numeri molto piccoli.
    // ---------------------------------------------------------------------
    constexpr float eta = 0.001f;

    // ---------------------------------------------------------------------
    // Termine di decadimento.
    //
    // Simula il fatto che una sinapsi inutilizzata o poco utile
    // tende gradualmente a perdere efficacia.
    //
    // Inoltre impedisce che tutti i pesi crescano indefinitamente.
    // ---------------------------------------------------------------------
    constexpr float decay = 0.0001f;

    // ---------------------------------------------------------------------
    // Limiti assoluti dei pesi.
    //
    // In una rete biologica la forza di una sinapsi non è infinita.
    // Questo bounding evita divergenze numeriche e saturazioni estreme.
    // ---------------------------------------------------------------------
    constexpr float max_weight = 5.0f;
    constexpr float min_weight = -5.0f;

    const unsigned nl = num_layers_;

    // ---------------------------------------------------------------------
    // Scorriamo tutte le possibili coppie di layer.
    //
    // La rete è feed-forward quindi esistono connessioni soltanto
    // dai layer più superficiali verso quelli più profondi.
    // ---------------------------------------------------------------------
    for (unsigned src = 0; src < nl; ++src)
    {
        for (unsigned dst = src + 1; dst < nl; ++dst)
        {
            // Recuperiamo la matrice sparsa che contiene tutte
            // le connessioni dal layer sorgente al layer destinazione.
            auto& W = M_[src * nl + dst];

            // Nessuna connessione presente.
            if (W.nonZeros() == 0)
                continue;

            // -----------------------------------------------------------------
            // Eigen memorizza le sparse matrix per colonne.
            //
            // outerSize() permette di attraversare soltanto gli elementi
            // realmente esistenti senza visitare celle vuote.
            // -----------------------------------------------------------------
            for (int k = 0; k < W.outerSize(); ++k)
            {
                for (Eigen::SparseMatrix<float>::InnerIterator it(W, k); it; ++it)
                {
                    // ---------------------------------------------------------
                    // Attività del neurone presinaptico.
                    //
                    // Quanto era attivo il neurone che invia il segnale.
                    // ---------------------------------------------------------
                    const float pre =
                        activations_[src](it.col());

                    // ---------------------------------------------------------
                    // Attività del neurone postsinaptico.
                    //
                    // Quanto era attivo il neurone che riceve il segnale.
                    // ---------------------------------------------------------
                    const float post =
                        activations_[dst](it.row());

                    // Peso attuale della sinapsi.
                    const float w =
                        it.value();

                    // ---------------------------------------------------------
                    // Termine Hebbiano.
                    //
                    // Se reward > 0:
                    //   connessioni fra neuroni co-attivi vengono rinforzate.
                    //
                    // Se reward < 0:
                    //   connessioni fra neuroni co-attivi vengono indebolite.
                    //
                    // Questo permette di associare gli stati interni
                    // dell'agente ai risultati ottenuti nell'ambiente.
                    // ---------------------------------------------------------
                    const float hebbian_term =
                        eta * reward * pre * post;

                    // ---------------------------------------------------------
                    // Decadimento biologico.
                    //
                    // Più il peso è grande, maggiore è la forza che lo
                    // spinge verso zero.
                    //
                    // Agisce come una sorta di omeostasi.
                    // ---------------------------------------------------------
                    const float decay_term =
                        decay * w;

                    // ---------------------------------------------------------
                    // Variazione totale della sinapsi.
                    //
                    // Il decadimento viene sottratto perché tende a
                    // riportare lentamente il peso verso zero.
                    // ---------------------------------------------------------
                    const float dw =
                        hebbian_term - decay_term;

                    // Nuovo peso.
                    float new_weight =
                        w + dw;

                    // ---------------------------------------------------------
                    // Bounding.
                    //
                    // Impedisce valori patologici.
                    // Una sinapsi non può diventare arbitrariamente forte.
                    // ---------------------------------------------------------
                    if (new_weight > max_weight)
                        new_weight = max_weight;

                    if (new_weight < min_weight)
                        new_weight = min_weight;

                    // Scrittura del nuovo peso.
                    it.valueRef() = new_weight;
                }
            }
        }
    }
}