#include <iostream>
#include <string>
#include <vector>
#include <queue>
#include <map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <random>
#include <atomic>
#include <memory>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>

// Add global mutex before class declarations
std::mutex g_console_mutex;

// Forward declarations
class Flight;
class Runway;
class AVN;

// Enum for Aircraft Types
enum class AircraftType {
    Commercial,
    Cargo,
    Emergency
};

// Enum for Emergency subtypes
enum class EmergencyType {
    None,
    Military,
    Medical,
    DiversionOrLowFuel,
    VIP
};

// Enum for Flight Direction
enum class FlightDirection {
    NorthArrival,
    SouthArrival,
    EastDeparture,
    WestDeparture
};

// Enum for Flight Phase
enum class FlightPhase {
    Holding,
    Approach,
    Landing,
    Taxi,
    AtGate,
    TakeoffRoll,
    Climb,
    Cruise,
    Departure
};

// Airline class
class Airline {
public:
    std::string name;
    AircraftType type;
    int totalAircrafts;
    int flightsInOperation;

    Airline(const std::string& n, AircraftType t, int total, int flights)
        : name(n), type(t), totalAircrafts(total), flightsInOperation(flights) {}
};

// Flight class
class Flight {
public:
    int flightNumber;
    Airline* airline;
    AircraftType aircraftType;
    FlightDirection direction;
    FlightPhase phase;
    float speed; // km/h
    bool violationActive;
    std::string violationReason;
    std::chrono::system_clock::time_point scheduledTime;
    std::chrono::system_clock::time_point actualTime;
    int runwayAssigned; // -1 if none
    bool runwayOccupied;
    EmergencyType emergencyType;
    int priorityLevel; // 1-4, with 1 being highest
    bool hasFault;
    std::string faultDescription;
    int estimatedWaitTime;
    std::vector<int> avnIDs;  // Track AVN IDs for this flight

    Flight(int num, Airline* al, AircraftType at, FlightDirection dir, 
           std::chrono::system_clock::time_point sched, EmergencyType emType = EmergencyType::None)
        : flightNumber(num), airline(al), aircraftType(at), direction(dir), phase(FlightPhase::Holding),
          speed(0.0f), violationActive(false), runwayAssigned(-1), runwayOccupied(false),
          emergencyType(emType), priorityLevel(calculatePriority()), hasFault(false), estimatedWaitTime(0)
    {
        scheduledTime = sched;
        actualTime = sched;
    }

    void updatePhase(FlightPhase newPhase) {
        phase = newPhase;
    }

    void updateSpeed(float newSpeed) {
        speed = newSpeed;
    }

    int calculatePriority() {
        if (aircraftType == AircraftType::Emergency) {
            return 1; // Top priority
        } else if (emergencyType == EmergencyType::VIP) {
            return 2; // VIP priority
        } else if (aircraftType == AircraftType::Cargo) {
            return 3; // Cargo priority
        } else {
            return 4; // Standard commercial
        }
    }
    
    std::string getPhaseString() const {
        switch (phase) {
            case FlightPhase::Holding: return "Holding";
            case FlightPhase::Approach: return "Approach";
            case FlightPhase::Landing: return "Landing";
            case FlightPhase::Taxi: return "Taxi";
            case FlightPhase::AtGate: return "AtGate";
            case FlightPhase::TakeoffRoll: return "TakeoffRoll";
            case FlightPhase::Climb: return "Climb";
            case FlightPhase::Cruise: return "Cruise";
            case FlightPhase::Departure: return "Departure";
            default: return "Unknown";
        }
    }
    
    std::string getDirectionString() const {
        switch (direction) {
            case FlightDirection::NorthArrival: return "North Arrival";
            case FlightDirection::SouthArrival: return "South Arrival";
            case FlightDirection::EastDeparture: return "East Departure";
            case FlightDirection::WestDeparture: return "West Departure";
            default: return "Unknown";
        }
    }
    
    std::string getAircraftTypeString() const {
        switch (aircraftType) {
            case AircraftType::Commercial: return "Commercial";
            case AircraftType::Cargo: return "Cargo";
            case AircraftType::Emergency: return "Emergency";
            default: return "Unknown";
        }
    }
    
    std::string getEmergencyTypeString() const {
        switch (emergencyType) {
            case EmergencyType::None: return "None";
            case EmergencyType::Military: return "Military";
            case EmergencyType::Medical: return "Medical";
            case EmergencyType::DiversionOrLowFuel: return "Diversion/Low Fuel";
            case EmergencyType::VIP: return "VIP";
            default: return "Unknown";
        }
    }
};

// Runway class
class Runway {
public:
    int id;
    std::string name;
    std::mutex runwayMutex;
    std::atomic<bool> occupied;

    Runway(int i, const std::string& n) : id(i), name(n), occupied(false) {}

    bool tryAcquire() {
        if (runwayMutex.try_lock()) {
            if (!occupied.load()) {
                occupied.store(true);
                return true;
            }
            runwayMutex.unlock();
        }
        return false;
    }

    void release() {
        occupied.store(false);
        runwayMutex.unlock();
    }
};

// AVN (Airspace Violation Notice) class
class AVN {
public:
    int avnID;
    Flight* flight;
    std::string airlineName;
    int flightNumber;
    AircraftType aircraftType;
    float recordedSpeed;
    float permissibleSpeed;
    std::chrono::system_clock::time_point issueDateTime;
    double fineAmount;
    bool paymentStatus; // false = unpaid, true = paid
    std::chrono::system_clock::time_point dueDate;

    AVN(int id, Flight* f, float recSpeed, float permSpeed, double fine)
        : avnID(id), flight(f), airlineName(f->airline->name), flightNumber(f->flightNumber),
          aircraftType(f->aircraftType), recordedSpeed(recSpeed), permissibleSpeed(permSpeed),
          fineAmount(fine), paymentStatus(false)
    {
        issueDateTime = std::chrono::system_clock::now();
        dueDate = issueDateTime + std::chrono::hours(24 * 3); // 3 days from issuance
    }
    
    std::string getFormattedDateTime(const std::chrono::system_clock::time_point& tp) const {
        auto t = std::chrono::system_clock::to_time_t(tp);
        std::tm tm = *std::localtime(&t);
        std::stringstream ss;
        ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }
    
    void printDetails() const {
        std::cout << "AVN #" << avnID << ":" << std::endl;
        std::cout << "  Airline: " << airlineName << std::endl;
        std::cout << "  Flight Number: " << flightNumber << std::endl;
        std::cout << "  Aircraft Type: ";
        switch (aircraftType) {
            case AircraftType::Commercial: std::cout << "Commercial"; break;
            case AircraftType::Cargo: std::cout << "Cargo"; break;
            case AircraftType::Emergency: std::cout << "Emergency"; break;
        }
        std::cout << std::endl;
        std::cout << "  Recorded Speed: " << recordedSpeed << " km/h" << std::endl;
        std::cout << "  Permissible Speed: " << permissibleSpeed << " km/h" << std::endl;
        std::cout << "  Issue Date/Time: " << getFormattedDateTime(issueDateTime) << std::endl;
        std::cout << "  Fine Amount: PKR " << fineAmount << std::endl;
        std::cout << "  Payment Status: " << (paymentStatus ? "Paid" : "Unpaid") << std::endl;
        std::cout << "  Due Date: " << getFormattedDateTime(dueDate) << std::endl;
    }
};

// Shared memory structures for IPC
namespace bip = boost::interprocess;

struct SharedAVN {
    int avnID;
    char airlineName[50];
    int flightNumber;
    int aircraftType;
    float recordedSpeed;
    float permissibleSpeed;
    time_t issueDateTime;
    double fineAmount;
    bool paymentStatus;
    time_t dueDate;
};

typedef bip::allocator<SharedAVN, bip::managed_shared_memory::segment_manager> ShmemAllocator;
typedef bip::vector<SharedAVN, ShmemAllocator> SharedAVNVector;

// Add this before the AVNGenerator class definition
struct SharedCounters {
    std::atomic<int> avnCounter;
    SharedCounters() : avnCounter(0) {}
};

// Add before ATCSController class definition
struct SharedRunwayStatus {
    std::atomic<bool> runwayOccupied[3];  // For 3 runways
    SharedRunwayStatus() {
        for (int i = 0; i < 3; i++) {
            runwayOccupied[i] = false;
        }
    }
};

// AVN Generator class
class AVNGenerator {
private:
    std::mutex avnMutex;
    bip::managed_shared_memory segment;
    SharedAVNVector* avnVector;
    SharedCounters* sharedCounters;
    bip::named_mutex namedMutex;

public:
    AVNGenerator() : 
        segment(bip::open_or_create, "AVNSharedMemory", 65536),
        namedMutex(bip::open_or_create, "AVNMutex")
    {
        // Initialize shared memory for AVNs
        ShmemAllocator alloc_inst(segment.get_segment_manager());
        avnVector = segment.find_or_construct<SharedAVNVector>("AVNVector")(alloc_inst);
        sharedCounters = segment.find_or_construct<SharedCounters>("Counters")();
    }

    int generateAVN(Flight* flight, float permissibleSpeed) {
        std::lock_guard<std::mutex> lock(avnMutex);
        
        double baseAmount = 0.0;
        switch (flight->aircraftType) {
            case AircraftType::Commercial:
                baseAmount = 500000.0;
                break;
            case AircraftType::Cargo:
                baseAmount = 700000.0;
                break;
            case AircraftType::Emergency:
                baseAmount = 500000.0; // Same as commercial for emergency flights
                break;
        }
        
        // Add 15% service fee
        double totalAmount = baseAmount * 1.15;
        
        int currentAvnId = ++sharedCounters->avnCounter;
        AVN newAVN(currentAvnId, flight, flight->speed, permissibleSpeed, totalAmount);
        
        // Add to shared memory
        bip::scoped_lock<bip::named_mutex> lock_shared(namedMutex);
        SharedAVN sharedAVN;
        sharedAVN.avnID = newAVN.avnID;
        std::strncpy(sharedAVN.airlineName, newAVN.airlineName.c_str(), 49);
        sharedAVN.airlineName[49] = '\0';
        sharedAVN.flightNumber = newAVN.flightNumber;
        sharedAVN.aircraftType = static_cast<int>(newAVN.aircraftType);
        sharedAVN.recordedSpeed = newAVN.recordedSpeed;
        sharedAVN.permissibleSpeed = permissibleSpeed;
        sharedAVN.issueDateTime = std::chrono::system_clock::to_time_t(newAVN.issueDateTime);
        sharedAVN.fineAmount = newAVN.fineAmount;
        sharedAVN.paymentStatus = false;
        sharedAVN.dueDate = std::chrono::system_clock::to_time_t(newAVN.dueDate);
        
        avnVector->push_back(sharedAVN);
        
        std::cout << "AVN #" << currentAvnId << " generated for " << flight->airline->name 
                  << " flight #" << flight->flightNumber << std::endl;
        std::cout << "  Speed: " << flight->speed << " km/h (limit: " << permissibleSpeed << " km/h)" << std::endl;
        std::cout << "  Fine Amount: PKR " << totalAmount << " (including 15% service fee)" << std::endl;
        
        return currentAvnId;
    }
    
    void updatePaymentStatus(int avnID, bool paid) {
        bip::scoped_lock<bip::named_mutex> lock(namedMutex);
        
        for (auto& avn : *avnVector) {
            if (avn.avnID == avnID) {
                avn.paymentStatus = paid;
                std::cout << "AVN #" << avnID << " payment status updated to: " 
                          << (paid ? "PAID" : "UNPAID") << std::endl;
                return;
            }
        }
        
        std::cout << "AVN #" << avnID << " not found for payment update." << std::endl;
    }
};

// AirlinePortal class
class AirlinePortal {
private:
    std::string airlineName;
    bip::managed_shared_memory segment;
    SharedAVNVector* avnVector;
    bip::named_mutex namedMutex;

public:
    AirlinePortal(const std::string& name) : 
        airlineName(name),
        segment(bip::open_or_create, "AVNSharedMemory", 65536),
        namedMutex(bip::open_or_create, "AVNMutex")
    {
        avnVector = segment.find<SharedAVNVector>("AVNVector").first;
        if (!avnVector) {
            std::cerr << "Failed to find AVNVector in shared memory" << std::endl;
        }
    }
    
    void listActiveAVNs() {
        bip::scoped_lock<bip::named_mutex> lock(namedMutex);
        {
            std::lock_guard<std::mutex> consoleLock(g_console_mutex);
            std::cout << "Active AVNs for " << airlineName << ":" << std::endl;
        }
        
        bool found = false;
        if (avnVector) {
            for (const auto& avn : *avnVector) {
                if (strcmp(avn.airlineName, airlineName.c_str()) == 0 && !avn.paymentStatus) {
                    found = true;
                    std::lock_guard<std::mutex> consoleLock(g_console_mutex);
                    std::cout << "AVN #" << avn.avnID << ":" << std::endl;
                    std::cout << "  Flight Number: " << avn.flightNumber << std::endl;
                    std::cout << "  Recorded Speed: " << avn.recordedSpeed << " km/h" << std::endl;
                    std::cout << "  Permissible Speed: " << avn.permissibleSpeed << " km/h" << std::endl;
                    
                    auto issueDT = *std::localtime(&avn.issueDateTime);
                    auto dueDT = *std::localtime(&avn.dueDate);
                    
                    std::cout << "  Issue Date: " 
                              << std::put_time(&issueDT, "%Y-%m-%d %H:%M:%S") << std::endl;
                    std::cout << "  Due Date: " 
                              << std::put_time(&dueDT, "%Y-%m-%d %H:%M:%S") << std::endl;
                    std::cout << "  Fine Amount: PKR " << std::fixed 
                              << std::setprecision(2) << avn.fineAmount << std::endl;
                    std::cout << "  Status: UNPAID" << std::endl;
                    std::cout << "------------------------" << std::endl;
                }
            }
        }
        
        if (!found) {
            std::lock_guard<std::mutex> consoleLock(g_console_mutex);
            std::cout << "No active unpaid AVNs found for " << airlineName << std::endl;
        }
    }
    
    void payAVN(int avnID) {
        // This would interface with the StripePay process in a real implementation
        std::cout << "Initiating payment for AVN #" << avnID << "..." << std::endl;
        
        // Simulate payment process
        std::cout << "Processing payment..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        // Update payment status
        AVNGenerator avnGen;
        avnGen.updatePaymentStatus(avnID, true);
        
        std::cout << "Payment successful for AVN #" << avnID << std::endl;
    }
};

// StripePay class
class StripePay {
private:
    bip::managed_shared_memory segment;
    SharedAVNVector* avnVector;
    bip::named_mutex namedMutex;

public:
    StripePay() :
        segment(bip::open_or_create, "AVNSharedMemory", 65536),
        namedMutex(bip::open_or_create, "AVNMutex")
    {
        avnVector = segment.find<SharedAVNVector>("AVNVector").first;
        if (!avnVector) {
            std::cerr << "Failed to find AVNVector in shared memory" << std::endl;
        }
    }
    
    bool processPayment(int avnID, double amount) {
        bip::scoped_lock<bip::named_mutex> lock(namedMutex);
        
        std::cout << "\n╔════════════════════════════════════════╗\n";
        std::cout << "║          PAYMENT PROCESSING            ║\n";
        std::cout << "╠════════════════════════════════════════╣\n";
        
        for (const auto& avn : *avnVector) {
            if (avn.avnID == avnID) {
                std::cout << "║ AVN ID: #" << std::setw(4) << avn.avnID << "\n";
                std::cout << "║ Airline: " << avn.airlineName << "\n";
                std::cout << "║ Flight: #" << avn.flightNumber << "\n";
                std::cout << "║ Amount Due: PKR " << std::fixed << std::setprecision(2) 
                          << avn.fineAmount << "\n";
                std::cout << "║ Amount Paid: PKR " << amount << "\n";
                
                if (amount >= avn.fineAmount) {
                    if (amount > avn.fineAmount) {
                        std::cout << "║ Change: PKR " << (amount - avn.fineAmount) << "\n";
                    }
                    std::cout << "╟────────────────────────────────────────╢\n";
                    std::cout << "║          PAYMENT SUCCESSFUL            ║\n";
                    
                    AVNGenerator avnGen;
                    avnGen.updatePaymentStatus(avnID, true);
                    
                    std::cout << "╚════════════════════════════════════════╝\n";
                    return true;
                } else {
                    std::cout << "╟────────────────────────────────────────╢\n";
                    std::cout << "║          PAYMENT FAILED                ║\n";
                    std::cout << "║ Reason: Insufficient payment           ║\n";
                    std::cout << "║ Missing: PKR " << (avn.fineAmount - amount) << "\n";
                    std::cout << "╚════════════════════════════════════════╝\n";
                    return false;
                }
            }
        }
        
        std::cout << "╟────────────────────────────────────────╢\n";
        std::cout << "║          PAYMENT FAILED                ║\n";
        std::cout << "║ Reason: AVN #" << avnID << " not found          ║\n";
        std::cout << "╚════════════════════════════════════════╝\n";
        return false;
    }
};

// ATCS Controller class
class ATCSController {
private:
    std::vector<Airline> airlines;
    std::vector<std::unique_ptr<Runway>> runways;
    std::vector<std::unique_ptr<Flight>> flights;  // Changed to unique_ptr
    std::vector<AVN> avns;
    std::unique_ptr<AVNGenerator> avnGenerator;

    std::mutex flightsMutex;
    std::mutex avnMutex;
    std::atomic<bool> simulationRunning;
    std::chrono::steady_clock::time_point simulationStartTime;
    std::chrono::seconds simulationDuration;

    // Flight scheduling system
    struct FlightSchedule {
        FlightDirection direction;
        int intervalSeconds;
        float emergencyProbability;
        std::string description;
        EmergencyType emergencyType;
    };

    std::vector<FlightSchedule> flightSchedules;
    std::atomic<bool> flightGenerationRunning;

    // Priority queue for runway allocation
    struct FlightPriorityComparator {
        bool operator()(const Flight* a, const Flight* b) {
            // First compare by priority level (lower number = higher priority)
            if (a->priorityLevel != b->priorityLevel)
                return a->priorityLevel > b->priorityLevel;
            
            // If same priority, compare by scheduled time (earlier = higher priority)
            return a->scheduledTime > b->scheduledTime;
        }
    };

    std::priority_queue<Flight*, std::vector<Flight*>, FlightPriorityComparator> runwayQueue;

    bip::managed_shared_memory segment;
    SharedRunwayStatus* sharedRunwayStatus;

public:
    ATCSController() : 
        simulationRunning(false), 
        flightGenerationRunning(false),
        simulationDuration(std::chrono::seconds(300)), // 5 minutes
        segment(bip::open_or_create, "ATCSSharedMemory", 65536)
    {
        // Clean up old shared memory at startup
        bip::shared_memory_object::remove("ATCSSharedMemory");
        bip::shared_memory_object::remove("AVNSharedMemory");
        bip::named_mutex::remove("AVNMutex");
        
        // Initialize shared memory
        segment = bip::managed_shared_memory(bip::create_only, "ATCSSharedMemory", 65536);
        sharedRunwayStatus = segment.construct<SharedRunwayStatus>("RunwayStatus")();

        // Initialize airlines
        airlines.emplace_back("PIA", AircraftType::Commercial, 6, 4);
        airlines.emplace_back("AirBlue", AircraftType::Commercial, 4, 4);
        airlines.emplace_back("FedEx Cargo", AircraftType::Cargo, 3, 2);
        airlines.emplace_back("Pakistan Airforce", AircraftType::Emergency, 2, 1);
        airlines.emplace_back("Blue Dart Cargo", AircraftType::Cargo, 2, 2);
        airlines.emplace_back("AghaKhan Air Ambulance", AircraftType::Emergency, 2, 1);

        // Initialize runways
        runways.push_back(std::make_unique<Runway>(0, "RWY-A (North-South Arrivals)"));
        runways.push_back(std::make_unique<Runway>(1, "RWY-B (East-West Departures)"));
        runways.push_back(std::make_unique<Runway>(2, "RWY-C (Cargo/Emergency/Overflow)"));

        // Initialize flight schedules with specific emergency types
        flightSchedules = {
            {FlightDirection::NorthArrival, 180, 0.10f, "International Arrivals", EmergencyType::DiversionOrLowFuel},
            {FlightDirection::SouthArrival, 120, 0.05f, "Domestic Arrivals", EmergencyType::Medical},
            {FlightDirection::EastDeparture, 150, 0.15f, "International Departures", EmergencyType::Military},
            {FlightDirection::WestDeparture, 240, 0.20f, "Domestic Departures", EmergencyType::VIP}
        };

        // Initialize AVN Generator
        avnGenerator = std::make_unique<AVNGenerator>();
    }

    // Add flight to system
    void addFlight(std::unique_ptr<Flight> flight) {
        std::lock_guard<std::mutex> lock(flightsMutex);
        Flight* flightPtr = flight.get();
        runwayQueue.push(flightPtr);
        
        {
            std::lock_guard<std::mutex> consoleLock(g_console_mutex);
            std::cout << "\n=== NEW FLIGHT ADDED ===\n";
            std::cout << "Flight: #" << flight->flightNumber 
                      << " (" << flight->airline->name << ")\n";
            std::cout << "Type: " << flight->getAircraftTypeString() << "\n";
            std::cout << "Direction: " << flight->getDirectionString() << "\n";
            std::cout << "============================\n";
        }
                  
        flights.push_back(std::move(flight));
    }

    // Thread function to manage flight lifecycle
    void flightThread(Flight& flight) {
        using namespace std::chrono;
        steady_clock::time_point phaseStart = steady_clock::now();
        
        while (simulationRunning) {
            std::this_thread::sleep_for(milliseconds(100));
            steady_clock::time_point now = steady_clock::now();
            auto elapsed = duration_cast<seconds>(now - phaseStart).count();

            // Manage flight phases with timing
            switch (flight.phase) {
                case FlightPhase::Holding:
                    if (elapsed >= 10) {
                        flight.updatePhase(FlightPhase::Approach);
                        flight.updateSpeed(400 + rand() % 201); // 400-600 km/h
                        phaseStart = now;
                        
                        {
                            std::lock_guard<std::mutex> consoleLock(g_console_mutex);
                            std::cout << "\n=== PHASE TRANSITION ===\n";
                            std::cout << "Flight: #" << flight.flightNumber << "\n";
                            std::cout << "New Phase: " << flight.getPhaseString() << "\n";
                            std::cout << "Speed: " << flight.speed << " km/h\n";
                            std::cout << "============================\n";
                        }
                        checkSpeedViolation(flight);
                    }
                    break;
                    
                case FlightPhase::Approach:
                    if (elapsed >= 8) {
                        flight.updatePhase(FlightPhase::Landing);
                        flight.updateSpeed(240); // Start at max allowed landing speed
                        phaseStart = now;
                        
                        {
                            std::lock_guard<std::mutex> consoleLock(g_console_mutex);
                            std::cout << "\n=== PHASE TRANSITION ===\n";
                            std::cout << "Flight: #" << flight.flightNumber << "\n";
                            std::cout << "New Phase: " << flight.getPhaseString() << "\n";
                            std::cout << "Speed: " << flight.speed << " km/h\n";
                            std::cout << "============================\n";
                        }
                        checkSpeedViolation(flight);
                    }
                    break;
                    
                case FlightPhase::Landing:
                    // Gradually decrease speed during landing
                    if (elapsed < 6) {
                        float landingProgress = elapsed / 6.0f; // 0 to 1
                        float newSpeed = 240.0f * (1.0f - landingProgress) + 30.0f * landingProgress;
                        flight.updateSpeed(newSpeed);
                        checkSpeedViolation(flight);
                    } else {
                        flight.updatePhase(FlightPhase::Taxi);
                        flight.updateSpeed(20); // Safe taxi speed
                        phaseStart = now;
                        
                        {
                            std::lock_guard<std::mutex> consoleLock(g_console_mutex);
                            std::cout << "\n=== PHASE TRANSITION ===\n";
                            std::cout << "Flight: #" << flight.flightNumber << "\n";
                            std::cout << "New Phase: " << flight.getPhaseString() << "\n";
                            std::cout << "Speed: " << flight.speed << " km/h\n";
                            std::cout << "============================\n";
                        }
                    }
                    break;
                    
                case FlightPhase::Taxi:
                    if (elapsed >= 5) {
                        flight.updatePhase(FlightPhase::AtGate);
                        flight.updateSpeed(0.0f);
                        phaseStart = now;
                        
                        {
                            std::lock_guard<std::mutex> consoleLock(g_console_mutex);
                            std::cout << "\n=== PHASE TRANSITION ===\n";
                            std::cout << "Flight: #" << flight.flightNumber << "\n";
                            std::cout << "New Phase: " << flight.getPhaseString() << "\n";
                            std::cout << "============================\n";
                        }
                    }
                    break;
                case FlightPhase::AtGate:
                    if (elapsed >= 5) {
                        if (flight.direction == FlightDirection::EastDeparture || 
                            flight.direction == FlightDirection::WestDeparture) {
                            flight.updatePhase(FlightPhase::Taxi);
                            flight.updateSpeed(15 + rand() % 16); // 15-30 km/h for taxiing
                            phaseStart = now;
                            
                            {
                                std::lock_guard<std::mutex> consoleLock(g_console_mutex);
                                std::cout << "\n=== PHASE TRANSITION ===\n";
                                std::cout << "Flight: #" << flight.flightNumber << "\n";
                                std::cout << "New Phase: " << flight.getPhaseString() << "\n";
                                std::cout << "Speed: " << flight.speed << " km/h\n";
                                std::cout << "============================\n";
                            }
                        }
                    }
                    if (flight.runwayAssigned != -1) {
                        releaseRunway(flight.runwayAssigned);
                        flight.runwayAssigned = -1;
                        
                        {
                            std::lock_guard<std::mutex> consoleLock(g_console_mutex);
                            std::cout << "\n=== RUNWAY RELEASED ===\n";
                            std::cout << "Flight: #" << flight.flightNumber << "\n";
                            std::cout << "Runway: " << flight.runwayAssigned << "\n";
                            std::cout << "============================\n";
                        }
                    }
                    break;
                case FlightPhase::TakeoffRoll:
                    if (elapsed >= 3) {
                        flight.updatePhase(FlightPhase::Climb);
                        flight.updateSpeed(250 + rand() % 213); // 250-463 km/h
                        phaseStart = now;
                        
                        {
                            std::lock_guard<std::mutex> consoleLock(g_console_mutex);
                            std::cout << "\n=== PHASE TRANSITION ===\n";
                            std::cout << "Flight: #" << flight.flightNumber << "\n";
                            std::cout << "New Phase: " << flight.getPhaseString() << "\n";
                            std::cout << "Speed: " << flight.speed << " km/h\n";
                            std::cout << "============================\n";
                        }
                        
                        // Check for speed violations in climb phase
                        checkSpeedViolation(flight);
                    } else {
                        // Gradually increase speed during takeoff roll
                        float rollProgress = static_cast<float>(elapsed) / 3.0f; // 0 to 1
                        flight.updateSpeed(290.0f * rollProgress); // Up to 290 km/h
                    }
                    break;
                case FlightPhase::Climb:
                    if (elapsed >= 4) {
                        flight.updatePhase(FlightPhase::Cruise);
                        flight.updateSpeed(800 + rand() % 101); // 800-900 km/h
                        phaseStart = now;
                        
                        {
                            std::lock_guard<std::mutex> consoleLock(g_console_mutex);
                            std::cout << "\n=== PHASE TRANSITION ===\n";
                            std::cout << "Flight: #" << flight.flightNumber << "\n";
                            std::cout << "New Phase: " << flight.getPhaseString() << "\n";
                            std::cout << "Speed: " << flight.speed << " km/h\n";
                            std::cout << "============================\n";
                        }
                        
                        // Check for speed violations in cruise phase
                        checkSpeedViolation(flight);
                        
                        // Release runway after aircraft has climbed
                        if (flight.runwayAssigned != -1) {
                            releaseRunway(flight.runwayAssigned);
                            flight.runwayAssigned = -1;
                            
                            {
                                std::lock_guard<std::mutex> consoleLock(g_console_mutex);
                                std::cout << "\n=== RUNWAY RELEASED ===\n";
                                std::cout << "Flight: #" << flight.flightNumber << "\n";
                                std::cout << "Runway: " << flight.runwayAssigned << "\n";
                                std::cout << "============================\n";
                            }
                        }
                    }
                    break;
                case FlightPhase::Cruise:
                    if (elapsed >= 10) {
                        flight.updatePhase(FlightPhase::Departure);
                        std::cout << "Flight #" << flight.flightNumber << " departed from airspace." << std::endl;
                        // Flight has left the controlled airspace, end its simulation
                        return;
                    }
                    break;
                case FlightPhase::Departure:
                    {
                        std::lock_guard<std::mutex> consoleLock(g_console_mutex);
                        std::cout << "Flight #" << flight.flightNumber << " departed from airspace.\n";
                        std::cout << "============================\n";
                    }
                    return;  // End the flight thread
                break;
                default:
                    break;
            }

            // Check for ground faults if still active
            if (simulationRunning) {
                checkGroundFaults(flight);
            }
        }
    }

    void checkSpeedViolation(Flight& flight) {
        float permissibleSpeed = 0.0f;
        bool violation = false;
        std::string violationReason;

        switch (flight.phase) {
            case FlightPhase::Holding:
                permissibleSpeed = 600.0f;
                if (flight.speed > 600.0f || flight.speed < 400.0f) {
                    violation = true;
                    violationReason = "Speed outside holding range (400-600 km/h)";
                }
                break;
            case FlightPhase::Approach:
                permissibleSpeed = 290.0f;
                if (flight.speed < 240.0f || flight.speed > 290.0f) {
                    violation = true;
                    violationReason = "Speed outside approach range (240-290 km/h)";
                }
                break;
            case FlightPhase::Landing:
                permissibleSpeed = 240.0f;
                if (flight.speed > 240.0f) {
                    violation = true;
                    violationReason = "Exceeded landing speed limit (240 km/h)";
                }
                break;
            case FlightPhase::Taxi:
                permissibleSpeed = 30.0f;
                if (flight.speed > 30.0f || flight.speed < 15.0f) {
                    violation = true;
                    violationReason = "Speed outside taxi range (15-30 km/h)";
                }
                break;
            case FlightPhase::AtGate:
                permissibleSpeed = 5.0f;
                if (flight.speed > 5.0f) {
                    violation = true;
                    violationReason = "Exceeded gate speed limit (5 km/h)";
                }
                break;
            case FlightPhase::TakeoffRoll:
                permissibleSpeed = 290.0f;
                if (flight.speed > 290.0f) {
                    violation = true;
                    violationReason = "Exceeded takeoff roll speed limit (290 km/h)";
                }
                break;
            case FlightPhase::Climb:
                permissibleSpeed = 463.0f;
                if (flight.speed < 250.0f || flight.speed > 463.0f) {
                    violation = true;
                    violationReason = "Speed outside climb range (250-463 km/h)";
                }
                break;
            case FlightPhase::Cruise:
                permissibleSpeed = 900.0f;
                if (flight.speed < 800.0f || flight.speed > 900.0f) {
                    violation = true;
                    violationReason = "Speed outside cruise range (800-900 km/h)";
                }
                break;
        }

        if (violation && !flight.violationActive) {
            flight.violationActive = true;
            flight.violationReason = violationReason;
            
            {
                std::lock_guard<std::mutex> consoleLock(g_console_mutex);
                std::cout << "\n=== SPEED VIOLATION DETECTED ===\n";
                std::cout << "Flight: #" << flight.flightNumber 
                          << " (" << flight.airline->name << ")\n";
                std::cout << "Phase: " << flight.getPhaseString() << "\n";
                std::cout << "Speed: " << flight.speed << " km/h (Limit: " << permissibleSpeed << " km/h)\n";
                std::cout << "Reason: " << violationReason << "\n";
                std::cout << "============================\n";
            }
            
            // Generate AVN and store its ID
            int avnID = avnGenerator->generateAVN(&flight, permissibleSpeed);
            flight.avnIDs.push_back(avnID);
            
            {
                std::lock_guard<std::mutex> consoleLock(g_console_mutex);
                std::cout << "\n=== AVN GENERATED ===\n";
                std::cout << "AVN ID: #" << avnID << "\n";
                std::cout << "Flight: #" << flight.flightNumber << "\n";
                std::cout << "Airline: " << flight.airline->name << "\n";
                std::cout << "============================\n";
            }
        }
    }

    // Ground fault handling
    void checkGroundFaults(Flight& flight) {
        if (flight.phase == FlightPhase::Taxi || flight.phase == FlightPhase::AtGate) {
            // 5% chance of fault during ground operations
            static std::random_device rd;
            static std::mt19937 gen(rd());
            static std::uniform_real_distribution<> dis(0.0, 1.0);
            
            if (dis(gen) < 0.05 && !flight.hasFault) {
                flight.hasFault = true;
                // Randomly select fault type
                const std::vector<std::string> faultTypes = {
                    "Brake failure",
                    "Hydraulic leak",
                    "APU malfunction",
                    "Steering system fault"
                };
                flight.faultDescription = faultTypes[rand() % faultTypes.size()];
                
                std::lock_guard<std::mutex> consoleLock(g_console_mutex);
                std::cout << "\n=== GROUND FAULT DETECTED ===\n";
                std::cout << "Flight: #" << flight.flightNumber << "\n";
                std::cout << "Fault: " << flight.faultDescription << "\n";
                std::cout << "Action: Aircraft being towed to maintenance\n";
                std::cout << "============================\n";
                
                // Remove from active queues
                removeFaultedFlight(flight);
            }
        }
    }

    void removeFaultedFlight(Flight& flight) {
        std::lock_guard<std::mutex> lock(flightsMutex);
        // Remove from runway queue if present
        std::vector<Flight*> tempQueue;
        while (!runwayQueue.empty()) {
            Flight* f = runwayQueue.top();
            runwayQueue.pop();
            if (f->flightNumber != flight.flightNumber) {
                tempQueue.push_back(f);
            }
        }
        for (auto* f : tempQueue) {
            runwayQueue.push(f);
        }
        
        // Release runway if assigned
        if (flight.runwayAssigned != -1) {
            releaseRunway(flight.runwayAssigned);
            flight.runwayAssigned = -1;
        }
    }

    // Runway management functions
    bool assignRunway(Flight& flight) {
        // Determine preferred runway based on direction and type
        int preferredRunway = -1;
        
        if (flight.aircraftType == AircraftType::Cargo || 
            flight.aircraftType == AircraftType::Emergency) {
            preferredRunway = 2; // RWY-C for cargo and emergency
        } else if (flight.direction == FlightDirection::NorthArrival || 
                   flight.direction == FlightDirection::SouthArrival) {
            preferredRunway = 0; // RWY-A for arrivals
        } else {
            preferredRunway = 1; // RWY-B for departures
        }
        
        // Try preferred runway first
        if (preferredRunway >= 0 && preferredRunway < runways.size()) {
            std::lock_guard<std::mutex> lock(runways[preferredRunway]->runwayMutex);
            if (!runways[preferredRunway]->occupied.load()) {
                runways[preferredRunway]->occupied.store(true);
                flight.runwayAssigned = preferredRunway;
                flight.runwayOccupied = true;
                
                std::lock_guard<std::mutex> consoleLock(g_console_mutex);
                std::cout << "\n=== RUNWAY ASSIGNMENT ===\n";
                std::cout << "Flight: #" << flight.flightNumber << "\n";
                std::cout << "Runway: " << runways[preferredRunway]->name << "\n";
                std::cout << "============================\n";
                return true;
            }
        }
        
        // If preferred runway not available, try alternatives (except for cargo)
        if (flight.aircraftType != AircraftType::Cargo) {
            for (int i = 0; i < runways.size(); i++) {
                if (i != preferredRunway) {
                    std::lock_guard<std::mutex> lock(runways[i]->runwayMutex);
                    if (!runways[i]->occupied.load()) {
                        runways[i]->occupied.store(true);
                        flight.runwayAssigned = i;
                        flight.runwayOccupied = true;
                        
                        std::lock_guard<std::mutex> consoleLock(g_console_mutex);
                        std::cout << "\n=== OVERFLOW RUNWAY ASSIGNMENT ===\n";
                        std::cout << "Flight: #" << flight.flightNumber << "\n";
                        std::cout << "Runway: " << runways[i]->name << " (overflow)\n";
                        std::cout << "============================\n";
                        return true;
                    }
                }
            }
        }
        
        return false;
    }

    void releaseRunway(int runwayID) {
        if (runwayID >= 0 && runwayID < runways.size()) {
            std::lock_guard<std::mutex> lock(runways[runwayID]->runwayMutex);
            runways[runwayID]->occupied.store(false);
            std::cout << "\n=== RUNWAY STATUS UPDATE ===\n";
            std::cout << "Runway: " << runways[runwayID]->name << " released\n";
            std::cout << "============================\n";
        }
    }

    // Flight generation thread function
    void flightGenerationThread() {
        using namespace std::chrono;
        auto startTime = steady_clock::now();
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(0.0, 1.0);
        
        std::vector<steady_clock::time_point> nextFlightTimes;
        for (const auto& schedule : flightSchedules) {
            nextFlightTimes.push_back(startTime);
        }
        
        flightGenerationRunning = true;
        static std::atomic<unsigned int> flightNumberCounter{1000};  // Changed to unsigned int
        
        while (flightGenerationRunning) {
            auto now = steady_clock::now();
            
            // Check each flight schedule
            for (size_t i = 0; i < flightSchedules.size(); i++) {
                const auto& schedule = flightSchedules[i];
                
                if (now >= nextFlightTimes[i]) {
                    bool isEmergency = (dis(gen) < schedule.emergencyProbability);
                    
                    std::vector<size_t> candidateAirlines;
                    for (size_t j = 0; j < airlines.size(); j++) {
                        // Select appropriate airline based on emergency type
                        if (isEmergency) {
                            // For military emergencies, only Pakistan Airforce
                            if (schedule.emergencyType == EmergencyType::Military && 
                                airlines[j].name == "Pakistan Airforce") {
                                candidateAirlines.push_back(j);
                            }
                            // For medical emergencies, only AghaKhan Air Ambulance
                            else if (schedule.emergencyType == EmergencyType::Medical && 
                                   airlines[j].name == "AghaKhan Air Ambulance") {
                                candidateAirlines.push_back(j);
                            }
                            // For other emergencies, any emergency airline
                            else if (schedule.emergencyType != EmergencyType::Military && 
                                   schedule.emergencyType != EmergencyType::Medical && 
                                   airlines[j].type == AircraftType::Emergency) {
                                candidateAirlines.push_back(j);
                            }
                        } else {
                            // For non-emergency flights
                            if (airlines[j].type != AircraftType::Emergency) {
                                candidateAirlines.push_back(j);
                            }
                        }
                    }
                    
                    if (!candidateAirlines.empty()) {
                        size_t airlineIdx = candidateAirlines[dis(gen) * candidateAirlines.size()];
                        Airline& airline = airlines[airlineIdx];
                        
                        EmergencyType emType = EmergencyType::None;
                        if (isEmergency) {
                            emType = schedule.emergencyType;
                        }
                        
                        unsigned int flightNumber = flightNumberCounter++;
                        
                        auto newFlight = std::make_unique<Flight>(flightNumber, &airline, 
                                                              airline.type, 
                                                              schedule.direction, 
                                                              system_clock::now(),
                                                              emType);
                        Flight* flightPtr = newFlight.get();
                        
                        {
                            std::lock_guard<std::mutex> lock(flightsMutex);
                            std::lock_guard<std::mutex> consoleLock(g_console_mutex);
                            
                            std::cout << "\n=== NEW FLIGHT ADDED ===\n";
                            std::cout << "Flight: #" << flightPtr->flightNumber << "\n";
                            std::cout << "Airline: " << flightPtr->airline->name << "\n";
                            std::cout << "Type: " << flightPtr->getAircraftTypeString() << "\n";
                            std::cout << "Direction: " << flightPtr->getDirectionString() << "\n";
                            if (emType != EmergencyType::None) {
                                std::cout << "Emergency: " << flightPtr->getEmergencyTypeString() << "\n";
                            }
                            std::cout << "============================\n";
                            
                            flights.push_back(std::move(newFlight));
                            runwayQueue.push(flightPtr);
                        }
                        
                        std::thread([this, flightPtr]() {
                            this->flightThread(*flightPtr);
                        }).detach();
                    }
                    
                    nextFlightTimes[i] = now + seconds(schedule.intervalSeconds);
                }
            }
            
            std::this_thread::sleep_for(milliseconds(100));
        }
    }

    // Runway management thread function
    void runwayManagementThread() {
        using namespace std::chrono;
        auto lastAnalyticsTime = steady_clock::now();
        
        while (simulationRunning) {
            auto now = steady_clock::now();
            
            // Display analytics every 30 seconds
            if (duration_cast<seconds>(now - lastAnalyticsTime).count() >= 30) {
                displayAnalytics();
                lastAnalyticsTime = now;
            }
            
            std::this_thread::sleep_for(milliseconds(500));
            
            // Process runway queue
            std::lock_guard<std::mutex> lock(flightsMutex);
            while (!runwayQueue.empty()) {
                Flight* flight = runwayQueue.top();
                
                if (flight->runwayAssigned == -1) {
                    if (assignRunway(*flight)) {
                        runwayQueue.pop();
                    } else {
                        // Couldn't assign runway, keep in queue
                        flight->estimatedWaitTime += 500;
                        break;
                    }
                } else {
                    runwayQueue.pop();
                }
            }
        }
    }

    std::string flightPhaseToString(FlightPhase phase) {
        switch (phase) {
            case FlightPhase::Holding: return "Holding";
            case FlightPhase::Approach: return "Approach";
            case FlightPhase::Landing: return "Landing";
            case FlightPhase::Taxi: return "Taxi";
            case FlightPhase::AtGate: return "At Gate";
            case FlightPhase::TakeoffRoll: return "Takeoff Roll";
            case FlightPhase::Climb: return "Climb";
            case FlightPhase::Cruise: return "Cruise";
            case FlightPhase::Departure: return "Departure";
            default: return "Unknown";
        }
    }

    // Analytics functions
    void displayAnalytics() {
        std::lock_guard<std::mutex> lock(flightsMutex);
        std::lock_guard<std::mutex> avnLock(avnMutex);
        std::lock_guard<std::mutex> consoleLock(g_console_mutex);
        
        // Get current time
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        
        std::cout << "\n=== ATC DASHBOARD ===\n";
        std::cout << "Time: " << std::put_time(std::localtime(&time), "%H:%M:%S") << "\n";

        // Count active flights and their states
        int activeFlights = 0;
        int activeViolations = 0;
        int flightsInAir = 0;
        int flightsOnGround = 0;
        int emergencyFlights = 0;
        std::map<std::string, int> airlineCounts;
        std::map<FlightPhase, int> phaseCounts;
        
        for (const auto& flight : flights) {
            activeFlights++;
            if (flight->violationActive) activeViolations++;
            if (flight->emergencyType != EmergencyType::None) emergencyFlights++;
            
            if (flight->phase == FlightPhase::Holding || 
                flight->phase == FlightPhase::Approach || 
                flight->phase == FlightPhase::Landing ||
                flight->phase == FlightPhase::Climb || 
                flight->phase == FlightPhase::Cruise) {
                flightsInAir++;
            } else {
                flightsOnGround++;
            }
            
            airlineCounts[flight->airline->name]++;
            phaseCounts[flight->phase]++;
        }
        
        // Display counts
        std::cout << "Active Flights: " << activeFlights << "\n";
        std::cout << "In Air: " << flightsInAir << " | On Ground: " << flightsOnGround << "\n";
        std::cout << "Emergency Flights: " << emergencyFlights << "\n";
        std::cout << "Active Violations: " << activeViolations << "\n";
        
        // Display runway status
        std::cout << "RUNWAY STATUS:\n";
        for (const auto& runway : runways) {
            std::cout << "  " << std::left << std::setw(30) << runway->name 
                      << (runway->occupied.load() ? "OCCUPIED" : "AVAILABLE") << "\n";
        }
        
        // Display airline activity
        std::cout << "AIRLINE ACTIVITY:\n";
        for (const auto& entry : airlineCounts) {
            std::cout << "  " << std::left << std::setw(20) << entry.first 
                      << ": " << entry.second << " flights\n";
        }
        
        // Display phase distribution
        std::cout << "FLIGHT PHASES:\n";
        for (const auto& entry : phaseCounts) {
            std::cout << "  " << std::left << std::setw(20) << flightPhaseToString(entry.first) 
                      << ": " << entry.second << "\n";
        }
        
        std::cout << "============================\n\n";
    }

    // Start simulation
    void startSimulation() {
        simulationRunning = true;
        flightGenerationRunning = true;
        simulationStartTime = std::chrono::steady_clock::now();
        
        // Start flight generation thread
        std::thread generationThread(&ATCSController::flightGenerationThread, this);
        
        // Start runway management thread
        std::thread runwayThread(&ATCSController::runwayManagementThread, this);
        
        // Main simulation loop
        auto lastAnalyticsTime = simulationStartTime;
        int lastAnalyticsPeriod = 0;
        
        while (simulationRunning) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - simulationStartTime).count();
            
            // Check for simulation end
            if (elapsed >= simulationDuration.count()) {
                {
                    std::lock_guard<std::mutex> consoleLock(g_console_mutex);
                    std::cout << "\n=== SIMULATION TIME COMPLETED ===\n";
                    std::cout << "Total simulation time: " << elapsed << " seconds\n";
                    std::cout << "============================\n";
                }
                
                // Signal threads to stop
                simulationRunning = false;
                flightGenerationRunning = false;
                break;
            }
            
            // Display analytics every 30 seconds and at the end
            int currentPeriod = elapsed / 30;
            if (currentPeriod > lastAnalyticsPeriod) {
                displayAnalytics();
                lastAnalyticsPeriod = currentPeriod;
                lastAnalyticsTime = now;
            }
            
            // Sleep to prevent high CPU usage
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // Wait for threads to finish
        if (generationThread.joinable()) {
            generationThread.join();
        }
        if (runwayThread.joinable()) {
            runwayThread.join();
        }
        
        // Display final analytics
        displayAnalytics();
        
        // Show completion message
        {
            std::lock_guard<std::mutex> consoleLock(g_console_mutex);
            std::cout << "\nSimulation completed after " << simulationDuration.count() << " seconds.\n";
            std::cout << "All threads terminated successfully.\n";
        }
    }
};

int main() {
    std::cout << "Starting Air Traffic Control System Simulation...\n";
    
    ATCSController atcs;
    std::atomic<bool> shouldExit{false};
    
    // Start simulation in a separate thread
    std::thread simulationThread(&ATCSController::startSimulation, &atcs);
    
    // Simple command interface for testing
    std::string command;
    while (!shouldExit) {
        std::cout << "\nEnter command (airline, pay, exit): ";
        std::getline(std::cin, command);
        
        if (command == "exit") {
            shouldExit = true;
        } else if (command == "airline") {
            std::string airlineName;
            std::cout << "Enter airline name (PIA, AirBlue, FedEx Cargo, etc.): ";
            std::getline(std::cin, airlineName);
            
            AirlinePortal portal(airlineName);
            portal.listActiveAVNs();
            
            std::cout << "\nPay an AVN? (y/n): ";
            std::getline(std::cin, command);
            if (command == "y") {
                int avnID;
                std::cout << "Enter AVN ID to pay: ";
                std::cin >> avnID;
                std::cin.ignore();
                
                portal.payAVN(avnID);
            }
        } else if (command == "pay") {
            int avnID;
            double amount;
            
            std::cout << "Enter AVN ID: ";
            std::cin >> avnID;
            std::cout << "Enter amount: ";
            std::cin >> amount;
            std::cin.ignore();
            
            StripePay stripePay;
            stripePay.processPayment(avnID, amount);
        }
    }
    
    // Clean up shared memory
    bip::shared_memory_object::remove("AVNSharedMemory");
    bip::named_mutex::remove("AVNMutex");
    
    if (simulationThread.joinable()) {
        simulationThread.join();
    }
    
    return 0;
}