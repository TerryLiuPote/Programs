#include <iostream>
#include <fstream>
#include <map>
#include <vector>
#include <string>
#include <algorithm>
#include <memory>
#include <sstream>
#include <unordered_map>

using namespace std;

// Node structure for the FP-tree
struct FPNode {
    string item;
    int count;
    map<string, shared_ptr<FPNode>> children;
    shared_ptr<FPNode> parent;

    FPNode(string item, shared_ptr<FPNode> parent = nullptr)
        : item(item), count(1), parent(parent) {}
};

// Function to insert a transaction into the FP-tree
void insertTransaction(shared_ptr<FPNode> root, const vector<string>& transaction, unordered_map<string, int>& itemFrequency, unordered_map<string, shared_ptr<FPNode>>& nodeMap) {
    auto currentNode = root;
    for (const auto& item : transaction) {
        if (currentNode->children.find(item) == currentNode->children.end()) {
            currentNode->children[item] = make_shared<FPNode>(item, currentNode);
            nodeMap[item] = currentNode->children[item];
        } else {
            currentNode->children[item]->count++;
        }
        itemFrequency[item]++;
        currentNode = currentNode->children[item];
    }
}

// Function to display the FP-tree
void displayTree(shared_ptr<FPNode> node, ostream& output, string prefix = "") {
    output << prefix << node->item << " (" << node->count << ")" << endl;
    for (const auto& child : node->children) {
        displayTree(child.second, output, prefix + "|-");
    }
}

// Main function
int main() {
    // Example transactions
    // Read transactions from input file "DB.in"
    ifstream inputFile("DB.in");
    if (!inputFile) {
        cerr << "Error: Unable to open input file." << endl;
        return 1;
    }

    vector<vector<string>> transactions;
    string line;
    while (getline(inputFile, line)) {
        vector<string> transaction;
        string item;
        istringstream stream(line);
        while (stream >> item) {
            transaction.push_back(item);
        }
        transactions.push_back(transaction);
    }
    inputFile.close();

    // Sort transactions by frequency of items (descending order)
    unordered_map<string, int> frequency;
    for (const auto& transaction : transactions) {
        for (const auto& item : transaction) {
            frequency[item]++;
        }
    }

    for (auto& transaction : transactions) {
        sort(transaction.begin(), transaction.end(), [&frequency](const string& a, const string& b) {
            return frequency[a] > frequency[b];
        });
    }

    // Build the FP-tree and auxiliary data structures
    auto root = make_shared<FPNode>("null");
    unordered_map<string, int> itemFrequency;
    unordered_map<string, shared_ptr<FPNode>> nodeMap;

    for (const auto& transaction : transactions) {
        insertTransaction(root, transaction, itemFrequency, nodeMap);
    }

    // Write the FP-tree to "Tree.out"
    ofstream outputFile("Tree.out");
    if (!outputFile) {
        cerr << "Error: Unable to create output file." << endl;
        return 1;
    }

    outputFile << "FP-tree:" << endl;
    displayTree(root, outputFile);
    outputFile.close();
    cout << "FP-tree has been written to \"Tree.out\" successfully." << endl;

    // Display item frequencies (for debugging or verification)
    cout << "Item frequencies:" << endl;
    for (const auto& [item, count] : itemFrequency) {
        cout << item << ": " << count << endl;
    }

    // Debugging: Display the FP-tree structure in the console
    cout << "FP-tree structure:" << endl;
    displayTree(root, cout);

    return 0;
}
/*
To input data for this program, create a file named "DB.in" in the same directory as the executable. 
Each line in the file should represent a transaction, with items separated by spaces. For example:

apple banana orange
banana orange
apple orange

Save the file and run the program. It will read the transactions from "DB.in", build the FP-tree, and write the output to "Tree.out".
*/