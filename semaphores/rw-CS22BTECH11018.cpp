#include <iostream>
#include <fstream>
#include <thread>
#include <semaphore.h>
#include <chrono>
#include <ctime>
#include <random>
#include <iomanip>
#include <vector>
#include <cstring> // for strerror
#include <sstream> // for stringstream

using namespace std;
using namespace chrono;

// Shared variables
int readcount = 0;    // Number of readers currently accessing resource
int writecount = 0;   // Number of writers currently accessing resource
bool writing = false; // Indicates if a writer is currently writing
double muCS;
double muRem;

// Semaphores
sem_t read_lock, write_lock, readTry, read_write_lock, l;

// Output files
ofstream rw_lock("RW-log.txt");
// Structure to store thread's waiting time
struct ThreadWaitTime
{
    vector<double> waitTimes;
    double avgWaitTime;
};

// Function to get current system time in hours, minutes, seconds, and microseconds
string getSysTime()
{
    auto now = system_clock::now();
    auto now_time = system_clock::to_time_t(now);
    auto tm = localtime(&now_time);
    auto ms = duration_cast<microseconds>(now.time_since_epoch()) % 1000000;

    stringstream ss;
    ss << put_time(tm, "%T") << '.' << setw(6) << setfill('0') << ms.count();

    return ss.str();
}

void reader(int id, int kr, ThreadWaitTime &waitTime)
{
    default_random_engine generator(chrono::system_clock::now().time_since_epoch().count());
    exponential_distribution<double> distribution_muCS(1.0 / muCS);
    exponential_distribution<double> distribution_muRem(1.0 / muRem);

    for (int i = 0; i < kr; ++i)
    {

        auto start_waiting = system_clock::now();
        sem_wait(&l);
        // Request to enter critical section
        rw_lock << i + 1 << "st "
                << "CS Request by Reader Thread " << id << " at " << getSysTime() << endl;
        sem_post(&l);
        sem_wait(&readTry);
        sem_wait(&read_lock);
        readcount++;
        if (readcount == 1)
            sem_wait(&read_write_lock);
        sem_post(&read_lock);
        sem_post(&readTry);
        auto end_waiting = system_clock::now();
        auto wait_time = duration_cast<microseconds>(end_waiting - start_waiting).count() / 1000000.0; // Convert to seconds
        waitTime.waitTimes.push_back(wait_time);

        sem_wait(&l);
        // Enter critical section
        rw_lock << i + 1 << "st "
                << "CS Entry by Reader Thread " << id << " at " << getSysTime() << endl;
        sem_post(&l);
        // Random delay for critical section
        double delay_CS = distribution_muCS(generator);
        this_thread::sleep_for(milliseconds(static_cast<int>(delay_CS )));

        // Exit critical section
        sem_wait(&read_lock);
        readcount--;
        if (readcount == 0)
            sem_post(&read_write_lock);
        sem_post(&read_lock);
        sem_wait(&l);
        rw_lock << i + 1 << "st "
                << "CS Exit by Reader Thread " << id << " at " << getSysTime() << endl;
        sem_post(&l);
        // Simulate remaining time outside critical section
        double delay_Rem = distribution_muRem(generator);                         // Fixed variable name here
        this_thread::sleep_for(milliseconds(static_cast<int>(delay_Rem ))); // Fixed variable name here
    }
}

// Writer thread function (for Fair solution)
void writer(int id, int kw,ThreadWaitTime &waitTime)
{
    default_random_engine generator(chrono::system_clock::now().time_since_epoch().count());
    exponential_distribution<double> distribution_muCS(1.0 / muCS);
    exponential_distribution<double> distribution_muRem(1.0 / muRem);

    for (int i = 0; i < kw; ++i)
    {

        auto start_waiting = system_clock::now();
        sem_wait(&l);
        // Request to enter critical section
        rw_lock << i + 1 << "st "
                << "CS Request by Writer Thread " << id << " at " << getSysTime() << endl;
        sem_post(&l);
        sem_wait(&write_lock);
        writecount++;
        if (writecount == 1)
            sem_wait(&readTry);
        sem_post(&write_lock);
        sem_wait(&read_write_lock);
         auto end_waiting = system_clock::now();
        auto wait_time = duration_cast<microseconds>(end_waiting - start_waiting).count() / 1000000.0; // Convert to seconds
        waitTime.waitTimes.push_back(wait_time);
        sem_wait(&l);
        // Enter critical section
        rw_lock << i + 1 << "st "
                << "CS Entry by Writer Thread " << id << " at " << getSysTime() << endl;
        sem_post(&l);
        // Random delay for critical section
        double delay_CS = distribution_muCS(generator);
        this_thread::sleep_for(milliseconds(static_cast<int>(delay_CS )));
        sem_wait(&l);
        // Exit critical section
        rw_lock << i + 1 << "st "
                << "CS Exit by Writer Thread " << id << " at " << getSysTime() << endl;
        sem_post(&l);
        sem_post(&read_write_lock);

        sem_wait(&write_lock);
        writecount--;
        if (writecount == 0)
            sem_post(&readTry);
        sem_post(&write_lock);

        // Simulate remaining time outside critical section
        double delay_Rem = distribution_muRem(generator);                         // Fixed variable name here
        this_thread::sleep_for(milliseconds(static_cast<int>(delay_Rem ))); // Fixed variable name here
    }
}

int main()
{
    // Record the start time
    auto start_time = system_clock::now();

    // Read parameters from file
    ifstream input("inp-params.txt");
    if (!input)
    {
        cerr << "Error: Unable to open input file." << endl;
        return 1;
    }

    int nw, nr, kw, kr;
    input >> nw >> nr >> kw >> kr >> muCS >> muRem;
    input.close();

    // Initialize semaphores
    if (sem_init(&read_lock, 0, 1) == -1 || sem_init(&write_lock, 0, 1) == -1 || sem_init(&readTry, 0, 1) == -1 || sem_init(&read_write_lock, 0, 1) == -1)
    {
        cerr << "Error initializing semaphores: " << strerror(errno) << endl;
        return 1;
    }
    sem_init(&l, 0, 1); // Initialize additional semaphore

    vector<thread> threadsW;
    vector<thread> threadsR;

     // Vector to store thread waiting times
    vector<ThreadWaitTime> readerWaitTimes(nr);
    vector<ThreadWaitTime> writerWaitTimes(nw);

    // Create writer threads (Writer Preference)
    for (int i = 0; i < nw; ++i)
        threadsW.push_back(thread(writer, i + 1, kw,ref(writerWaitTimes[i])));

    // Create reader threads (Writer Preference)
    for (int i = 0; i < nr; ++i)
        threadsR.push_back(thread(reader, i + 1, kr,ref(readerWaitTimes[i])));

    // Join writer threads (Writer Preference)
    for (auto &writer_thread : threadsW)
        writer_thread.join();

    // Join reader threads (Writer Preference)
    for (auto &reader_thread : threadsR)
        reader_thread.join();

    // Record the end time
    auto end_time = system_clock::now();

    // Close log file
    rw_lock.close();

     ofstream avg_time_file("Average_time_rw.txt");
    if (!avg_time_file)
    {
        cerr << "Error: Unable to open avg_time.txt for writing." << endl;
        return 1;
    }

    avg_time_file << "Average Waiting Time for Reader Threads:" << endl;
    double totalReaderWaitTime = 0.0;
    for (int i = 0; i < nr; ++i)
    {
        double avgWaitTime = accumulate(readerWaitTimes[i].waitTimes.begin(), readerWaitTimes[i].waitTimes.end(), 0.0) / readerWaitTimes[i].waitTimes.size();
        avg_time_file << "Reader Thread " << i + 1 << ": " << avgWaitTime << " seconds" << endl;
        totalReaderWaitTime += avgWaitTime;
    }

    avg_time_file << endl;

    avg_time_file << "Average Waiting Time for Writer Threads:" << endl;
    double totalWriterWaitTime = 0.0;
    for (int i = 0; i < nw; ++i)
    {
        double avgWaitTime = accumulate(writerWaitTimes[i].waitTimes.begin(), writerWaitTimes[i].waitTimes.end(), 0.0) / writerWaitTimes[i].waitTimes.size();
        avg_time_file << "Writer Thread " << i + 1 << ": " << avgWaitTime << " seconds" << endl;
        totalWriterWaitTime += avgWaitTime;
    }

    avg_time_file << endl;

    // Calculate overall average waiting time for all reader and writer threads
    double overallAvgReaderWaitTime = totalReaderWaitTime / nr;
    double overallAvgWriterWaitTime = totalWriterWaitTime / nw;

    avg_time_file << "Overall Average Waiting Time for All Reader Threads: " << overallAvgReaderWaitTime << " seconds" << endl;
    avg_time_file << "Overall Average Waiting Time for All Writer Threads: " << overallAvgWriterWaitTime << " seconds" << endl;

    avg_time_file.close();
    

    // Destroy semaphores
    sem_destroy(&read_lock);
    sem_destroy(&write_lock);
    sem_destroy(&readTry);
    sem_destroy(&read_write_lock);
    sem_destroy(&l); // Destroy additional semaphore

    return 0;
}
