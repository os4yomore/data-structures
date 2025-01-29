
//a lot of my functionality here was was just functions from my first question refurbished to fit this question's specifics 
//so chatgpt had the same level of influence
// Main structure to hold package information
// Each package is a node in our binary search tree
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <functional>
#include <stack>
#include <algorithm>
#include <vector>
#include <queue>
#include <list>
#include <map>
using namespace std;

struct Package {
    int trackingNumber;
    string deliveryAddress;
    string customerName;
    string recipientName;    // Added for recipient tracking
    int urgencyLevel;        // 1 is most urgent, 5 is least urgent
    string status;           // "pending", "in-van", "delivered"
    string deliveryRoute;    // Added for route tracking
    Package* leftChild;    
    Package* rightChild;   

    Package(int tracking, string address, string customer, string recipient, 
            int urgency, string route) 
        : trackingNumber(tracking), deliveryAddress(address), customerName(customer), 
          recipientName(recipient), urgencyLevel(urgency), status("pending"),
          deliveryRoute(route), leftChild(nullptr), rightChild(nullptr) {}
};

// Global data structures
queue<Package*> deliveryVan;              // For current deliveries
list<Package*> deliveredPackages;         // Linked list for delivered packages
list<Package*> pendingPackages;           // Linked list for pending packages
stack<pair<string, Package*>> undoStack;  // For undo operations
stack<pair<string, Package*>> redoStack;  // For redo operations

struct PackageUrgencySort {
    bool operator()(Package* first, Package* second) {
        if (first->urgencyLevel == second->urgencyLevel) {
            return first->trackingNumber > second->trackingNumber;
        }
        return first->urgencyLevel > second->urgencyLevel;  
    }
};

// Enhanced version of collectPackagesForDelivery
void collectPackagesForDelivery(Package* root, priority_queue<Package*, vector<Package*>, PackageUrgencySort>& priorityQueue) {
    if (!root) return;
    if (root->status == "pending") {  // Only add pending packages
        priorityQueue.push(root);
    }
    collectPackagesForDelivery(root->leftChild, priorityQueue);
    collectPackagesForDelivery(root->rightChild, priorityQueue);
}

// Enhanced file operations
void updateDeliveryVanFile() {
    ofstream vanFile("Truck.txt", ios::trunc);
    if (!vanFile.is_open()) {
        cout << "Error: Could not save delivery van state!" << endl;
        return;
    }

    queue<Package*> tempVan = deliveryVan;
    while (!tempVan.empty()) {
        Package* pkg = tempVan.front();
        tempVan.pop();
        vanFile << pkg->trackingNumber << ", " 
                << pkg->customerName << ", "
                << pkg->recipientName << ", "  
                << pkg->deliveryAddress << ", " 
                << pkg->deliveryRoute << ", "
                << pkg->urgencyLevel << "\n";
    }
    vanFile.close();
}


void completeDeliveries() {
    if (deliveryVan.empty()) {
        cout << "No packages in the van to deliver!" << endl;
        return;
    }

    ofstream deliveryLog("DeliveredParcels.txt", ios::app);
    if (!deliveryLog.is_open()) {
        cout << "Error: Cannot access delivery log!" << endl;
        return;
    }

    while (!deliveryVan.empty()) {
        Package* pkg = deliveryVan.front();
        deliveryVan.pop();
        
        pkg->status = "delivered";
        deliveredPackages.push_back(pkg);
        pendingPackages.remove(pkg);
        
        // Add to undo stack
        undoStack.push({"deliver", pkg});
        
        deliveryLog << pkg->trackingNumber << ", " 
                   << pkg->customerName << ", "
                   << pkg->recipientName << ", "
                   << pkg->deliveryAddress << ", "
                   << pkg->deliveryRoute << ", "
                   << pkg->urgencyLevel << "\n";
    }

    deliveryLog.close();
    ofstream vanFile("Truck.txt", ios::trunc);
    vanFile.close();

    cout << "All packages delivered successfully!" << endl;
}

// Enhanced van state restoration
void restoreDeliveryVanState() {
    ifstream vanFile("Truck.txt");
    if (!vanFile.is_open()) {
        cout << "No previous delivery van data found." << endl;
        return;
    }

    string line;
    while (getline(vanFile, line)) {
        stringstream ss(line);
        string tracking, customer, recipient, address, route, urgency;

        getline(ss, tracking, ',');
        getline(ss, customer, ',');
        getline(ss, recipient, ',');
        getline(ss, address, ',');
        getline(ss, route, ',');
        getline(ss, urgency, ',');

        // Clean strings
        customer = customer.substr(customer.find_first_not_of(" "));
        customer = customer.substr(0, customer.find_last_not_of(" ") + 1);
        recipient = recipient.substr(recipient.find_first_not_of(" "));
        recipient = recipient.substr(0, recipient.find_last_not_of(" ") + 1);
        address = address.substr(address.find_first_not_of(" "));
        address = address.substr(0, address.find_last_not_of(" ") + 1);
        route = route.substr(route.find_first_not_of(" "));
        route = route.substr(0, route.find_last_not_of(" ") + 1);

        Package* pkg = new Package(
            stoi(tracking), 
            address, 
            customer,
            recipient,
            stoi(urgency),
            route
        );
        pkg->status = "in-van";
        deliveryVan.push(pkg);
    }
    vanFile.close();
}

// Enhanced package finding functions
Package* findPackageByRecipient(Package* root, string recipient) {
    if (!root) return nullptr;
    
    if (root->recipientName == recipient) return root;
    
    Package* left = findPackageByRecipient(root->leftChild, recipient);
    if (left) return left;
    
    return findPackageByRecipient(root->rightChild, recipient);
}

Package* findSmallestTracking(Package* node) {
    while (node->leftChild) {
        node = node->leftChild;
    }
    return node;
}

// Enhanced package removal with undo support
Package* removePackage(Package* root, int tracking) {
    if (!root) {
        cout << "Package not found!" << endl;
        return root;
    }

    if (tracking < root->trackingNumber) {
        root->leftChild = removePackage(root->leftChild, tracking);
    } 
    else if (tracking > root->trackingNumber) {
        root->rightChild = removePackage(root->rightChild, tracking);
    }
    else {
        // Add to undo stack before removing
        undoStack.push({"remove", root});
        
        if (!root->leftChild && !root->rightChild) {
            delete root;
            return nullptr;
        }
        else if (!root->leftChild) {
            Package* temp = root->rightChild;
            delete root;
            return temp;
        }
        else if (!root->rightChild) {
            Package* temp = root->leftChild;
            delete root;
            return temp;
        }
        else {
            Package* successor = findSmallestTracking(root->rightChild);
            root->trackingNumber = successor->trackingNumber;
            root->customerName = successor->customerName;
            root->recipientName = successor->recipientName;
            root->deliveryAddress = successor->deliveryAddress;
            root->deliveryRoute = successor->deliveryRoute;
            root->urgencyLevel = successor->urgencyLevel;
            root->status = successor->status;
            root->rightChild = removePackage(root->rightChild, successor->trackingNumber);
        }
    }
    return root;
}

// Enhanced package finding
Package* findPackage(Package* root, int tracking) {
    if (!root || root->trackingNumber == tracking) return root;
    
    if (tracking < root->trackingNumber) {
        return findPackage(root->leftChild, tracking);
    }
    return findPackage(root->rightChild, tracking);
}

// Enhanced file operations
void savePackageToFile(ofstream &outFile, Package* root) {
    if (root) {
        outFile << root->trackingNumber << ", " 
               << root->customerName << ", "
               << root->recipientName << ", "
               << root->deliveryAddress << ", "
               << root->deliveryRoute << ", "
               << root->urgencyLevel << ", "
               << root->status << "\n";
        
        savePackageToFile(outFile, root->leftChild);
        savePackageToFile(outFile, root->rightChild);
    }
}

void updatePackageDatabase(Package* root) {
    ofstream outFile("Parcels.txt", ios::trunc);
    savePackageToFile(outFile, root);
    outFile.close();
}

// Enhanced van loading with priority and FIFO
void loadVanForDelivery(Package*& root) {
    if (deliveryVan.size() >= 5) {
        cout << "Van is full! Complete current deliveries first." << endl;
        return;
    }

    priority_queue<Package*, vector<Package*>, PackageUrgencySort> priorityQueue;
    collectPackagesForDelivery(root, priorityQueue);

    while (!priorityQueue.empty() && deliveryVan.size() < 5) {
        Package* pkg = priorityQueue.top();
        priorityQueue.pop();
        
        if (pkg->status == "pending") {
            pkg->status = "in-van";
            deliveryVan.push(pkg);
            undoStack.push({"load", pkg});
        }
    }
    
    updateDeliveryVanFile();
    cout << "Van loaded with highest priority packages!" << endl;
}

// Enhanced package addition with undo support
void addPackageToSystem(Package*& root, Package* newPackage) {
    undoStack.push({"add", newPackage});
    pendingPackages.push_back(newPackage);
    
    if (!root) {
        root = newPackage;
        return;
    }
    
    if (newPackage->trackingNumber < root->trackingNumber) {
        addPackageToSystem(root->leftChild, newPackage);
    } else {
        addPackageToSystem(root->rightChild, newPackage);
    }
}

// Enhanced database loading
void loadPackageDatabase(Package*& root) {
    ifstream inFile("Parcels.txt");
    if (!inFile.is_open()) {
        cout << "Could not open package database!" << endl;
        return;
    }

    string line;
    while (getline(inFile, line)) {
        stringstream ss(line);
        string tracking, customer, recipient, address, route, urgency, status;

        getline(ss, tracking, ',');
        getline(ss, customer, ',');
        getline(ss, recipient, ',');
        getline(ss, address, ',');
        getline(ss, route, ',');
        getline(ss, urgency, ',');
        getline(ss, status, ',');

        // Clean strings
        customer = customer.substr(customer.find_first_not_of(" "));
        customer = customer.substr(0, customer.find_last_not_of(" ") + 1);
        recipient = recipient.substr(recipient.find_first_not_of(" "));
        recipient = recipient.substr(0, recipient.find_last_not_of(" ") + 1);
        address = address.substr(address.find_first_not_of(" "));
        address = address.substr(0, address.find_last_not_of(" ") + 1);
        route = route.substr(route.find_first_not_of(" "));
        route = route.substr(0, route.find_last_not_of(" ") + 1);

        Package* newPackage = new Package(
            stoi(tracking),
            address,
            customer,
            recipient,
            stoi(urgency),
            route
        );
        newPackage->status = status;

        addPackageToSystem(root, newPackage);
        if (status == "delivered") {
            deliveredPackages.push_back(newPackage);
        } else if (status == "pending") {
            pendingPackages.push_back(newPackage);
        }
    }

    inFile.close();
    cout << "Package database loaded successfully!" << endl;
}

// Enhanced summary generation
void generateDeliverySummary(Package* root) {
    cout << "\n=== Enhanced Delivery System Report ===\n";

    // Total deliveries count
    cout << "Total successful deliveries: " << deliveredPackages.size() << endl;

    // Route analysis
    map<string, int> routeCounts;
    for (Package* pkg : deliveredPackages) {
        routeCounts[pkg->deliveryRoute]++;
    }
    
    cout << "\nDelivery Routes Used:\n";
    for (const auto& route : routeCounts) {
        cout << "Route " << route.first << ": " << route.second << " deliveries\n";
    }

    // Pending deliveries by priority
    cout << "\nPending Deliveries by Priority:\n";
    priority_queue<Package*, vector<Package*>, PackageUrgencySort> priorityQueue;
    collectPackagesForDelivery(root, priorityQueue);
    
    map<int, int> priorityCounts;
    while (!priorityQueue.empty()) {
        Package* pkg = priorityQueue.top();
        priorityQueue.pop();
        priorityCounts[pkg->urgencyLevel]++;
    }
    
    for (const auto& count : priorityCounts) {
        cout << "Priority " << count.first << ": " << count.second << " packages\n";
    }
}

// Enhanced package creation
void createNewPackage(Package*& root) {
    string customer, recipient, address, route;
    int urgency, tracking;

    cout << "Enter customer name: ";
    cin.ignore();
    getline(cin, customer);

    cout << "Enter recipient name: ";
    getline(cin, recipient);

    cout << "Enter delivery address: ";
    getline(cin, address);

    cout << "Enter delivery route: ";
    getline(cin, route);

    cout << "Enter urgency level (1-5, 1 is most urgent): ";
    do {
        cin >> urgency;
        if (urgency < 1 || urgency > 5) {
            cout << "Please enter a valid urgency level (1-5): ";
        }
    } while (urgency < 1 || urgency > 5);

    bool isUnique = false;
    while (!isUnique) {
        cout << "Enter tracking number: ";
        cin >> tracking;

        if (findPackage(root, tracking)) {
            cout << "This tracking number already exists. Please try another." << endl;
        } else {
            isUnique = true;
        }
    }

    Package* newPackage = new Package(tracking, address, customer, recipient, urgency, route);
    addPackageToSystem(root, newPackage);
}

// New undo functionality
void undoLastAction(Package*& root) {
    if (undoStack.empty()) {
        cout << "No actions to undo!" << endl;
        return;
    }

    pair<string, Package*> lastAction = undoStack.top();
    undoStack.pop();
    
    if (lastAction.first == "add") {
        // Remove package from BST and pending list
        removePackage(root, lastAction.second->trackingNumber);
        pendingPackages.remove(lastAction.second);
        redoStack.push(lastAction);
    }
    else if (lastAction.first == "remove") {
        // Re-add package to BST
        addPackageToSystem(root, lastAction.second);
        redoStack.push({"add", lastAction.second});
    }
    else if (lastAction.first == "load") {
        // Remove from delivery van and set status back to pending
        queue<Package*> tempVan;
        while (!deliveryVan.empty()) {
            Package* pkg = deliveryVan.front();
            deliveryVan.pop();
            if (pkg != lastAction.second) {
                tempVan.push(pkg);
            }
        }
        deliveryVan = tempVan;
        lastAction.second->status = "pending";
        redoStack.push(lastAction);
        updateDeliveryVanFile();
    }
    else if (lastAction.first == "deliver") {
        // Move back to delivery van from delivered list
        lastAction.second->status = "in-van";
        deliveryVan.push(lastAction.second);
        deliveredPackages.remove(lastAction.second);
        pendingPackages.push_back(lastAction.second);
        redoStack.push(lastAction);
        updateDeliveryVanFile();
    }
    
    updatePackageDatabase(root);
    cout << "Last action undone successfully!" << endl;
}

// New redo functionality
void redoLastAction(Package*& root) {
    if (redoStack.empty()) {
        cout << "No actions to redo!" << endl;
        return;
    }

    pair<string, Package*> lastAction = redoStack.top();
    redoStack.pop();
    
    if (lastAction.first == "add") {
        addPackageToSystem(root, lastAction.second);
        undoStack.push(lastAction);
    }
    else if (lastAction.first == "remove") {
        removePackage(root, lastAction.second->trackingNumber);
        undoStack.push(lastAction);
    }
    else if (lastAction.first == "load") {
        lastAction.second->status = "in-van";
        deliveryVan.push(lastAction.second);
        undoStack.push(lastAction);
        updateDeliveryVanFile();
    }
    else if (lastAction.first == "deliver") {
        lastAction.second->status = "delivered";
        deliveredPackages.push_back(lastAction.second);
        pendingPackages.remove(lastAction.second);
        queue<Package*> tempVan;
        while (!deliveryVan.empty()) {
            Package* pkg = deliveryVan.front();
            deliveryVan.pop();
            if (pkg != lastAction.second) {
                tempVan.push(pkg);
            }
        }
        deliveryVan = tempVan;
        undoStack.push(lastAction);
        updateDeliveryVanFile();
    }
    
    updatePackageDatabase(root);
    cout << "Last action redone successfully!" << endl;
}

// Main menu function
void displayMenu() {
    cout << "\n=== Package Delivery System ===\n";
    cout << "1. Register New Package\n";
    cout << "2. Load Delivery Van\n";
    cout << "3. Complete Deliveries\n";
    cout << "4. Generate Delivery Summary\n";
    cout << "5. Find Package by Tracking Number\n";
    cout << "6. Find Package by Recipient\n";
    cout << "7. Undo Last Action\n";
    cout << "8. Redo Last Action\n";
    cout << "9. Exit\n";
    cout << "Enter your choice: ";
}

int main() {
    Package* root = nullptr;
    loadPackageDatabase(root);
    restoreDeliveryVanState();

    int choice;
    do {
        displayMenu();
        cin >> choice;

        switch (choice) {
            case 1:
                createNewPackage(root);
                break;
            case 2:
                loadVanForDelivery(root);
                break;
            case 3:
                completeDeliveries();
                break;
            case 4:
                generateDeliverySummary(root);
                break;
            case 5: {
                int tracking;
                cout << "Enter tracking number: ";
                cin >> tracking;
                Package* found = findPackage(root, tracking);
                if (found) {
                    cout << "Package found!\n";
                    cout << "Customer: " << found->customerName << "\n";
                    cout << "Recipient: " << found->recipientName << "\n";
                    cout << "Status: " << found->status << "\n";
                } else {
                    cout << "Package not found!\n";
                }
                break;
            }
            case 6: {
                string recipient;
                cout << "Enter recipient name: ";
                cin.ignore();
                getline(cin, recipient);
                Package* found = findPackageByRecipient(root, recipient);
                if (found) {
                    cout << "Package found!\n";
                    cout << "Tracking: " << found->trackingNumber << "\n";
                    cout << "Customer: " << found->customerName << "\n";
                    cout << "Status: " << found->status << "\n";
                } else {
                    cout << "Package not found!\n";
                }
                break;
            }
            case 7:
                undoLastAction(root);
                break;
            case 8:
                redoLastAction(root);
                break;
            case 9:
                cout << "Exiting system...\n";
                break;
            default:
                cout << "Invalid choice! Please try again.\n";
        }
    } while (choice != 9);

    return 0;
}