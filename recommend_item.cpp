#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <cmath>
#include <fcntl.h>
#include <io.h>

using namespace std;

// Node structure for the FP-tree
struct FPNode {
    string item;
    int count;
    vector<shared_ptr<FPNode>> children;
    shared_ptr<FPNode> parent; // 添加parent指针

    FPNode(string item, int count = 0) : item(item), count(count), parent(nullptr) {}
};

// Global data structures to store the FP-tree and auxiliary data
shared_ptr<FPNode> globalRoot;
unordered_map<string, int> globalItemFrequency;
unordered_map<string, shared_ptr<FPNode>> globalNodeMap;
// 新增：用于存储GNN分析的相似度矩阵
unordered_map<string, unordered_map<string, float>> globalSimilarityMatrix;
// 新增：全局路径变量，用于存储收集的路径
vector<vector<string>> g_paths;

// 新增：从GNN分析结果文件中加载相似度矩阵
bool loadSimilarityMatrix(const string& filename) {
    ifstream file(filename);
    if (!file) {
        wcerr << L"警告：无法打开GNN分析文件。将仅使用FP树进行推荐。" << endl;
        return false;
    }

    string line;
    bool inSimilaritySection = false;
    
    while (getline(file, line)) {
        if (line == "Item Similarity Matrix:") {
            inSimilaritySection = true;
            continue;
        }
        
        if (inSimilaritySection && !line.empty()) {
            size_t dashPos = line.find('-');
            size_t colonPos = line.find(':');
            if (dashPos != string::npos && colonPos != string::npos) {
                string item1 = line.substr(0, dashPos);
                string item2 = line.substr(dashPos + 1, colonPos - dashPos - 1);
                float similarity;
                
                try {
                    similarity = stof(line.substr(colonPos + 2));
                    globalSimilarityMatrix[item1][item2] = similarity;
                    globalSimilarityMatrix[item2][item1] = similarity; // 确保对称性
                } catch (const exception& e) {
                    wcerr << L"警告：无法解析行中的相似度: " << line.c_str() << endl;
                    continue;
                }
            }
        }
    }
    
    file.close();
    wcout << L"成功加载GNN分析的相似度矩阵。" << endl;
    return true;
}

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
            root->children.push_back(newNode);
            newNode->parent = root; // 设置父指针
        } else if (depth > 0 && depth <= nodeStack.size()) {
            nodeStack[depth - 1]->children.push_back(newNode);
            newNode->parent = nodeStack[depth - 1]; // 设置父指针
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

// 新增：参数结构体，用于配置推荐算法
struct RecommendationParams {
    float fpTreeWeight = 0.7f;       // FP树权重
    float similarityWeight = 0.3f;   // 相似度权重
    int topN = 3;                    // 返回前N个推荐结果
    bool useGNN = true;              // 是否使用GNN分析结果
};

// 新增：函数重载，支持传入推荐参数
vector<pair<string, float>> recommendItems(const unordered_set<string>& userItems, const RecommendationParams& params = RecommendationParams()) {
    map<string, float> itemScores;

    // 1. 使用FP树计算基础分数（递归版本）
    map<string, float> fpTreeScores; // 改为float类型以支持深度权重
    const int MAX_DEPTH = 3; // 最大递归深度
    
    // 收集所有包含用户物品的完整路径
    g_paths.clear();
    function<void(shared_ptr<FPNode>, vector<string>&)> collectPaths = [&](shared_ptr<FPNode> node, vector<string>& currentPath) {
        // 如果当前节点不是根节点，将其添加到路径中
        if (node != globalRoot) {
            currentPath.push_back(node->item);
        }
        
        // 如果当前节点有子节点，继续递归
        if (!node->children.empty()) {
            for (const auto& child : node->children) {
                collectPaths(child, currentPath);
            }
        } else {
            // 如果没有子节点，且路径中包含至少一个用户物品，则保存该路径
            bool containsUserItem = false;
            for (const auto& item : currentPath) {
                if (userItems.find(item) != userItems.end()) {
                    containsUserItem = true;
                    break;
                }
            }
            if (containsUserItem) {
                g_paths.push_back(currentPath);
            }
        }
        
        // 回溯
        if (node != globalRoot) {
            currentPath.pop_back();
        }
    };
    
    // 收集包含用户物品的路径
    vector<string> currentPath;
    collectPaths(globalRoot, currentPath);
    
    // 分析这些路径，找出共同的后续节点
    for (const auto& path : g_paths) {
        // 找出路径中所有用户物品的位置
        vector<int> userItemPositions;
        for (int i = 0; i < path.size(); i++) {
            if (userItems.find(path[i]) != userItems.end()) {
                userItemPositions.push_back(i);
            }
        }
        
        // 如果路径中没有用户物品，跳过
        if (userItemPositions.empty()) {
            continue;
        }
        
        // 获取最后一个用户物品的位置
        int lastUserItemPos = userItemPositions.back();
        
        // 计算该用户物品之后的节点的贡献值
        for (int i = lastUserItemPos + 1; i < path.size(); i++) {
            if (userItems.find(path[i]) == userItems.end()) { // 排除用户物品自身
                // 根据深度调整分数权重，深度越大权重越小
                int depth = i - lastUserItemPos;
                float depthWeight = 1.0f / (depth + 1);
                fpTreeScores[path[i]] += depthWeight;
            }
        }
    }

    // 2. 计算物品总频次的最大值，用于归一化
    int maxFrequency = 0;
    for (const auto& [_, count] : globalItemFrequency) {
        if (count > maxFrequency) maxFrequency = count;
    }

    // 3. 初始化分数
    for (const auto& [item, _] : globalItemFrequency) {
        if (userItems.find(item) == userItems.end()) { // 排除用户已选择的物品
            itemScores[item] = 0.0f;
        }
    }

    // 4. 应用FP树分数
    int maxFpTreeScore = 0;
    for (const auto& [_, score] : fpTreeScores) {
        if (score > maxFpTreeScore) maxFpTreeScore = score;
    }
    
    for (const auto& [item, score] : fpTreeScores) {
        if (maxFpTreeScore > 0) {
            itemScores[item] += (score / (float)maxFpTreeScore) * params.fpTreeWeight;
        }
    }

    // 5. 如果启用，应用相似度分数
    if (params.useGNN && !globalSimilarityMatrix.empty()) {
        for (const auto& [targetItem, _] : itemScores) {
            float simScore = 0.0f;
            int validSims = 0;
            
            for (const auto& userItem : userItems) {
                if (globalSimilarityMatrix.find(userItem) != globalSimilarityMatrix.end() &&
                    globalSimilarityMatrix[userItem].find(targetItem) != globalSimilarityMatrix[userItem].end()) {
                    simScore += globalSimilarityMatrix[userItem][targetItem];
                    validSims++;
                }
            }
            
            if (validSims > 0) {
                itemScores[targetItem] += (simScore / validSims) * params.similarityWeight;
            }
        }
    }

    // 6. 转换为向量并排序
    vector<pair<string, float>> scoredItems(itemScores.begin(), itemScores.end());
    sort(scoredItems.begin(), scoredItems.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });

    // 7. 截取前N个结果
    if (scoredItems.size() > (size_t)params.topN) {
        scoredItems.resize(params.topN);
    }

    return scoredItems;
}

// 保持原有函数接口兼容性
string recommendItem(const unordered_set<string>& userItems) {
    auto recommendations = recommendItems(userItems);
    if (recommendations.empty()) {
        return "No recommendation available.";
    }
    return recommendations[0].first;
}

// 新增：解析命令行参数
RecommendationParams parseCommandLineArgs(int argc, char* argv[]) {
    RecommendationParams params;
    
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "--fp-weight" && i + 1 < argc) {
            try {
                params.fpTreeWeight = stof(argv[i + 1]);
                i++;
            } catch (...) {
                cerr << "Warning: Invalid value for --fp-weight, using default." << endl;
            }
        } else if (arg == "--sim-weight" && i + 1 < argc) {
            try {
                params.similarityWeight = stof(argv[i + 1]);
                i++;
            } catch (...) {
                cerr << "Warning: Invalid value for --sim-weight, using default." << endl;
            }
        } else if (arg == "--top-n" && i + 1 < argc) {
            try {
                params.topN = stoi(argv[i + 1]);
                i++;
            } catch (...) {
                cerr << "Warning: Invalid value for --top-n, using default." << endl;
            }
        } else if (arg == "--no-gnn") {
            params.useGNN = false;
        }
    }
    
    // 确保权重和为1
    float totalWeight = params.fpTreeWeight + params.similarityWeight;
    if (totalWeight > 0) {
        params.fpTreeWeight /= totalWeight;
        params.similarityWeight /= totalWeight;
    }
    
    return params;
}

int main(int argc, char* argv[]) {
    // 设置控制台为UTF-8编码
    _setmode(_fileno(stdout), _O_U8TEXT);
    _setmode(_fileno(stderr), _O_U8TEXT);
    
    // 解析命令行参数
    RecommendationParams params = parseCommandLineArgs(argc, argv);
    
    wcout << L"推荐参数：" << endl;
    wcout << L"- FP-Tree 权重：" << params.fpTreeWeight << endl;
    wcout << L"- 相似度权重：" << params.similarityWeight << endl;
    wcout << L"- 前N个结果：" << params.topN << endl;
    wcout << L"- 使用GNN分析：" << (params.useGNN ? L"是" : L"否") << endl;

    // Load the FP-tree from "Tree.out"
    if (!loadFPTree("Tree.out")) {
        return 1;
    }

    // 如果启用GNN，则加载相似度矩阵
    if (params.useGNN) {
        loadSimilarityMatrix("GNN_Analysis.out");
    }

    // Display global item frequencies (for debugging or verification)
    wcout << L"\n全局物品频率：" << endl;
    for (const auto& [item, count] : globalItemFrequency) {
        wcout << item.c_str() << L": " << count << endl;
    }

    // 获取用户输入
    unordered_set<string> userItems;
    wcout << L"\n请输入物品（空格分隔）：";
    wstring input;
    getline(wcin, input);

    wistringstream stream(input);
    wstring witem;
    while (stream >> witem) {
        userItems.insert(string(witem.begin(), witem.end()));
    }
    
    // 如果用户没有输入任何物品，使用默认测试用例
    if (userItems.empty()) {
        wcout << L"\n未输入任何物品，使用默认测试用例..." << endl;
        userItems = {"mango", "pear"};
    }
    
    // 显示使用的物品
    wcout << L"\n使用的物品：";
    for (const auto& userItem : userItems) {
        wcout << userItem.c_str() << L" ";
    }
    wcout << endl;

    // Debugging: Display the FP-tree structure in the console
    wcout << L"\nFP树结构：" << endl;
    function<void(shared_ptr<FPNode>, string)> displayTree = [&](shared_ptr<FPNode> node, string prefix) {
        // Skip root node from display
        if (node == globalRoot) {
            for (const auto& child : node->children) {
                displayTree(child.second, "");
            }
            return;
        }
        
        wcout << prefix.c_str() << node->item.c_str() << L" (" << node->count << L")" << endl;
        for (const auto& child : node->children) {
            displayTree(child.second, prefix + "|-");
        }
    };
    
    // Call displayTree starting from root
    displayTree(globalRoot, "");

    // Recommend items with parameters
    vector<pair<string, float>> recommendations = recommendItems(userItems, params);
    
    if (recommendations.empty()) {
        wcout << L"没有可用的推荐。" << endl;
    } else {
        wcout << L"\n推荐物品（带分数）：" << endl;
        for (size_t i = 0; i < recommendations.size(); i++) {
            wcout << (i + 1) << L". " << recommendations[i].first.c_str() 
                 << L" (分数: " << recommendations[i].second << L")" << endl;
        }
    }
    
    // 显示更详细的递归分析结果
    wcout << L"\n详细递归分析：" << endl;
    wcout << L"推荐物品及其递归深度和贡献：" << endl;
    const int MAX_DEPTH = 3; // 详细分析包含用户物品的路径
        wcout << endl << L"=== 详细路径分析结果 ===" << endl;
        
        if (g_paths.empty()) {
            wcout << L"未找到包含用户物品的路径。" << endl;
        } else {
            for (int i = 0; i < g_paths.size(); i++) {
                wcout << L"路径 " << (i + 1) << L": ";
                for (const auto& item : g_paths[i]) {
                    wcout << item.c_str() << L" -> ";
                }
                wcout << L"结束" << endl;
                
                // 分析路径中的推荐物品
                vector<int> userItemPositions;
                for (int j = 0; j < g_paths[i].size(); j++) {
                    if (userItems.find(g_paths[i][j]) != userItems.end()) {
                        userItemPositions.push_back(j);
                    }
                }
                
                if (!userItemPositions.empty()) {
                    int lastUserItemPos = userItemPositions.back();
                    wcout << L"  推荐分析（从用户物品开始）：" << endl;
                    
                    for (int j = lastUserItemPos + 1; j < g_paths[i].size(); j++) {
                        if (userItems.find(g_paths[i][j]) == userItems.end()) {
                            int depth = j - lastUserItemPos;
                            float depthWeight = 1.0f / (depth + 1);
                            wcout << L"    - " << g_paths[i][j].c_str() << L" (深度: " << depth << L", 深度权重: " << depthWeight << L")" << endl;
                        }
                    }
                }
            }
        }
        
        // 原始递归分析（用于显示树结构）
        wcout << endl << L"=== 树结构递归分析 ===" << endl;
        function<void(shared_ptr<FPNode>, int depth)> printTree = [&](shared_ptr<FPNode> node, int depth) {
            const int MAX_DEPTH = 4; // 最大递归深度
            if (depth > MAX_DEPTH) {
                return;
            }
            
            // Skip root node
            if (node == globalRoot) {
                for (const auto& child : node->children) {
                    printTree(child, depth);
                }
                return;
            }
            
            string indent(depth * 2, ' ');
            wcout << indent.c_str() << node->item.c_str() << L" (计数: " << node->count << L")" << endl;
            
            for (const auto& child : node->children) {
                printTree(child, depth + 1);
            }
        };
        printTree(globalRoot, 0);

    return 0;
}