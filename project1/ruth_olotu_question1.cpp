/*
Event Management System
Author Notes:
- The displayEventSchedule function's sorting implementation was adapted from a Claude AI example
  that helped me understand how to use std::sort with lambda functions to reduce code redundancy
- Shout out to the C++ community on stackoverflow, even though they insulted me, they still helped with my bst implementation
*/

#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <functional>
#include <stack>
#include <algorithm>
#include <vector>
#include <queue>
#include <chrono>
#include <ctime>
using namespace std;

//please note, majority of the syntax (not logic) for the the undo and redo stack operation was by claude ai, with minimal modififcations from my side.
struct Attendee {
    string fullName;      
    string phoneNumber;  
    Attendee* nextAttendee; 
    
   
    Attendee(string name, string phone) : fullName(name), phoneNumber(phone), nextAttendee(nullptr) {}
};

struct CheckIn {
    int eventId;
    string attendeeName;
    string timestamp;
    
    CheckIn(int id, string name, string time) 
        : eventId(id), attendeeName(name), timestamp(time) {}
};

struct EventNode {
    int eventId;             
    string eventName;         
    string eventType;     
    int importanceLevel;  //1-3 ;for 1-high, 2-medium, 3-low
    
    
    Attendee* attendeeList;
    Attendee* lastAttendee;
    
    
    EventNode* leftChild;         
    EventNode* rightChild;     

    EventNode(int id, string name, string type, int importance) 
        : eventId(id), eventName(name), eventType(type), importanceLevel(importance),
          attendeeList(nullptr), lastAttendee(nullptr), leftChild(nullptr), rightChild(nullptr) {}
};

class Command {
public:
//This abstract base class defines a contract that all commands must follow. 
//It's a pure virtual class, so all commands (functions) 
//in the system must be able to execute and undo itself.
    virtual void execute() = 0;
    virtual void undo() = 0;
    virtual ~Command() {}
};

// Command for updating event details
class UpdateEventCommand : public Command {
private:
    EventNode* event;
    string oldName, newName;
    string oldType, newType;
    int oldImportance, newImportance;
    bool nameChanged, typeChanged, importanceChanged;

public:
    UpdateEventCommand(EventNode* evt, 
                      const string& nName, const string& nType, int nImportance)
        : event(evt), 
          oldName(evt->eventName), newName(nName),
          oldType(evt->eventType), newType(nType),
          oldImportance(evt->importanceLevel), newImportance(nImportance),
          nameChanged(!nName.empty()),
          typeChanged(!nType.empty()),
          importanceChanged(nImportance >= 1 && nImportance <= 3) {}

    void execute() override {
        if (nameChanged) event->eventName = newName;
        if (typeChanged) event->eventType = newType;
        if (importanceChanged) event->importanceLevel = newImportance;
    }

    void undo() override {
        if (nameChanged) event->eventName = oldName;
        if (typeChanged) event->eventType = oldType;
        if (importanceChanged) event->importanceLevel = oldImportance;
    }
};

// Command for adding attendee
class AddAttendeeCommand : public Command {
private:
    EventNode* event;
    Attendee* attendee;
    bool isExecuted;

public:
    AddAttendeeCommand(EventNode* evt, const string& name, const string& phone) 
        : event(evt), isExecuted(false) {
        attendee = new Attendee(name, phone);
    }

//The destructor ensures proper cleanup of the attendee object if the command was never executed.
    ~AddAttendeeCommand() {
        if (!isExecuted && attendee) {
            delete attendee;
        }
    }

    void execute() override {
        if (!event->attendeeList) {
            event->attendeeList = event->lastAttendee = attendee;
        } else {
            event->lastAttendee->nextAttendee = attendee;
            event->lastAttendee = attendee;
        }
        isExecuted = true;
    }

    void undo() override {
        if (!event->attendeeList) return;

        if (event->attendeeList == attendee) {
            event->attendeeList = event->lastAttendee = nullptr;
        } else {
            Attendee* current = event->attendeeList;
            while (current->nextAttendee != attendee) {
                current = current->nextAttendee;
            }
            current->nextAttendee = nullptr;
            event->lastAttendee = current;
        }
        isExecuted = false;
    }
};

// Command manager to handle undo/redo operations
//The CommandManager is like a history keeper. It maintains two stacks:
//The undo stack records all commands as they're executed
//The redo stack stores commands that were undone

class CommandManager {
private:
    stack<Command*> undoStack;
    stack<Command*> redoStack;

public:
    ~CommandManager() {
        while (!undoStack.empty()) {
            delete undoStack.top();
            undoStack.pop();
        }
        while (!redoStack.empty()) {
            delete redoStack.top();
            redoStack.pop();
        }
    }

    void executeCommand(Command* command) {
        command->execute();
        undoStack.push(command);
        
        // Clear redo stack
        while (!redoStack.empty()) {
            delete redoStack.top();
            redoStack.pop();
        }
    }

    bool canUndo() { return !undoStack.empty(); }
    bool canRedo() { return !redoStack.empty(); }

    void undo() {
        if (!canUndo()) {
            cout << "Nothing to undo!" << endl;
            return;
        }

        Command* command = undoStack.top();
        undoStack.pop();
        command->undo();
        redoStack.push(command);
    }

    void redo() {
        if (!canRedo()) {
            cout << "Nothing to redo!" << endl;
            return;
        }

        Command* command = redoStack.top();
        redoStack.pop();
        command->execute();
        undoStack.push(command);
    }
};

// Global command manager instance
CommandManager commandManager;

queue<CheckIn> checkInQueue;
priority_queue<pair<int, EventNode*>> eventPriorityQueue;


void saveEventToFile(ofstream &outFile, EventNode* root) {
    if (root != nullptr) {
        outFile << root->eventId << ", " << root->eventName << ", " 
                << root->eventType << ", " << root->importanceLevel << "\n";
        saveEventToFile(outFile, root->leftChild);
        saveEventToFile(outFile, root->rightChild);
    }
}

// Saves all our events to disk for data persisitence
void saveAllEvents(EventNode* seminars, EventNode* sports, EventNode* competitions, EventNode* others) {
    ofstream outFile("events.txt", ios::trunc); 
    if (!outFile.is_open()) {
        cout << "Oops! Couldn't open the events file for saving. Check permissions." << endl;
        return;
    }
    saveEventToFile(outFile, seminars);
    saveEventToFile(outFile, sports);
    saveEventToFile(outFile, competitions);
    saveEventToFile(outFile, others);
    outFile.close();
}


//loads all prewritten data when the code is actually running, and inserts each node into a single BST
EventNode* loadEventFromFile(ifstream &inFile) {
    string line;
    EventNode* root = nullptr;

    while (getline(inFile, line)) {
        stringstream ss(line);
        int eventId, importanceLevel;
        string eventName, eventType;

        
        getline(ss, eventName, ',');
        getline(ss, eventType, ',');
        ss >> importanceLevel;
        ss>> eventId;

        EventNode* newEvent = new EventNode(eventId, eventName, eventType, importanceLevel);
        insertEvent(root, newEvent); 
    }

    return root;
}

//this then breaks down the single BST into the 4 individual BSTs for each event category
void loadAllEvents(EventNode*& seminars, EventNode*& sports, EventNode*& competitions, EventNode*& others) {
    ifstream inFile("events.txt");
    if (!inFile.is_open()) {
        cout << "Couldn't open the events file. Starting with an empty database." << endl;
        return;
    }

    // Load events into appropriate trees
    seminars = loadEventFromFile(inFile);
    sports = loadEventFromFile(inFile);
    competitions = loadEventFromFile(inFile);
    others = loadEventFromFile(inFile);

    inFile.close();
}

// Saves participant info to keep track of who's coming!
void saveAttendeeInfo(EventNode* seminars, EventNode* sports, EventNode* competitions, EventNode* others) {
    ofstream outFile("attendees.txt", ios::trunc);
    if (!outFile) {
        cout << "Hey, couldn't open the attendees file. Something's not right." << endl;
        return;
    }

    function<void(EventNode*)> saveEventAttendees = [&outFile, &saveEventAttendees](EventNode* event) {
        if (!event) return;

        saveEventAttendees(event->leftChild);

        // Write event details first
        outFile << event->eventId << "," << event->eventType << "," << event->eventName << "\n";

        // Then write all attendee info
        Attendee* current = event->attendeeList;
        while (current) {
            outFile << current->fullName << "," << current->phoneNumber << "\n";
            current = current->nextAttendee;
        }
        outFile << "#\n"; // Our trusty event separator

        saveEventAttendees(event->rightChild);
    };

    saveEventAttendees(seminars);
    saveEventAttendees(sports);
    saveEventAttendees(competitions);
    saveEventAttendees(others);
    outFile.close();
}

//loads all prewritten data when the code is actually running
void loadAttendeeInfo(EventNode* seminars, EventNode* sports, EventNode* competitions, EventNode* others) {
    ifstream inFile("attendees.txt");
    if (!inFile.is_open()) {
        cout << "Couldn't open the attendees file. No attendee data loaded." << endl;
        return;
    }

    string line;
    while (getline(inFile, line)) {
        if (line == "#") continue; // Skip event separator

        stringstream ss(line);
        int eventId;
        string eventType, eventName;

        // Read event details
        ss >> eventId >> eventType >> eventName;

        EventNode* event = nullptr;

        // Find the corresponding event in the correct tree
        if (eventType == "Seminar") event = findEvent(seminars, eventId);
        else if (eventType == "Sports") event = findEvent(sports, eventId);
        else if (eventType == "Competition") event = findEvent(competitions, eventId);
        else if (eventType == "Others") event = findEvent(others, eventId);

        if (!event) continue; // Skip if event is not found

        // Read attendees for this event
        while (getline(inFile, line) && line != "#") {
            string attendeeName, phoneNumber;
            stringstream attendeeStream(line);
            getline(attendeeStream, attendeeName, ',');
            getline(attendeeStream, phoneNumber, ',');

            // Add attendee to the event
            Attendee* newAttendee = new Attendee(attendeeName, phoneNumber);
            newAttendee->nextAttendee = event->attendeeList;
            event->attendeeList = newAttendee;
        }
    }

    inFile.close();
}


EventNode* findMinNode(EventNode* node) {
    while (node && node->leftChild) {
        node = node->leftChild;
    }
    return node;
}

// Basic search function with some assistance from chatgpt
EventNode* findEvent(EventNode* root, int targetId) {
    while (root && root->eventId != targetId) {
        root = (targetId < root->eventId) ? root->leftChild : root->rightChild;
    }
    return root; // Either the found node or nullptr
}

/*I hardcoded event types, because ideally, the application would have a 
dropdown box of all the type of events available to register, there is the "Other"
available sha, just in case, but i cannot kill myself.

*/
void insertEvent(EventNode*& root, EventNode* newEvent) { 
    if (!root) {
        root = newEvent;
        return;
    }
    if (newEvent->eventId < root->eventId) {
        insertEvent(root->leftChild, newEvent);
    } else {
        insertEvent(root->rightChild, newEvent);
    }

    
}

// chatgpt helped me here to ensure that even when i was removing events, my BST would still be balanced
EventNode* removeEvent(EventNode* root, int targetId) {
    if (!root) {
        cout << "Hmm, couldn't find that event. Maybe it was already deleted?" << endl;
        return nullptr;
    }

    if (targetId < root->eventId) {
        root->leftChild = removeEvent(root->leftChild, targetId);
    } else if (targetId > root->eventId) {
        root->rightChild = removeEvent(root->rightChild, targetId);
    } else {
        // Case 1: Leaf node 
        if (!root->leftChild && !root->rightChild) {
            delete root;
            return nullptr;
        }
        
        // Case 2: One child 
        if (!root->leftChild) {
            EventNode* temp = root->rightChild;
            delete root;
            return temp;
        }
        if (!root->rightChild) {
            EventNode* temp = root->leftChild;
            delete root;
            return temp;
        }
        
        // Case 3: Two children - chatgpt helped here, deleting a parent node without affecting the children node
        EventNode* successor = findMinNode(root->rightChild);
        root->eventId = successor->eventId;
        root->eventName = successor->eventName;
        root->eventType = successor->eventType;
        root->importanceLevel = successor->importanceLevel;
        root->rightChild = removeEvent(root->rightChild, successor->eventId);
    }
    return root;
}

// Display Functions 

void showEventDetails(EventNode* event) {
    cout << "\nEvent Details:" << endl;
    cout << "ID: " << event->eventId << endl;
    cout << "Name: " << event->eventName << endl;
    cout << "Type: " << event->eventType << endl;
    cout << "Importance Level: " << event->importanceLevel << endl;
    
    if (!event->attendeeList) {
        cout << "No attendees registered yet.\n";
    } else {
        cout << "\nAttendees:" << endl;
        Attendee* current = event->attendeeList;
        while (current) {
            cout << "- " << current->fullName << " (" << current->phoneNumber << ")\n";
            current = current->nextAttendee;
        }
    }
    cout << endl;
}

// Displays all events in a nice organized way
void showAllEvents(EventNode* root) {
    if (root) {
        showAllEvents(root->leftChild);
        showEventDetails(root);
        showAllEvents(root->rightChild);
    }
}

// This is where Claude's help came in handy - helped me sort events by importance
void displaySchedule(EventNode* seminars, EventNode* sports, EventNode* competitions, EventNode* others) {
    vector<EventNode*> allEvents;
    
    // Helper lambda to collect all events, this allows us modify variables form outside functions (in this case allEvents)
    //gives permission tot the function to modify our table during in-order traversal
    auto collectEvents = [&](EventNode* node, auto& collect) -> void {
        if (node) {
            collect(node->leftChild, collect);
            allEvents.push_back(node);
            collect(node->rightChild, collect);
        }
    };

    // Gather all events from each category
    collectEvents(seminars, collectEvents);
    collectEvents(sports, collectEvents);
    collectEvents(competitions, collectEvents);
    collectEvents(others, collectEvents);

    if (allEvents.empty()) {
        cout << "No events scheduled yet!" << endl;
        return;
    }

    // Sort by importance level (high to low) and ID (for ties)
    sort(allEvents.begin(), allEvents.end(), [](EventNode* a, EventNode* b) {
        return a->importanceLevel != b->importanceLevel ? 
               a->importanceLevel > b->importanceLevel : 
               a->eventId < b->eventId;
    });

    cout << "\n=== Event Schedule ===" << endl;
    for (auto event : allEvents) {
        cout << "Priority " << event->importanceLevel << ": " 
             << event->eventName << " (" << event->eventType << ")" << endl;
    }
}

// ===== User Interface Functions =====

void createNewEvent(EventNode*& seminars, EventNode*& sports, EventNode*& competitions, EventNode*& others) {
    string name, type;
    int importance, id;

    cin.ignore();
    cout << "Event name: ";
    getline(cin, name);

    cout << "Event type (Seminar/Sports/Competition/Others): ";
    getline(cin, type);
    string tolower(type);
    transform(type.begin(), type.end(), type.begin(), ::tolower);

    cout << "Importance level (1-3): ";
    cin >> importance;

    // Make sure we don't duplicate IDs
    bool uniqueId = false;
    while (!uniqueId) {
        cout << "Event ID: ";
        cin >> id;
        
        if (!findEvent(seminars, id) && !findEvent(sports, id) && !findEvent(competitions, id) && !findEvent(others, id)) {
            uniqueId = true;
        } else {
            cout << "That ID's taken! Try another one." << endl;
        }
    }

    EventNode* newEvent = new EventNode(id, name, type, importance);

    if (type == "seminar") {
        insertEvent(seminars, newEvent);
    } else if (type == "others"){
      insertEvent(others, newEvent);
    } else if (type == "sports") {
        insertEvent(sports, newEvent);
    } else if (type == "competition") {
        insertEvent(competitions, newEvent);
    } else {
        cout << "Oops! That's not a valid event type." << endl;
        delete newEvent;
        return;
    }

    cout << "Event created successfully!" << endl;
}

void updateEventInfo(EventNode*& seminars, EventNode*& sports, EventNode*& competitions, EventNode*& others) {
    int id;
    cout << "Enter the Event ID to update: ";
    cin >> id;

    // Search for the event in all categories (keep your existing search code)
    EventNode* eventToUpdate = findEvent(seminars, id);
    if (!eventToUpdate) eventToUpdate = findEvent(sports, id);
    if (!eventToUpdate) eventToUpdate = findEvent(competitions, id);
    if (!eventToUpdate) eventToUpdate = findEvent(others, id);

    if (!eventToUpdate) {
        cout << "Event not found!" << endl;
        return;
    }

    // Display current details (keep your existing display code)
    cout << "Event found!" << endl;
    cout << "Current details:" << endl;
    // ... (rest of your display code)

    cin.ignore();
    string name, type;
    int importance;

    // Get new details (keep your existing input code)
    cout << "Enter new name (or press Enter to keep the current name): ";
    getline(cin, name);

    cout << "Enter new type (Seminar/Sports/Competition/Others or press Enter to keep current type): ";
    getline(cin, type);
    transform(type.begin(), type.end(), type.begin(), ::tolower);

    cout << "Enter new importance level (1-3 or press 0 to keep current level): ";
    cin >> importance;

    // Create and execute update command
    Command* updateCmd = new UpdateEventCommand(eventToUpdate, name, type, importance);
    commandManager.executeCommand(updateCmd);

    cout << "Event updated successfully!" << endl;
    
    // Save changes
    saveAllEvents(seminars, sports, competitions, others);
}


void registerNewAttendee(EventNode*& seminars, EventNode*& sports, EventNode*& competitions, EventNode*& others) {
 
    string type;
    int id;
  

    EventNode* event = nullptr;
    if (type == "seminar") event = findEvent(seminars, id);
    else if (type == "sports") event = findEvent(sports, id);
    else if (type == "competition") event = findEvent(competitions, id);
    else if (type == "others") event = findEvent(others, id);

    if (!event) {
        cout << "Couldn't find that event. Double-check the type and ID?" << endl;
        return;
    }

    cout << "Found event: " << event->eventName << endl;

    char addMore;
    do {
        string name, phone;
        cin.ignore();

        cout << "Attendee name: ";
        getline(cin, name);

        cout << "Phone number: ";
        getline(cin, phone);

        Command* registerCmd = new AddAttendeeCommand(event, name, phone);
        commandManager.executeCommand(registerCmd);

        saveAttendeeInfo(seminars, sports, competitions, others);
        cout << "Attendee registered successfully!" << endl;

        cout << "Register another? (y/n): ";
        cin >> addMore;
    } while (addMore == 'y' || addMore == 'Y');
}


void undoLastOperation(EventNode*& seminars, EventNode*& sports, EventNode*& competitions, EventNode*& others) {
    commandManager.undo();
    saveAllEvents(seminars, sports, competitions, others);
    saveAttendeeInfo(seminars, sports, competitions, others);
}

void redoLastOperation(EventNode*& seminars, EventNode*& sports, EventNode*& competitions, EventNode*& others) {
    commandManager.redo();
    saveAllEvents(seminars, sports, competitions, others);
    saveAttendeeInfo(seminars, sports, competitions, others);
}

void processCheckIn(EventNode* seminars, EventNode* sports, EventNode* competitions, EventNode* others) {
    int eventId;
    string attendeeName;
    
    cout << "Enter Event ID: ";
    cin >> eventId;
    cin.ignore();
    cout << "Enter Attendee Name: ";
    getline(cin, attendeeName);
    
    // Get current timestamp, got this ENTIRELY from claudeAI
    auto now = chrono::system_clock::now();
    time_t currentTime = chrono::system_clock::to_time_t(now);
    string timestamp = ctime(&currentTime);
    timestamp = timestamp.substr(0, timestamp.length() - 1);  // Remove newline
    
    checkInQueue.push(CheckIn(eventId, attendeeName, timestamp));
    cout << "Check-in queued successfully!\n";
}

//actually checks people in into the event
void processNextCheckIn() {
    if (checkInQueue.empty()) {
        cout << "No one in the check-in queue.\n";
        return;
    }
    
    CheckIn next = checkInQueue.front();
    checkInQueue.pop();
    
    cout << "Processing check-in for:\n"
         << "Attendee: " << next.attendeeName << "\n"
         << "Event ID: " << next.eventId << "\n"
         << "Check-in Time: " << next.timestamp << "\n";
}

// View next person in line
void viewNextInLine() {
    if (checkInQueue.empty()) {
        cout << "No one in the check-in queue.\n";
        return;
    }
    
    CheckIn next = checkInQueue.front();
    cout << "Next in line:\n"
         << "Attendee: " << next.attendeeName << "\n"
         << "Event ID: " << next.eventId << "\n";
}



// Update priority queue with all events 
void updatePriorityQueue(EventNode* root) {
    if (!root) return;
    
    updatePriorityQueue(root->leftChild);
    // Priority is negative of importance level so lower numbers have higher priority
    eventPriorityQueue.push({-root->importanceLevel, root});
    updatePriorityQueue(root->rightChild);
}

// Generate comprehensive report
void generateReport(EventNode* seminars, EventNode* sports, EventNode* competitions, EventNode* others) {
    cout << "\n=== EVENT MANAGEMENT SYSTEM REPORT ===\n\n";
    
    // Events and Participants
    cout << "=== EVENTS AND PARTICIPANTS ===\n";
    cout << "\nSEMINARS:\n";
    showAllEvents(seminars);
    cout << "\nSPORTS:\n";
    showAllEvents(sports);
    cout << "\nCOMPETITIONS:\n";
    showAllEvents(competitions);
    cout << "\nOTHERS:\n";
    showAllEvents(others);
    
    // Check-in Statistics
    cout << "\n=== CHECK-IN STATISTICS ===\n";
    cout << "Current Queue Length: " << checkInQueue.size() << "\n";
    
    // Priority Schedule
    cout << "\n=== PRIORITY SCHEDULE ===\n";
    // Clear existing priority queue
    while (!eventPriorityQueue.empty()) eventPriorityQueue.pop();
    
    // Update priority queue with all events
    updatePriorityQueue(seminars);
    updatePriorityQueue(sports);
    updatePriorityQueue(competitions);
    updatePriorityQueue(others);
    
    // Display events in priority order
    while (!eventPriorityQueue.empty()) {
        EventNode* event = eventPriorityQueue.top().second;
        cout << "Priority Level " << -eventPriorityQueue.top().first << ": "
             << event->eventName << " (ID: " << event->eventId << ")\n";
        eventPriorityQueue.pop();
    }
}


// The main function 
int main() {
    // Our four BSTs - one for each event type
    EventNode *seminars = nullptr, *sports = nullptr, *competitions = nullptr, *others = nullptr;

    char keepGoing;
    do {
        cout << "\n=== Event Management System ===\n"
             << "1. Create New Event\n"
             << "2. View Event Details\n"
             << "3. Register Attendee\n"
             << "4. View Schedule\n"
             << "5. Remove Event\n"
             << "6. Update Event\n"
             << "7. Add Attendee to Queue\n"
             << "8. Process Next in Queue\n"
             << "9. View Next in Line\n"
             << "10. Generate Report\n"
             << "11. Undo Last Operation\n"  
             << "12. Redo Last Operation\n"  
             << "13. Exit.\n"

             << "Choose an option: ";
             
        int choice;
        cin >> choice;

        switch (choice) {
            case 1:
                createNewEvent(seminars, sports, competitions, others);
                saveAllEvents(seminars, sports, competitions, others);
                break;
            case 2: {
                string type;
                int id;
                cin.ignore();
                cout << "Event type: ";
                getline(cin, type);
                transform(type.begin(), type.end(), type.begin(), ::tolower);
                cout << "Event ID: ";
                cin >> id;
                
                EventNode* event = nullptr;
                if (type == "seminar") event = findEvent(seminars, id);
                else if (type == "sports") event = findEvent(sports, id);
                else if (type == "competition") event = findEvent(competitions, id);
                else if (type == "others") event = findEvent(others, id);
                
                if (event) showEventDetails(event);
                else cout << "Event not found!\n";
                break;
            }
            case 3:
                registerNewAttendee(seminars, sports, competitions, others);
                break;
            case 4:
                displaySchedule(seminars, sports, competitions, others);
                break;
            case 5: {
                string type;
                int id;
                cin.ignore();
                cout << "Event type to remove: ";
                getline(cin, type);
                transform(type.begin(), type.end(), type.begin(), ::tolower);
                cout << "Event ID: ";
                cin >> id;
                
                if (type == "seminar") seminars = removeEvent(seminars, id);
                else if (type == "sports") sports = removeEvent(sports, id);
                else if (type == "sompetition") competitions = removeEvent(competitions, id);
                else if (type == "others") competitions = removeEvent(others, id);
                else cout << "Invalid event type!\n";
                
                saveAllEvents(seminars, sports, competitions, others);
                saveAttendeeInfo(seminars, sports, competitions, others);
                break;
            }

            case 6: {
              updateEventInfo(seminars, sports, competitions, others);
              saveAllEvents(seminars, sports, competitions, others);
              saveAttendeeInfo(seminars, sports, competitions, others);
              break;
            }
            case 7:
            processCheckIn(seminars, sports, competitions, others);
            break;
            case 8:
            processNextCheckIn();
            break;
            case 9:
            viewNextInLine();
            break;
            case 10:
            generateReport(seminars, sports, competitions, others);
            break;
            case 11:
            undoLastOperation(seminars, sports, competitions, others);
            break;
            case 12:
            redoLastOperation(seminars, sports, competitions, others);
            break;
            case 13:
                cout << "Thanks for using the system! Goodbye!\n";
                return 0;
            default:
                cout << "Invalid choice. Try again!\n";
        }

        cout << "\nAnything else? (y/n): ";
        cin >> keepGoing;
    } while (keepGoing == 'y' || keepGoing == 'Y');

    return 0;
}