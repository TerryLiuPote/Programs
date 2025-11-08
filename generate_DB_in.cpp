#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdlib>
#include <ctime>
#include <algorithm>

using namespace std;

int main() {
    // Seed the random number generator
    srand(static_cast<unsigned>(time(0)));

    // Define possible items
    vector<string> items = {"apple", "banana", "orange", "grape", "pear", "peach", "mango", "cherry"};

    // Number of transactions to generate
    int numTransactions;

    // Get user input for number of transactions
    cout << "Enter the number of transactions: ";
    cin >> numTransactions;

    // Open the file "DB.in" for writing
    ofstream outputFile("DB.in");
    if (!outputFile) {
        cerr << "Error: Unable to create output file." << endl;
        return 1;
    }

    // Generate random transactions
    for (int i = 0; i < numTransactions; ++i) {
        int itemsPerTransaction = rand() % items.size() + 1; // Random number of items per transaction (1 to items.size())
        vector<string> transaction;
        while (transaction.size() < itemsPerTransaction) {
            string item = items[rand() % items.size()];
            if (find(transaction.begin(), transaction.end(), item) == transaction.end()) {
                transaction.push_back(item);
            }
        }
        for (size_t k = 0; k < transaction.size(); ++k) {
            outputFile << transaction[k];
            if (k < transaction.size() - 1) {
                outputFile << " ";
            }
        }
        outputFile << endl;
    }

    outputFile.close();
    cout << "File \"DB.in\" with random transactions has been created successfully." << endl;

    return 0;
}