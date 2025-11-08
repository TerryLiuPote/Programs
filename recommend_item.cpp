#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <unordered_map>
#include <functional>

using namespace std;

// Node structure for the FP-tree
struct FPNode {
    string item;
    int count;
    map<string, shared_ptr<FPNode>> children;

    FPNode(string item, int count = 0) : item(item), count(count) {}
};

// Global data structures to store the FP-tree and auxiliary data
shared_ptr<FPNode> globalRoot;
unordered_map<string, int> globalItemFrequency;
unordered_map<string, shared_ptr<FPNode>> globalNodeMap;

// Function to load the FP-tree from a file
shared_ptr<FPNode> loadFPTree(const string& filename) {
    ifstream inputFile(filename);
    if (!inputFile) {
        cerr << "Error: Unable to open FP-tree file." << endl;
        return nullptr;
    }

    auto root = make_shared<FPNode>("null");
    string line;
    vector<shared_ptr<FPNode>> nodeStack;
    nodeStack.push_back(root);

    // Skip the title line "FP-tree:"
    if (!getline(inputFile, line) || line != "FP-tree:") {
        cerr << "Error: Invalid or missing title line in FP-tree file." << endl;
        return nullptr;
    }

    while (getline(inputFile, line)) {
        int depth = 0;
        while (line.substr(depth * 2, 2) == "|-") {
            depth++;
        }
        line = line.substr(depth * 2);

        size_t pos = line.find(" (");
        if (pos == string::npos) {
            cerr << "Error: Invalid line format: " << line << endl;
            continue;
        }

        string item = line.substr(0, pos);
        int count;
        try {
            count = stoi(line.substr(pos + 2, line.find(")") - pos - 2));
        } catch (const exception& e) {
            cerr << "Error: Failed to parse count in line: " << line << endl;
            continue;
        }

        auto newNode = make_shared<FPNode>(item, count);
        globalItemFrequency[item] += count;
        globalNodeMap[item] = newNode;

        if (depth == 0) {
            // Ensure the root node is correctly set
            root->children[item] = newNode;
        } else if (depth > 0 && depth <= nodeStack.size()) {
            nodeStack[depth - 1]->children[item] = newNode;
        } else {
            cerr << "Error: Depth mismatch in line: " << line << endl;
            continue;
        }

        if (nodeStack.size() > depth) {
            nodeStack.resize(depth);
        }
        nodeStack.push_back(newNode);
    }

    inputFile.close();
    globalRoot = root;
    return root;
}

// Function to recommend the best item based on user input
string recommendItem(const vector<string>& userItems) {
    map<string, int> itemScores;

    auto currentNode = globalRoot;
    for (const auto& item : userItems) {
        if (currentNode->children.find(item) != currentNode->children.end()) {
            currentNode = currentNode->children[item];
        } else {
            cerr << "Debug: Item \"" << item << "\" not found in current path." << endl;
            cerr << "Current node children: ";
            for (const auto& child : currentNode->children) {
                cerr << child.first << " ";
            }
            cerr << endl;
            return "No recommendation available.";
        }
    }

    for (const auto& child : currentNode->children) {
        itemScores[child.first] = child.second->count;
    }

    if (itemScores.empty()) {
        return "No recommendation available.";
    }

    return max_element(itemScores.begin(), itemScores.end(), [](const auto& a, const auto& b) {
        return a.second < b.second;
    })->first;
}

int main() {
    // Load the FP-tree from "Tree.out"
    if (!loadFPTree("Tree.out")) {
        return 1;
    }

    // Display global item frequencies (for debugging or verification)
    cout << "Global item frequencies:" << endl;
    for (const auto& [item, count] : globalItemFrequency) {
        cout << item << ": " << count << endl;
    }

    // Debugging: Display the FP-tree structure in the console
    cout << "FP-tree structure:" << endl;
    function<void(shared_ptr<FPNode>, string)> displayTree = [&](shared_ptr<FPNode> node, string prefix) {
        cout << prefix << node->item << " (" << node->count << ")" << endl;
        for (const auto& child : node->children) {
            displayTree(child.second, prefix + "|-");
        }
    };
    displayTree(globalRoot, "");

    // Get user input
    cout << "Enter items (space-separated): ";
    string input;
    getline(cin, input);

    vector<string> userItems;
    istringstream stream(input);
    string item;
    while (stream >> item) {
        userItems.push_back(item);
    }

    // Recommend the best item
    string recommendation = recommendItem(userItems);
    cout << "Recommended item: " << recommendation << endl;

    return 0;
}