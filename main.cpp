// rb_benchmark.cpp
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <chrono>
#include <random>
#include <iomanip>
#include <map>
#include <set>
#include <unordered_map>
#include <algorithm>
#include <cstdlib>

// ============================================================
// Timer
// ============================================================
class Timer {
public:
    using Clock = std::chrono::high_resolution_clock;
    void start() { m_start = Clock::now(); }
    void stop()  { m_end   = Clock::now(); }
    double elapsedNanoseconds() const {
        return std::chrono::duration<double, std::nano>(m_end - m_start).count();
    }
    static std::string format(double ns) {
        std::ostringstream oss; oss << std::fixed << std::setprecision(3);
        if      (ns < 1'000.0)          oss << ns                    << " ns";
        else if (ns < 1'000'000.0)      oss << ns / 1'000.0          << " us";
        else if (ns < 1'000'000'000.0)  oss << ns / 1'000'000.0      << " ms";
        else if (ns < 60'000'000'000.0) oss << ns / 1'000'000'000.0  << " s";
        else                            oss << ns / 60'000'000'000.0 << " min";
        return oss.str();
    }
private:
    Clock::time_point m_start{}, m_end{};
};

// ============================================================
// Arbre Rouge-Noir (from scratch)
// ============================================================
class RedBlackTree {
private:
    struct Node {
        int   key;
        bool  red;          // true = ROUGE, false = NOIR
        Node *left, *right, *parent;
        explicit Node(int k) : key(k), red(true),
            left(nullptr), right(nullptr), parent(nullptr) {}
    };

    Node*  root_  = nullptr;
    size_t count_ = 0;

    static bool isRed  (const Node* n) { return n &&  n->red; }
    static bool isBlack(const Node* n) { return !n || !n->red; }

    void rotateLeft(Node* x) {
        Node* y = x->right;
        x->right = y->left;
        if (y->left) y->left->parent = x;
        y->parent = x->parent;
        if (!x->parent)               root_ = y;
        else if (x == x->parent->left) x->parent->left  = y;
        else                          x->parent->right = y;
        y->left  = x;
        x->parent = y;
    }

    void rotateRight(Node* x) {
        Node* y = x->left;
        x->left = y->right;
        if (y->right) y->right->parent = x;
        y->parent = x->parent;
        if (!x->parent)                root_ = y;
        else if (x == x->parent->right) x->parent->right = y;
        else                           x->parent->left  = y;
        y->right = x;
        x->parent = y;
    }

    void insertFixup(Node* z) {
        while (z->parent && z->parent->red) {
            Node* parent = z->parent;
            Node* gp     = parent->parent;
            if (parent == gp->left) {
                Node* uncle = gp->right;
                if (isRed(uncle)) {
                    parent->red = false; uncle->red = false; gp->red = true;
                    z = gp;
                } else {
                    if (z == parent->right) { z = parent; rotateLeft(z); }
                    z->parent->red = false;
                    z->parent->parent->red = true;
                    rotateRight(z->parent->parent);
                }
            } else {
                Node* uncle = gp->left;
                if (isRed(uncle)) {
                    parent->red = false; uncle->red = false; gp->red = true;
                    z = gp;
                } else {
                    if (z == parent->left) { z = parent; rotateRight(z); }
                    z->parent->red = false;
                    z->parent->parent->red = true;
                    rotateLeft(z->parent->parent);
                }
            }
        }
        root_->red = false;
    }

    void transplant(Node* u, Node* v) {
        if (!u->parent)              root_ = v;
        else if (u == u->parent->left) u->parent->left  = v;
        else                         u->parent->right = v;
        if (v) v->parent = u->parent;
    }

    Node* minimum(Node* x) {
        while (x->left) x = x->left;
        return x;
    }

    // x peut etre nullptr : on doit fournir explicitement son parent
    void deleteFixup(Node* x, Node* parent) {
        while (x != root_ && isBlack(x)) {
            if (x == parent->left) {
                Node* w = parent->right;
                if (isRed(w)) {
                    w->red = false; parent->red = true;
                    rotateLeft(parent);
                    w = parent->right;
                }
                if (!w) { x = parent; parent = x->parent; continue; }
                if (isBlack(w->left) && isBlack(w->right)) {
                    w->red = true;
                    x = parent; parent = x->parent;
                } else {
                    if (isBlack(w->right)) {
                        if (w->left) w->left->red = false;
                        w->red = true;
                        rotateRight(w);
                        w = parent->right;
                    }
                    w->red = parent->red;
                    parent->red = false;
                    if (w->right) w->right->red = false;
                    rotateLeft(parent);
                    x = root_; parent = nullptr;
                }
            } else {
                Node* w = parent->left;
                if (isRed(w)) {
                    w->red = false; parent->red = true;
                    rotateRight(parent);
                    w = parent->left;
                }
                if (!w) { x = parent; parent = x->parent; continue; }
                if (isBlack(w->left) && isBlack(w->right)) {
                    w->red = true;
                    x = parent; parent = x->parent;
                } else {
                    if (isBlack(w->left)) {
                        if (w->right) w->right->red = false;
                        w->red = true;
                        rotateLeft(w);
                        w = parent->left;
                    }
                    w->red = parent->red;
                    parent->red = false;
                    if (w->left) w->left->red = false;
                    rotateRight(parent);
                    x = root_; parent = nullptr;
                }
            }
        }
        if (x) x->red = false;
    }

    void destroy(Node* x) {
        if (!x) return;
        destroy(x->left); destroy(x->right); delete x;
    }

public:
    RedBlackTree() = default;
    ~RedBlackTree() { destroy(root_); }
    RedBlackTree(const RedBlackTree&) = delete;
    RedBlackTree& operator=(const RedBlackTree&) = delete;

    void insert(int key) {
        // doublons ignores (comme map / set / unordered_map)
        for (Node* c = root_; c; ) {
            if (key == c->key) return;
            c = (key < c->key) ? c->left : c->right;
        }
        Node* z = new Node(key);
        Node* y = nullptr;
        Node* x = root_;
        while (x) {
            y = x;
            if (key < x->key) x = x->left; else x = x->right;
        }
        z->parent = y;
        if      (!y)              root_ = z;
        else if (key < y->key)    y->left  = z;
        else                      y->right = z;
        insertFixup(z);
        ++count_;
    }

    bool search(int key) const {
        for (Node* c = root_; c; ) {
            if (key == c->key) return true;
            c = (key < c->key) ? c->left : c->right;
        }
        return false;
    }

    bool remove(int key) {
        Node* z = root_;
        while (z && z->key != key) z = (key < z->key) ? z->left : z->right;
        if (!z) return false;

        Node* y = z;
        bool  yOrigRed = y->red;
        Node *x, *xParent;

        if (!z->left) {
            x = z->right; xParent = z->parent;
            transplant(z, z->right);
        } else if (!z->right) {
            x = z->left;  xParent = z->parent;
            transplant(z, z->left);
        } else {
            y = minimum(z->right);
            yOrigRed = y->red;
            x = y->right;
            if (y->parent == z) {
                xParent = y;
            } else {
                xParent = y->parent;
                transplant(y, y->right);
                y->right = z->right;
                y->right->parent = y;
            }
            transplant(z, y);
            y->left = z->left;
            y->left->parent = y;
            y->red = z->red;
        }
        delete z; --count_;
        if (!yOrigRed) deleteFixup(x, xParent);
        return true;
    }

    size_t size() const noexcept { return count_; }
};

// ============================================================
// Benchmark
// ============================================================
enum class InsertionOrder { AS_IS, SORTED, ALMOST_SORTED };

class Benchmark {
public:
    Benchmark(std::string filename, InsertionOrder order)
        : m_filename(std::move(filename)), m_order(order), m_rng(std::random_device{}()) {}

    void run() {
        readFile();
        prepareData();
        insertAll();
        searchRandom(3);
        removeRandom(3);
    }

private:
    // ---------- Lecture fichier ----------
    void readFile() {
        std::cout << "=== ETAPE 1 : Lecture du fichier ===\n";
        std::ifstream file(m_filename);
        if (!file.is_open()) {
            std::cerr << "Impossible d'ouvrir: " << m_filename << "\n";
            std::exit(EXIT_FAILURE);
        }
        m_timer.start();
        int v; while (file >> v) m_data.push_back(v);
        m_timer.stop();
        std::cout << "  Nombre d'entiers lus : " << m_data.size() << "\n";
        std::cout << "  Temps de lecture     : "
                  << Timer::format(m_timer.elapsedNanoseconds()) << "\n\n";
    }

    // ---------- Preparation selon le mode choisi ----------
    void prepareData() {
        if (m_order == InsertionOrder::AS_IS) {
            std::cout << "=== Mode : donnees ALEATOIRES (telles quelles) ===\n\n";
            return;
        }
        if (m_order == InsertionOrder::SORTED) {
            std::cout << "=== Mode : donnees TRIEES ===\n";
            m_timer.start();
            std::sort(m_data.begin(), m_data.end());
            m_timer.stop();
            std::cout << "  Temps de tri : "
                      << Timer::format(m_timer.elapsedNanoseconds()) << "\n\n";
            return;
        }
        // ALMOST_SORTED : tri puis ~5% de swaps adjacents
        std::cout << "=== Mode : donnees PRESQUE TRIEES (~5% de perturbations) ===\n";
        m_timer.start();
        std::sort(m_data.begin(), m_data.end());
        std::uniform_real_distribution<double> p(0.0, 1.0);
        for (size_t i = 0; i + 1 < m_data.size(); ++i) {
            if (p(m_rng) < 0.05) { std::swap(m_data[i], m_data[i + 1]); ++i; }
        }
        m_timer.stop();
        std::cout << "  Temps de tri + perturbation : "
                  << Timer::format(m_timer.elapsedNanoseconds()) << "\n\n";
    }

    // ---------- Insertion ----------
    void insertAll() {
        std::cout << "=== ETAPE 2 : Insertion dans les 4 structures ===\n";

        m_timer.start();
        for (int v : m_data) m_rb.insert(v);
        m_timer.stop();
        std::cout << "  Red-Black tree  : " << Timer::format(m_timer.elapsedNanoseconds())
                  << "   (taille = " << m_rb.size() << ")\n";

        m_timer.start();
        for (int v : m_data) m_map.emplace(v, true);
        m_timer.stop();
        std::cout << "  std::map        : " << Timer::format(m_timer.elapsedNanoseconds())
                  << "   (taille = " << m_map.size() << ")\n";

        m_timer.start();
        for (int v : m_data) m_set.insert(v);
        m_timer.stop();
        std::cout << "  std::set        : " << Timer::format(m_timer.elapsedNanoseconds())
                  << "   (taille = " << m_set.size() << ")\n";

        m_timer.start();
        for (int v : m_data) m_umap.emplace(v, true);
        m_timer.stop();
        std::cout << "  unordered_map   : " << Timer::format(m_timer.elapsedNanoseconds())
                  << "   (taille = " << m_umap.size() << ")\n\n";
    }

    // ---------- Recherche ----------
    void searchRandom(int count) {
        std::cout << "=== ETAPE 3 : Recherche de " << count << " valeurs aleatoires ===\n";
        for (int value : pickDistinctValues(count)) {
            std::cout << "  Valeur " << value << " :\n";

            m_timer.start(); bool a = m_rb.search(value);               m_timer.stop();
            std::cout << "    Red-Black tree : " << (a ? "trouve " : "absent ")
                      << " en " << Timer::format(m_timer.elapsedNanoseconds()) << "\n";

            m_timer.start(); bool b = m_map.find(value) != m_map.end(); m_timer.stop();
            std::cout << "    std::map       : " << (b ? "trouve " : "absent ")
                      << " en " << Timer::format(m_timer.elapsedNanoseconds()) << "\n";

            m_timer.start(); bool c = m_set.find(value) != m_set.end(); m_timer.stop();
            std::cout << "    std::set       : " << (c ? "trouve " : "absent ")
                      << " en " << Timer::format(m_timer.elapsedNanoseconds()) << "\n";

            m_timer.start(); bool d = m_umap.find(value) != m_umap.end(); m_timer.stop();
            std::cout << "    unordered_map  : " << (d ? "trouve " : "absent ")
                      << " en " << Timer::format(m_timer.elapsedNanoseconds()) << "\n";
        }
        std::cout << "\n";
    }

    // ---------- Suppression ----------
    void removeRandom(int count) {
        std::cout << "=== ETAPE 4 : Suppression de " << count << " valeurs aleatoires ===\n";
        for (int value : pickDistinctValues(count)) {
            std::cout << "  Valeur " << value << " :\n";

            m_timer.start(); bool a = m_rb.remove(value);                m_timer.stop();
            std::cout << "    Red-Black tree : " << (a ? "supprime" : "absent  ")
                      << " en " << Timer::format(m_timer.elapsedNanoseconds()) << "\n";

            m_timer.start(); size_t b = m_map.erase(value);              m_timer.stop();
            std::cout << "    std::map       : " << (b ? "supprime" : "absent  ")
                      << " en " << Timer::format(m_timer.elapsedNanoseconds()) << "\n";

            m_timer.start(); size_t c = m_set.erase(value);              m_timer.stop();
            std::cout << "    std::set       : " << (c ? "supprime" : "absent  ")
                      << " en " << Timer::format(m_timer.elapsedNanoseconds()) << "\n";

            m_timer.start(); size_t d = m_umap.erase(value);             m_timer.stop();
            std::cout << "    unordered_map  : " << (d ? "supprime" : "absent  ")
                      << " en " << Timer::format(m_timer.elapsedNanoseconds()) << "\n";
        }
        std::cout << "\n=== Tailles finales ===\n"
                  << "  Red-Black tree : " << m_rb.size()   << "\n"
                  << "  std::map       : " << m_map.size()  << "\n"
                  << "  std::set       : " << m_set.size()  << "\n"
                  << "  unordered_map  : " << m_umap.size() << "\n";
    }

    // ---------- Choix de `count` valeurs distinctes aleatoires depuis m_data ----------
    std::vector<int> pickDistinctValues(int count) {
        std::vector<int> picked;
        if (m_data.empty() || count <= 0) return picked;
        count = std::min<int>(count, static_cast<int>(m_data.size()));

        std::set<int> seen;
        std::uniform_int_distribution<size_t> dist(0, m_data.size() - 1);
        int tries = 0;
        while (static_cast<int>(picked.size()) < count
               && tries < static_cast<int>(m_data.size()) * 20) {
            int v = m_data[dist(m_rng)];
            if (seen.insert(v).second) picked.push_back(v);
            ++tries;
        }
        // fallback si pas assez de valeurs distinctes (cas pathologique)
        if (static_cast<int>(picked.size()) < count) {
            for (int v : m_data) {
                if (static_cast<int>(picked.size()) >= count) break;
                if (seen.insert(v).second) picked.push_back(v);
            }
        }
        return picked;
    }

    std::string             m_filename;
    InsertionOrder          m_order;
    std::vector<int>        m_data;

    RedBlackTree            m_rb;
    std::map<int, bool>     m_map;
    std::set<int>           m_set;
    std::unordered_map<int, bool> m_umap;

    Timer                   m_timer;
    std::mt19937            m_rng;
};

// ============================================================
// main
// ============================================================
int main(int argc, char** argv) {
    const std::string file = (argc > 1) ? argv[1] : "../fichierTXT_Nbre/nombres4.txt";

    std::cout << "=== Choix du mode d'insertion ===\n"
              << "  1. Donnees aleatoires (telles quelles)\n"
              << "  2. Donnees triees\n"
              << "  3. Donnees presque triees (~5% de perturbations)\n"
              << "Votre choix : ";
    int choice = 1;
    std::cin >> choice;

    InsertionOrder order = InsertionOrder::AS_IS;
    if      (choice == 2) order = InsertionOrder::SORTED;
    else if (choice == 3) order = InsertionOrder::ALMOST_SORTED;
    else if (choice != 1) {
        std::cout << "Choix invalide, utilisation du mode aleatoire.\n";
    }
    std::cout << "\n";

    Benchmark bench(file, order);
    bench.run();
    return 0;
}
