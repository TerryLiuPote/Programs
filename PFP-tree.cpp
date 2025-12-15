#include <iostream>
#include <fstream>
#include <map>
#include <vector>
#include <string>
#include <algorithm>
#include <memory>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <cmath>

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

// 定义图节点结构
struct GraphNode {
    string item;
    int count;
    vector<int> neighbors;
    vector<float> features; // 节点特征
};

// 图数据结构
struct Graph {
    vector<GraphNode> nodes;
    unordered_map<string, int> nodeMap; // 物品名到节点索引的映射
};

// 辅助函数：插入交易到FP树
void insertTransaction(shared_ptr<FPNode> root, const vector<string>& transaction, 
                     unordered_map<string, int>& itemFrequency, 
                     unordered_map<string, shared_ptr<FPNode>>& nodeMap) {
    auto current = root;
    
    for (const auto& item : transaction) {
        itemFrequency[item]++;
        
        // 检查当前节点的子节点中是否已存在该物品
        if (current->children.find(item) != current->children.end()) {
            // 已存在，增加计数
            current->children[item]->count++;
        } else {
            // 不存在，创建新节点
            auto newNode = make_shared<FPNode>(item, current);
            current->children[item] = newNode;
            nodeMap[item] = newNode;
        }
        
        // 移动到下一个节点
        current = current->children[item];
    }
}

// 简单的GNN辅助类实现
namespace GNNHelper {
    using Graph = ::Graph;
    using GraphNode = ::GraphNode;
    
    Graph convertToGraph(shared_ptr<FPNode> root) {
        Graph graph;
        int index = 0;
        
        // 简单实现：收集所有物品节点
        function<void(shared_ptr<FPNode>)> traverse = [&](shared_ptr<FPNode> node) {
            if (node->item != "null" && graph.nodeMap.find(node->item) == graph.nodeMap.end()) {
                graph.nodeMap[node->item] = index;
                GraphNode gNode;
                gNode.item = node->item;
                gNode.count = node->count;
                graph.nodes.push_back(gNode);
                index++;
            }
            
            // 遍历子节点并建立连接
            for (const auto& [childItem, childNode] : node->children) {
                if (node->item != "null") {
                    int parentIndex = graph.nodeMap[node->item];
                    int childIndex = graph.nodeMap[childItem];
                    graph.nodes[parentIndex].neighbors.push_back(childIndex);
                    graph.nodes[childIndex].neighbors.push_back(parentIndex);
                }
                traverse(childNode);
            }
        };
        
        traverse(root);
        return graph;
    }
    
    unordered_map<string, unordered_map<string, float>> computeItemSimilarity(const Graph& graph) {
        unordered_map<string, unordered_map<string, float>> similarityMatrix;
        
        // 简单实现：基于共现频率计算相似度
        for (const auto& node : graph.nodes) {
            for (const auto& otherNode : graph.nodes) {
                if (node.item != otherNode.item) {
                    // 简单相似度计算，实际项目中可能需要更复杂的算法
                    float similarity = static_cast<float>(min(node.count, otherNode.count)) / 
                                     max(node.count, otherNode.count);
                    similarityMatrix[node.item][otherNode.item] = similarity;
                }
            }
        }
        
        return similarityMatrix;
    }
    
    void saveGraphAnalysis(const Graph& graph, 
                          const unordered_map<string, unordered_map<string, float>>& similarityMatrix, 
                          const string& filename) {
        ofstream outputFile(filename);
        if (!outputFile) {
            cerr << "Error: Unable to create GNN analysis output file." << endl;
            return;
        }
        
        // 保存节点信息
        outputFile << "Nodes:" << endl;
        for (const auto& node : graph.nodes) {
            outputFile << node.item << " (" << node.count << ")" << endl;
        }
        
        // 保存相似度矩阵
        outputFile << "\nSimilarity Matrix:" << endl;
        for (const auto& [item1, similarities] : similarityMatrix) {
            for (const auto& [item2, similarity] : similarities) {
                outputFile << item1 << "-" << item2 << ": " << similarity << endl;
            }
        }
        
        outputFile.close();
    }
}

// Function to display the FP-tree
void displayTree(shared_ptr<FPNode> node, ostream& output, string prefix = "") {
    output << prefix << node->item << " (" << node->count << ")" << endl;
    for (const auto& child : node->children) {
        displayTree(child.second, output, prefix + "|-" );
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
    
    // 执行图神经网络辅助分析
    cout << "\nPerforming GNN auxiliary analysis..." << endl;
    GNNHelper::Graph graph = GNNHelper::convertToGraph(root);
    auto similarityMatrix = GNNHelper::computeItemSimilarity(graph);
    GNNHelper::saveGraphAnalysis(graph, similarityMatrix, "GNN_Analysis.out");

    return 0;
}