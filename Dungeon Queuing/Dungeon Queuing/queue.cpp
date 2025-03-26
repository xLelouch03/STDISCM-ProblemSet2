// Hans Martin F. Rejano        STDISCM S14
#include <iostream>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <random>
#include <chrono>
#include <limits>
#include <sstream>

using namespace std;

struct Party {
    int id;        // party ID
    int duration;  // dungeon duration in seconds
};

mutex mtx;
condition_variable cv;
queue<Party> partyQueue;
vector<bool> isInstanceAvailable;
bool isDone = false;
unsigned int total_parties_served = 0;
unsigned int total_time_served = 0;
vector<unsigned int> num_parties_served;
vector<unsigned int> instance_time_served;
vector<string> instance_status;

unsigned int getValidInput(const string& prompt, unsigned int minValue = 0,
    unsigned int maxValue = numeric_limits<unsigned int>::max()) {
    string input;
    int tempValue;

    while (true) {
        cout << prompt;
        cin >> input;

        // check if the entire input is a valid integer
        stringstream ss(input);
        if (!(ss >> tempValue) || !(ss.eof())) {
            cout << "Invalid input! Please enter a valid integer number.\n\n";
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            continue;
        }

        // ensure the number is within valid range
        if (tempValue < 0 || (unsigned int)tempValue < minValue || (unsigned int)tempValue > maxValue) {
            cout << "Invalid input! Please enter a number between " << minValue << " and " << maxValue << ".\n\n";
            continue;
        }

        return (unsigned int)tempValue;
    }
}

// handler to manage dungeon execution
void instanceHandler(int id) {
    while (true) {
        unique_lock<mutex> lock(mtx); // lock mutex to access shared resources safely
        cv.wait(lock, [id] { return !partyQueue.empty() || isDone; }); // wait until there is a party or termination signal
        if (isDone && partyQueue.empty()) break;

        // Retrieve party from queue
        Party p = partyQueue.front();
        partyQueue.pop();
        isInstanceAvailable[id - 1] = false;
        instance_status[id - 1] = "active";
        num_parties_served[id - 1]++;
        instance_time_served[id - 1] += p.duration;

        cout << "Instance " << id << " active with Party " << p.id << " for " << p.duration << " seconds." << endl;
        lock.unlock(); // unlock before sleeping

        this_thread::sleep_for(chrono::seconds(p.duration));

        lock.lock(); // lock to update shared resources
        instance_status[id - 1] = "empty";
        isInstanceAvailable[id - 1] = true;
        cout << "Instance " << id << " is now empty." << endl;
        lock.unlock(); // unlock after updates
    }
}

// dispatcher to manage party assignments
void dispatcher(int n) {
    while (true) {
        unique_lock<mutex> lock(mtx);
        if (isDone && partyQueue.empty()) break;

        for (int i = 0; i < n; i++) {
            if (!partyQueue.empty() && isInstanceAvailable[i]) {
                cv.notify_one(); // notify an instance to process a party
            }
        }
    }
}

int main() {
    unsigned int n, t, h, d, t1, t2;

    n = getValidInput("Enter number of instances: ", 1);
    t = getValidInput("Enter number of tanks: ", 1);
    h = getValidInput("Enter number of healers: ", 1);

    // ensure DPS count is at least 3 for a full party
    d = getValidInput("Enter number of DPS: ", 1);

    do {
        t1 = getValidInput("Enter min dungeon time: ", 1);
        t2 = getValidInput("Enter max dungeon time: ", t1); // ensure t2 >= t1
        if (t1 > t2) {
            cout << "Minimum time cannot be greater than maximum time. Try again." << endl;
        }
    } while (t1 > t2);

    cout << "\n";

    // determine the maximum number of parties that can be formed
    unsigned int max_parties = min({ t, h, d / 3 });
    unsigned int unmatched_tanks = t - max_parties;
    unsigned int unmatched_healers = h - max_parties;
    unsigned int unmatched_dps = d - (max_parties * 3);

    // random duration generator for dungeon runs
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<int> dis(t1, t2);

    // populate the party queue with generated parties
    for (int i = 0; i < max_parties; i++) {
        partyQueue.push({ i + 1, dis(gen) });
    }

    // initialize instance status tracking
    instance_status.resize(n, "empty");
    num_parties_served.resize(n, 0);
    instance_time_served.resize(n, 0);
    isInstanceAvailable.resize(n, true);

    cout << "Initial Instance Status:\n";
    for (unsigned int i = 0; i < n; i++) {
        cout << "Instance " << (i + 1) << " is empty." << endl;
    }
    cout << endl;

    // launch instance threads
    vector<thread> instances;
    for (unsigned int i = 0; i < n; i++) {
        instances.emplace_back(instanceHandler, i + 1);
    }

    // launch dispatcher thread
    thread dispatcherThread(dispatcher, n);

    // allow some processing time before stopping
    this_thread::sleep_for(chrono::seconds(1));
    isDone = true;
    cv.notify_all(); // notify all instances to exit

    // wait for all threads to finish
    for (auto& th : instances) {
        th.join();
    }
    dispatcherThread.join();

    // calculate total parties served and total time served
    for (unsigned int i = 0; i < n; i++) {
        total_parties_served += num_parties_served[i];
        total_time_served += instance_time_served[i];
    }

    // display final statistics
    cout << "\nFinal Statistics of the Dungeon Queuing:\n";
    for (unsigned int i = 0; i < n; i++) {
        cout << "Instance " << (i + 1) << " served " << num_parties_served[i]
            << " parties, total time: " << instance_time_served[i] << " seconds." << endl;
    }

    cout << "Total parties served: " << total_parties_served << endl;
    cout << "Total time served: " << total_time_served << " seconds" << endl;

    // display unmatched players
    cout << "\nUnmatched Players:" << endl;
    cout << "Unmatched Tanks: " << unmatched_tanks << endl;
    cout << "Unmatched Healers: " << unmatched_healers << endl;
    cout << "Unmatched DPS: " << unmatched_dps << endl;

    return 0;
}