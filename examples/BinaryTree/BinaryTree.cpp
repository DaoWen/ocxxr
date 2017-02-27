/*
 * BinaryTree.cpp
 *
 * A simple tree-based mapping data structure.
 */

#include <ocxxr-main.hpp>

#include <cstdlib>

#ifdef MEASURE_TIME
#include <ctime>
#include <ratio>
#include <chrono>
using namespace std::chrono;

high_resolution_clock::time_point start;
#endif

constexpr bool kVerboseMessages = false;

template <typename T>
using Ptr = ocxxr::RelPtr<T>;

template <typename K, typename V>
class BinaryTree {
 public:
    static constexpr size_t kTreeSize =
            (1 << 20);  // Allocate enough space for 1M entries

    static ocxxr::Arena<BinaryTree> Create() {
        constexpr size_t bytes = sizeof(BinaryTree) + sizeof(Node) * kTreeSize;
        auto arena = ocxxr::Arena<BinaryTree>::Create(bytes);
        CreateIn(arena.Untyped());
        return arena;
    }

    static BinaryTree *CreateIn(ocxxr::Arena<void> arena) {
        return arena.New<BinaryTree>(arena);
    }

    BinaryTree(ocxxr::Arena<void> arena) : arena_(arena), root_(nullptr) {}

    bool Put(const K &key, const V &value) {
        if (kVerboseMessages) {
            PRINTF("Starting put op...\n");
        }
        return Update(key, value, root_);
    }

    bool Get(const K &key, V &result) {
        if (kVerboseMessages) {
            PRINTF("Starting get op...\n");
        }
        return Find(key, root_, result);
    }

    // TODO - add remove op, and use a freelist internally for nodes

 private:
    struct Node {
        const K key;
        V value;
        Ptr<Node> left, right;
        Node(const K &k, const V &v)
                : key(k), value(v), left(nullptr), right(nullptr) {}
    };

    ocxxr::Arena<void> arena_;
    Ptr<Node> root_;

    bool Update(const K &key, const V &value, Ptr<Node> &node) {
        if (!node) {
            // Base case 1: not found
            node = arena_.New<Node>(key, value);
            return false;
        } else if (node->key == key) {
            // Base case 2: found
            node->value = value;
            return true;
        } else {
            // Recursive case
            Ptr<Node> &next = (node->key < key) ? node->left : node->right;
            return Update(key, value, next);
        }
    };

    bool Find(const K &key, Ptr<Node> node, V &output) {
        if (!node) {
            // Base case 1: not found
            return false;
        } else if (node->key == key) {
            // Base case 2: found
            output = node->value;
            return true;
        } else {
            // Recursive case
            Ptr<Node> &next = (node->key < key) ? node->left : node->right;
            return Find(key, next, output);
        }
    };
};

u64 myhash(u64 x) {
    return x * 11400714819323198549UL;
}

void ocxxr::Main(ocxxr::Datablock<ocxxr::MainTaskArgs> args) {
#ifdef MEASURE_TIME
    start = high_resolution_clock::now();
#endif
    
	u32 n;
    if (args->argc() != 2) {
        n = 100000;
        PRINTF("Usage: BinaryTree <num>, defaulting to %" PRIu32 "\n", n);
    } else {
        n = atoi(args->argv(1));
    }

    auto tree = BinaryTree<u64, char>::Create();

    //
    // Puts
    //

    constexpr u64 kPutCount = 100000;

    if (kVerboseMessages) {
        PRINTF("Starting puts\n");
    }

    for (u64 i = 0; i < kPutCount; i++) {
        tree->Put(myhash(i), static_cast<char>('a' + i));
    }

    if (kVerboseMessages) {
        PRINTF("Done with puts\n");
    }

    //
    // Gets
    //

    if (kVerboseMessages) {
        PRINTF("Starting gets\n");
    }

    char result[] = {'X', '\0'};
    tree->Get(myhash(6), result[0]);

    if (kVerboseMessages) {
        PRINTF("Done with gets\n");
    }

    //
    // Results
    //

    PRINTF("Result = %s\n", result);
#ifdef MEASURE_TIME
	high_resolution_clock::time_point end = high_resolution_clock::now();
	duration<double> time_span = duration_cast<duration<double>>(end - start);
	PRINTF("elapsed time: %f second\n", time_span.count());
#endif
    ocxxr::Shutdown();
}
