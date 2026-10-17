// =============================================================================
// CSD202 - Assignment Topic 6: Hotel Room Management using a Binary Search Tree
//
// - The BST is implemented explicitly with pointers (no std::map / std::set).
// - Search key: Room ID (string, stored in UPPER CASE so comparison is consistent).
// - Duplicate IDs are rejected.
// - Deletion is implemented twice: by Copying and by Merging (separate functions).
// - Rotations: left, right, left-right, right-left (BST order is preserved).
// - Bonus: height / balance check, pretty print, save / load, sample data.
//
// Compile:  g++ -std=c++11 -Wall -Wextra -o hotel_bst hotel_bst.cpp
// Run:      ./hotel_bst          (add --echo to echo piped input, used for tests)
// =============================================================================
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <queue>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
using namespace std;

// ----------------------------------------------------------------------------
// Data
// ----------------------------------------------------------------------------
struct Room {
    string id;      // unique BST key (upper case)
    string type;    // e.g. Standard, Deluxe, Suite
    float price;    // price per night
    string status;  // Available / Occupied / Maintenance
    int floor;
    int capacity;
};

struct Node {
    Room room;
    Node* left;
    Node* right;
    explicit Node(const Room& r) : room(r), left(nullptr), right(nullptr) {}
};

typedef function<void(const Room&)> Visitor;

static const int MAX_ID_LEN = 10;
static const int MAX_TYPE_LEN = 16;
static const float MAX_PRICE = 10000000.0f;
static const int MAX_FLOOR = 200;
static const int MAX_CAPACITY = 20;

// ----------------------------------------------------------------------------
// Small string / number helpers
// ----------------------------------------------------------------------------
static string trim(const string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

static string toUpper(string s) {
    for (size_t i = 0; i < s.size(); i++) s[i] = (char)toupper((unsigned char)s[i]);
    return s;
}

static string toLower(string s) {
    for (size_t i = 0; i < s.size(); i++) s[i] = (char)tolower((unsigned char)s[i]);
    return s;
}

static bool hasSpace(const string& s) {
    for (size_t i = 0; i < s.size(); i++)
        if (isspace((unsigned char)s[i])) return true;
    return false;
}

static bool isValidId(const string& s) {
    return !s.empty() && (int)s.size() <= MAX_ID_LEN && !hasSpace(s) && s.find('|') == string::npos;
}

static bool isValidType(const string& s) {
    return !s.empty() && (int)s.size() <= MAX_TYPE_LEN && s.find('|') == string::npos;
}

// Accepts any capitalisation and stores the canonical spelling.
static bool normalizeStatus(const string& in, string& out) {
    string l = toLower(trim(in));
    if (l == "available") { out = "Available"; return true; }
    if (l == "occupied") { out = "Occupied"; return true; }
    if (l == "maintenance") { out = "Maintenance"; return true; }
    return false;
}

static bool parseFloatStrict(const string& s, float& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    double v = strtod(s.c_str(), &end);
    if (end == s.c_str() || *end != '\0') return false;
    if (!std::isfinite(v)) return false;
    out = (float)v;
    return true;
}

static bool parseIntStrict(const string& s, int& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    long v = strtol(s.c_str(), &end, 10);
    if (end == s.c_str() || *end != '\0') return false;
    if (v < -1000000L || v > 1000000L) return false;
    out = (int)v;
    return true;
}

// ----------------------------------------------------------------------------
// Console input (every read goes through promptLine)
// ----------------------------------------------------------------------------
struct InputEnded {};  // thrown on end of input so the tree is still freed
static bool gEcho = false;

static string promptLine(const string& prompt) {
    cout << prompt;
    cout.flush();
    string line;
    if (!getline(cin, line)) throw InputEnded();
    if (gEcho) cout << line << "\n";
    return trim(line);
}

// Each ask* function returns true if a value was entered. With allowBlank=true,
// pressing Enter returns false ("keep the current value").
static bool askId(const string& prompt, string& out, bool allowBlank = false) {
    while (true) {
        string s = promptLine(prompt);
        if (s.empty() && allowBlank) return false;
        if (isValidId(s)) { out = toUpper(s); return true; }
        cout << "  Invalid Room ID: use 1-" << MAX_ID_LEN << " characters, no spaces, no '|'.\n";
    }
}

static bool askType(const string& prompt, string& out, bool allowBlank = false) {
    while (true) {
        string s = promptLine(prompt);
        if (s.empty() && allowBlank) return false;
        if (isValidType(s)) { out = s; return true; }
        cout << "  Invalid Room Type: use 1-" << MAX_TYPE_LEN << " characters, no '|'.\n";
    }
}

static bool askPrice(const string& prompt, float& out, bool allowBlank = false) {
    while (true) {
        string s = promptLine(prompt);
        if (s.empty() && allowBlank) return false;
        float v;
        if (parseFloatStrict(s, v) && v > 0.0f && v <= MAX_PRICE) { out = v; return true; }
        cout << "  Invalid price: enter a number greater than 0 (max " << (long)MAX_PRICE << ").\n";
    }
}

static bool askStatus(const string& prompt, string& out, bool allowBlank = false) {
    while (true) {
        string s = promptLine(prompt);
        if (s.empty() && allowBlank) return false;
        if (normalizeStatus(s, out)) return true;
        cout << "  Invalid status: choose Available, Occupied or Maintenance.\n";
    }
}

static bool askInt(const string& prompt, int lo, int hi, int& out, const string& what,
                   bool allowBlank = false) {
    while (true) {
        string s = promptLine(prompt);
        if (s.empty() && allowBlank) return false;
        int v;
        if (parseIntStrict(s, v) && v >= lo && v <= hi) { out = v; return true; }
        cout << "  Invalid " << what << ": enter an integer from " << lo << " to " << hi << ".\n";
    }
}

// ----------------------------------------------------------------------------
// Output helpers
// ----------------------------------------------------------------------------
static string fmtPrice(float p) {
    ostringstream os;
    os << fixed << setprecision(2) << p;
    return os.str();
}

static void printHeader() {
    cout << left << setw(12) << "Room ID" << setw(18) << "Type" << right << setw(10) << "Price"
         << "  " << left << setw(13) << "Status" << right << setw(6) << "Floor" << setw(10)
         << "Capacity" << "\n";
    cout << string(69, '-') << "\n";
}

static void printRoom(const Room& r) {
    cout << left << setw(12) << r.id << setw(18) << r.type << right << setw(10) << fixed
         << setprecision(2) << r.price << "  " << left << setw(13) << r.status << right << setw(6)
         << r.floor << setw(10) << r.capacity << "\n";
}

// ----------------------------------------------------------------------------
// Binary Search Tree of rooms
// ----------------------------------------------------------------------------
class RoomBST {
private:
    Node* root;

    // ---- recursive helpers ----
    static void destroy(Node* n) {
        if (!n) return;
        destroy(n->left);
        destroy(n->right);
        delete n;
    }
    static void inorderRec(Node* n, const Visitor& v) {
        if (!n) return;
        inorderRec(n->left, v);
        v(n->room);
        inorderRec(n->right, v);
    }
    static void preorderRec(Node* n, const Visitor& v) {
        if (!n) return;
        v(n->room);
        preorderRec(n->left, v);
        preorderRec(n->right, v);
    }
    static void postorderRec(Node* n, const Visitor& v) {
        if (!n) return;
        postorderRec(n->left, v);
        postorderRec(n->right, v);
        v(n->room);
    }
    static int heightRec(Node* n) {  // number of levels; empty tree = 0
        if (!n) return 0;
        int l = heightRec(n->left), r = heightRec(n->right);
        return 1 + (l > r ? l : r);
    }
    // Returns height, or -1 if some node has subtree heights differing by more than 1.
    static int balancedHeight(Node* n) {
        if (!n) return 0;
        int l = balancedHeight(n->left);
        if (l < 0) return -1;
        int r = balancedHeight(n->right);
        if (r < 0) return -1;
        if (l - r > 1 || r - l > 1) return -1;
        return 1 + (l > r ? l : r);
    }
    static bool orderOk(Node* n, const string* lo, const string* hi) {
        if (!n) return true;
        if (lo && !(*lo < n->room.id)) return false;
        if (hi && !(n->room.id < *hi)) return false;
        return orderOk(n->left, lo, &n->room.id) && orderOk(n->right, &n->room.id, hi);
    }
    static void prettyRec(Node* n, int level) {
        if (!n) return;
        prettyRec(n->right, level + 1);
        cout << string((size_t)level * 8, ' ') << n->room.id << " (" << n->room.type << ", "
             << n->room.status << ")\n";
        prettyRec(n->left, level + 1);
    }

    // ---- rotations on a link (reference to the pointer that holds the subtree) ----
    static void rotLeft(Node*& x) {
        Node* y = x->right;
        x->right = y->left;
        y->left = x;
        x = y;
    }
    static void rotRight(Node*& x) {
        Node* y = x->left;
        x->left = y->right;
        y->right = x;
        x = y;
    }

    // Returns the link (pointer-to-pointer) where `id` is, or where it would be inserted.
    Node** findLink(const string& id) {
        Node** link = &root;
        while (*link && (*link)->room.id != id)
            link = (id < (*link)->room.id) ? &(*link)->left : &(*link)->right;
        return link;
    }

public:
    RoomBST() : root(nullptr) {}
    ~RoomBST() { destroy(root); }
    RoomBST(const RoomBST&) = delete;
    RoomBST& operator=(const RoomBST&) = delete;

    bool empty() const { return root == nullptr; }
    void clear() { destroy(root); root = nullptr; }
    void swapWith(RoomBST& other) { std::swap(root, other.root); }

    // ---- core operations ----
    bool insert(const Room& r) {
        Node** link = findLink(r.id);
        if (*link) return false;  // duplicate ID
        *link = new Node(r);
        return true;
    }

    Room* find(const string& id) {
        Node** link = findLink(id);
        return *link ? &(*link)->room : nullptr;
    }

    // Deletion by COPYING: a node with two children takes the data of its in-order
    // predecessor (rightmost node of the left subtree), then that predecessor
    // node (which has no right child) is unlinked.
    bool removeByCopying(const string& id) {
        Node** link = findLink(id);
        Node* node = *link;
        if (!node) return false;
        if (!node->left) {  // leaf or only right child
            *link = node->right;
            delete node;
        } else if (!node->right) {  // only left child
            *link = node->left;
            delete node;
        } else {  // two children
            Node** predLink = &node->left;
            while ((*predLink)->right) predLink = &(*predLink)->right;
            Node* pred = *predLink;
            node->room = pred->room;   // copy the predecessor's record
            *predLink = pred->left;    // pred has no right child
            delete pred;
        }
        return true;
    }

    // Deletion by MERGING: a node with two children is replaced by its left
    // subtree; the right subtree is attached as the right child of the
    // rightmost node of the left subtree (the in-order predecessor).
    bool removeByMerging(const string& id) {
        Node** link = findLink(id);
        Node* node = *link;
        if (!node) return false;
        if (!node->left) {
            *link = node->right;
        } else if (!node->right) {
            *link = node->left;
        } else {
            Node* rightmost = node->left;
            while (rightmost->right) rightmost = rightmost->right;
            rightmost->right = node->right;
            *link = node->left;
        }
        delete node;
        return true;
    }

    // ---- traversals ----
    void inorder(const Visitor& v) const { inorderRec(root, v); }
    void preorder(const Visitor& v) const { preorderRec(root, v); }
    void postorder(const Visitor& v) const { postorderRec(root, v); }
    void levelOrder(const Visitor& v) const {  // breadth-first
        if (!root) return;
        queue<Node*> q;
        q.push(root);
        while (!q.empty()) {
            Node* n = q.front();
            q.pop();
            v(n->room);
            if (n->left) q.push(n->left);
            if (n->right) q.push(n->right);
        }
    }

    // ---- rotations at the node with the given ID ----
    bool rotateLeftAt(const string& id, string& why) {
        Node** link = findLink(id);
        if (!*link) { why = "Room " + id + " was not found."; return false; }
        if (!(*link)->right) {
            why = "Cannot rotate left at " + id + ": it has no right child.";
            return false;
        }
        rotLeft(*link);
        return true;
    }
    bool rotateRightAt(const string& id, string& why) {
        Node** link = findLink(id);
        if (!*link) { why = "Room " + id + " was not found."; return false; }
        if (!(*link)->left) {
            why = "Cannot rotate right at " + id + ": it has no left child.";
            return false;
        }
        rotRight(*link);
        return true;
    }
    // Left-Right: rotate left at the left child, then rotate right at the node.
    bool rotateLeftRightAt(const string& id, string& why) {
        Node** link = findLink(id);
        if (!*link) { why = "Room " + id + " was not found."; return false; }
        Node* x = *link;
        if (!x->left) {
            why = "Cannot do left-right rotation at " + id + ": it has no left child.";
            return false;
        }
        if (!x->left->right) {
            why = "Cannot do left-right rotation at " + id + ": its left child has no right child.";
            return false;
        }
        rotLeft(x->left);
        rotRight(*link);
        return true;
    }
    // Right-Left: rotate right at the right child, then rotate left at the node.
    bool rotateRightLeftAt(const string& id, string& why) {
        Node** link = findLink(id);
        if (!*link) { why = "Room " + id + " was not found."; return false; }
        Node* x = *link;
        if (!x->right) {
            why = "Cannot do right-left rotation at " + id + ": it has no right child.";
            return false;
        }
        if (!x->right->left) {
            why = "Cannot do right-left rotation at " + id + ": its right child has no left child.";
            return false;
        }
        rotRight(x->right);
        rotLeft(*link);
        return true;
    }

    // ---- bonus: shape information ----
    int height() const { return heightRec(root); }
    bool isBalanced() const { return balancedHeight(root) >= 0; }
    bool orderIsValid() const { return orderOk(root, nullptr, nullptr); }
    void prettyPrint() const { prettyRec(root, 0); }

    // ---- bonus: file I/O (pre-order, so reloading rebuilds the same shape) ----
    bool saveToFile(const string& filename, int& count) const {
        ofstream out(filename.c_str());
        if (!out) return false;
        count = 0;
        int* c = &count;
        ofstream* o = &out;
        preorder([o, c](const Room& r) {
            *o << r.id << '|' << r.type << '|' << fixed << setprecision(2) << r.price << '|'
               << r.status << '|' << r.floor << '|' << r.capacity << '\n';
            ++(*c);
        });
        return (bool)out;
    }

    // Loads into a temporary tree and replaces the current one only if the file opens.
    bool loadFromFile(const string& filename, int& loaded, int& skipped) {
        ifstream in(filename.c_str());
        if (!in) return false;
        RoomBST fresh;
        loaded = skipped = 0;
        string line;
        while (getline(in, line)) {
            line = trim(line);
            if (line.empty()) continue;
            vector<string> f;
            size_t start = 0;
            while (true) {
                size_t p = line.find('|', start);
                if (p == string::npos) { f.push_back(trim(line.substr(start))); break; }
                f.push_back(trim(line.substr(start, p - start)));
                start = p + 1;
            }
            Room r;
            float price;
            int floorNo, cap;
            string status;
            if (f.size() != 6 || !isValidId(f[0]) || !isValidType(f[1]) ||
                !parseFloatStrict(f[2], price) || price <= 0.0f || price > MAX_PRICE ||
                !normalizeStatus(f[3], status) || !parseIntStrict(f[4], floorNo) || floorNo < 0 ||
                floorNo > MAX_FLOOR || !parseIntStrict(f[5], cap) || cap < 1 || cap > MAX_CAPACITY) {
                skipped++;
                continue;
            }
            r.id = toUpper(f[0]);
            r.type = f[1];
            r.price = price;
            r.status = status;
            r.floor = floorNo;
            r.capacity = cap;
            if (fresh.insert(r)) loaded++; else skipped++;  // duplicate IDs are skipped
        }
        swapWith(fresh);  // `fresh` now holds the old tree and frees it
        return true;
    }
};

// ----------------------------------------------------------------------------
// Statistics (plain vectors, no std::map)
// ----------------------------------------------------------------------------
struct TypeStat {
    string type;  // first spelling seen
    int available, occupied, maintenance;
};

static void showStatistics(const RoomBST& tree) {
    if (tree.empty()) { cout << "The tree is empty, nothing to report.\n"; return; }
    vector<TypeStat> stats;
    int total = 0, av = 0, oc = 0, ma = 0;
    tree.inorder([&](const Room& r) {
        total++;
        size_t i = 0;
        while (i < stats.size() && toLower(stats[i].type) != toLower(r.type)) i++;
        if (i == stats.size()) {
            TypeStat s = {r.type, 0, 0, 0};
            stats.push_back(s);
        }
        if (r.status == "Available") { stats[i].available++; av++; }
        else if (r.status == "Occupied") { stats[i].occupied++; oc++; }
        else { stats[i].maintenance++; ma++; }
    });
    cout << "Total number of rooms: " << total << "\n\n";
    cout << left << setw(18) << "Room Type" << right << setw(8) << "Total" << setw(12)
         << "Available" << setw(10) << "Occupied" << setw(13) << "Maintenance" << "\n";
    cout << string(61, '-') << "\n";
    for (size_t i = 0; i < stats.size(); i++) {
        const TypeStat& s = stats[i];
        cout << left << setw(18) << s.type << right << setw(8)
             << (s.available + s.occupied + s.maintenance) << setw(12) << s.available << setw(10)
             << s.occupied << setw(13) << s.maintenance << "\n";
    }
    cout << string(61, '-') << "\n";
    cout << left << setw(18) << "ALL TYPES" << right << setw(8) << total << setw(12) << av
         << setw(10) << oc << setw(13) << ma << "\n";
}

static double potentialRevenue(const RoomBST& tree) {
    double sum = 0.0;
    tree.inorder([&](const Room& r) {
        if (r.status != "Maintenance") sum += r.price;
    });
    return sum;
}

// ----------------------------------------------------------------------------
// Menu actions
// ----------------------------------------------------------------------------
static void showMenu() {
    cout << "\n============ HOTEL ROOM MANAGEMENT ============\n"
            "1. Add new room\n"
            "2. Delete room (copying)\n"
            "3. Delete room (merging)\n"
            "4. Search room by ID\n"
            "5. Search room by Room Type\n"
            "6. Update room information\n"
            "7. Display (in-order)\n"
            "8. Display (pre-order)\n"
            "9. Display (post-order)\n"
            "10. Display (level-order)\n"
            "11. Rotate left at a node\n"
            "12. Rotate right at a node\n"
            "13. Demonstrate left-right rotation\n"
            "14. Demonstrate right-left rotation\n"
            "15. Display available rooms by price range\n"
            "16. Show potential one-night revenue\n"
            "17. Show statistics\n"
            "--- bonus ---\n"
            "18. Tree height and balance check\n"
            "19. Pretty-print the tree\n"
            "20. Save tree to file\n"
            "21. Load tree from file\n"
            "22. Insert sample data (duplicates skipped)\n"
            "23. Clear the tree\n"
            "0. Exit\n";
}

static void addRoom(RoomBST& tree) {
    Room r;
    askId("Room ID: ", r.id);
    if (tree.find(r.id)) {  // reject early, before asking for the other fields
        cout << "Rejected: Room ID " << r.id << " already exists.\n";
        return;
    }
    askType("Room Type: ", r.type);
    askPrice("Price per night: ", r.price);
    askStatus("Status (Available/Occupied/Maintenance): ", r.status);
    askInt("Floor: ", 0, MAX_FLOOR, r.floor, "floor");
    askInt("Capacity: ", 1, MAX_CAPACITY, r.capacity, "capacity");
    if (tree.insert(r)) cout << "Room " << r.id << " added.\n";
    else cout << "Rejected: Room ID " << r.id << " already exists.\n";
}

static void deleteRoom(RoomBST& tree, bool byCopying) {
    if (tree.empty()) { cout << "The tree is empty, nothing to delete.\n"; return; }
    string id;
    askId("Room ID to delete: ", id);
    bool ok = byCopying ? tree.removeByCopying(id) : tree.removeByMerging(id);
    if (ok) cout << "Room " << id << " deleted by " << (byCopying ? "copying" : "merging") << ".\n";
    else cout << "Room " << id << " was not found.\n";
}

static void searchById(RoomBST& tree) {
    if (tree.empty()) { cout << "The tree is empty.\n"; return; }
    string id;
    askId("Room ID to search: ", id);
    Room* r = tree.find(id);
    if (!r) { cout << "Room " << id << " was not found.\n"; return; }
    cout << "Found:\n";
    printHeader();
    printRoom(*r);
}

static void searchByType(const RoomBST& tree) {
    if (tree.empty()) { cout << "The tree is empty.\n"; return; }
    string type;
    askType("Room Type to search: ", type);
    string key = toLower(type);
    int n = 0;
    tree.inorder([&](const Room& r) {  // whole-tree traversal
        if (toLower(r.type) != key) return;
        if (n == 0) { cout << "Rooms of type \"" << type << "\":\n"; printHeader(); }
        printRoom(r);
        n++;
    });
    if (n == 0) cout << "No room of type \"" << type << "\" was found.\n";
    else cout << n << " room(s) found.\n";
}

static void updateRoom(RoomBST& tree) {
    if (tree.empty()) { cout << "The tree is empty.\n"; return; }
    string id;
    askId("Room ID to update: ", id);
    Room* cur = tree.find(id);
    if (!cur) { cout << "Room " << id << " was not found.\n"; return; }
    cout << "Current record:\n";
    printHeader();
    printRoom(*cur);
    cout << "Press Enter to keep the current value of a field.\n";

    Room nr = *cur;  // work on a copy; nothing changes until every field is valid
    const string oldId = cur->id;
    while (true) {
        string tmp;
        if (!askId("New Room ID [" + oldId + "]: ", tmp, true) || tmp == oldId) break;
        if (tree.find(tmp)) {
            cout << "  Room ID " << tmp << " already exists, choose another one.\n";
            continue;
        }
        nr.id = tmp;
        break;
    }
    askType("Room Type [" + nr.type + "]: ", nr.type, true);
    askPrice("Price per night [" + fmtPrice(nr.price) + "]: ", nr.price, true);
    askStatus("Status (Available/Occupied/Maintenance) [" + nr.status + "]: ", nr.status, true);
    askInt("Floor [" + to_string(nr.floor) + "]: ", 0, MAX_FLOOR, nr.floor, "floor", true);
    askInt("Capacity [" + to_string(nr.capacity) + "]: ", 1, MAX_CAPACITY, nr.capacity, "capacity",
           true);

    if (nr.id == oldId) {
        *tree.find(oldId) = nr;
    } else {  // changing the key = delete the old node, insert a new one
        tree.removeByCopying(oldId);
        tree.insert(nr);
        cout << "Room ID changed: " << oldId << " -> " << nr.id
             << " (old node deleted, new node inserted).\n";
    }
    cout << "Room updated:\n";
    printHeader();
    printRoom(nr);
}

enum TraversalKind { IN_ORDER, PRE_ORDER, POST_ORDER, LEVEL_ORDER };

static void displayTree(const RoomBST& tree, TraversalKind kind) {
    if (tree.empty()) { cout << "The tree is empty.\n"; return; }
    const char* title[] = {"In-order (LNR)", "Pre-order (NLR)", "Post-order (LRN)",
                           "Level-order (BFS)"};
    cout << title[kind] << ":\n";
    printHeader();
    int n = 0;
    Visitor v = [&n](const Room& r) { printRoom(r); n++; };
    if (kind == IN_ORDER) tree.inorder(v);
    else if (kind == PRE_ORDER) tree.preorder(v);
    else if (kind == POST_ORDER) tree.postorder(v);
    else tree.levelOrder(v);
    cout << n << " room(s).\n";
}

static void checkOrder(const RoomBST& tree) {
    cout << "BST ordering check: " << (tree.orderIsValid() ? "OK (in-order is ascending)" : "BROKEN")
         << "\n";
}

static void rotateAt(RoomBST& tree, int kind) {
    if (tree.empty()) { cout << "The tree is empty, nothing to rotate.\n"; return; }
    string id;
    askId("Room ID of the node to rotate at: ", id);
    if (!tree.find(id)) { cout << "Room " << id << " was not found. The tree was not changed.\n"; return; }
    cout << "Before:\n";
    tree.prettyPrint();
    string why;
    bool ok = false;
    const char* name = "";
    if (kind == 0) { ok = tree.rotateLeftAt(id, why); name = "Left"; }
    else if (kind == 1) { ok = tree.rotateRightAt(id, why); name = "Right"; }
    else if (kind == 2) { ok = tree.rotateLeftRightAt(id, why); name = "Left-Right"; }
    else { ok = tree.rotateRightLeftAt(id, why); name = "Right-Left"; }
    if (!ok) { cout << why << " The tree was not changed.\n"; return; }
    cout << name << " rotation done at " << id << ". After:\n";
    tree.prettyPrint();
    checkOrder(tree);
}

static void availableByPrice(const RoomBST& tree) {
    if (tree.empty()) { cout << "The tree is empty.\n"; return; }
    float lo, hi;
    askPrice("Minimum price: ", lo);
    while (true) {
        askPrice("Maximum price: ", hi);
        if (hi >= lo) break;
        cout << "  Maximum price must be at least the minimum price.\n";
    }
    int n = 0;
    tree.inorder([&](const Room& r) {
        if (r.status != "Available" || r.price < lo || r.price > hi) return;
        if (n == 0) { cout << "Available rooms from " << fixed << setprecision(2) << lo << " to " << hi << ":\n"; printHeader(); }
        printRoom(r);
        n++;
    });
    if (n == 0) cout << "No available room in that price range.\n";
    else cout << n << " room(s) found.\n";
}

static void saveTree(const RoomBST& tree) {
    string name = promptLine("File name to save to [rooms.txt]: ");
    if (name.empty()) name = "rooms.txt";
    int count = 0;
    if (tree.saveToFile(name, count)) cout << "Saved " << count << " room(s) to " << name << ".\n";
    else cout << "Could not write file " << name << ".\n";
}

static void loadTree(RoomBST& tree) {
    string name = promptLine("File name to load from [rooms.txt]: ");
    if (name.empty()) name = "rooms.txt";
    int loaded = 0, skipped = 0;
    if (tree.loadFromFile(name, loaded, skipped))
        cout << "Loaded " << loaded << " room(s) from " << name << "; skipped " << skipped
             << " invalid/duplicate line(s).\n";
    else cout << "Could not open file " << name << " (the current tree was kept).\n";
}

static void insertSampleData(RoomBST& tree) {
    // The first three are the records from the assignment; the rest give the tree
    // leaves, one-child nodes and two-child nodes for testing.
    const Room sample[] = {
        {"R030", "Deluxe", 90.0f, "Available", 3, 2},
        {"R010", "Standard", 50.0f, "Occupied", 1, 2},
        {"R050", "Suite", 150.0f, "Maintenance", 5, 4},
        {"R005", "Standard", 45.0f, "Available", 1, 1},
        {"R020", "Deluxe", 85.0f, "Available", 2, 2},
        {"R040", "Suite", 140.0f, "Occupied", 4, 4},
        {"R060", "Standard", 55.0f, "Available", 6, 2},
        {"R015", "Standard", 52.0f, "Maintenance", 1, 2},
        {"R025", "Deluxe", 88.0f, "Occupied", 2, 3},
        {"R070", "Suite", 160.0f, "Available", 7, 4}};
    int added = 0, skipped = 0;
    for (size_t i = 0; i < sizeof(sample) / sizeof(sample[0]); i++) {
        if (tree.insert(sample[i])) added++; else skipped++;
    }
    cout << "Sample data: " << added << " added, " << skipped << " skipped (already present).\n";
}

// ----------------------------------------------------------------------------
// main
// ----------------------------------------------------------------------------
int main(int argc, char** argv) {
    for (int i = 1; i < argc; i++)
        if (string(argv[i]) == "--echo") gEcho = true;

    RoomBST tree;  // freed by its destructor on every exit path
    try {
        while (true) {
            showMenu();
            string line = promptLine("Enter your choice: ");
            int choice;
            if (!parseIntStrict(line, choice)) choice = -1;
            switch (choice) {
                case 1: addRoom(tree); break;
                case 2: deleteRoom(tree, true); break;
                case 3: deleteRoom(tree, false); break;
                case 4: searchById(tree); break;
                case 5: searchByType(tree); break;
                case 6: updateRoom(tree); break;
                case 7: displayTree(tree, IN_ORDER); break;
                case 8: displayTree(tree, PRE_ORDER); break;
                case 9: displayTree(tree, POST_ORDER); break;
                case 10: displayTree(tree, LEVEL_ORDER); break;
                case 11: rotateAt(tree, 0); break;
                case 12: rotateAt(tree, 1); break;
                case 13: rotateAt(tree, 2); break;
                case 14: rotateAt(tree, 3); break;
                case 15: availableByPrice(tree); break;
                case 16:
                    if (tree.empty()) cout << "The tree is empty: potential revenue is 0.00.\n";
                    else cout << "Potential one-night revenue (rooms not under Maintenance): "
                              << fixed << setprecision(2) << potentialRevenue(tree) << "\n";
                    break;
                case 17: showStatistics(tree); break;
                case 18:
                    if (tree.empty()) cout << "The tree is empty (height 0, balanced).\n";
                    else cout << "Height: " << tree.height() << " level(s). Balanced: "
                              << (tree.isBalanced() ? "yes" : "NO")
                              << " (subtree heights differ by at most 1 at every node).\n";
                    break;
                case 19:
                    if (tree.empty()) cout << "The tree is empty.\n";
                    else {
                        cout << "(Root at the left, right subtree above, left subtree below)\n";
                        tree.prettyPrint();
                    }
                    break;
                case 20: saveTree(tree); break;
                case 21: loadTree(tree); break;
                case 22: insertSampleData(tree); break;
                case 23: tree.clear(); cout << "Tree cleared, memory released.\n"; break;
                case 0: cout << "Goodbye! Releasing memory...\n"; return 0;
                default: cout << "Invalid choice, please enter a number from the menu.\n";
            }
        }
    } catch (const InputEnded&) {
        cout << "\nEnd of input reached. Releasing memory and exiting.\n";
    }
    return 0;
}
