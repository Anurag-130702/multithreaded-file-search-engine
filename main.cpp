#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <filesystem>
#include <atomic>
#include <chrono>
#include <unordered_set>
#include <sstream>

namespace fs = std::filesystem;

// ================= THREAD POOL =================

class ThreadPool {
private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;

    std::mutex queueMutex;
    std::condition_variable taskAvailable;
    std::condition_variable taskFinished;

    bool stop;
    std::atomic<int> activeTasks;

public:
    ThreadPool(size_t threadCount) : stop(false), activeTasks(0) {
        for (size_t i = 0; i < threadCount; i++) {
            workers.emplace_back([this]() {
                while (true) {
                    std::function<void()> task;

                    {
                        std::unique_lock<std::mutex> lock(queueMutex);

                        taskAvailable.wait(lock, [this]() {
                            return stop || !tasks.empty();
                        });

                        if (stop && tasks.empty()) {
                            return;
                        }

                        task = std::move(tasks.front());
                        tasks.pop();
                        activeTasks++;
                    }

                    task();

                    {
                        std::lock_guard<std::mutex> lock(queueMutex);
                        activeTasks--;
                    }

                    taskFinished.notify_all();
                }
            });
        }
    }

    void enqueue(std::function<void()> task) {
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            tasks.push(std::move(task));
        }

        taskAvailable.notify_one();
    }

    void waitForCompletion() {
        std::unique_lock<std::mutex> lock(queueMutex);

        taskFinished.wait(lock, [this]() {
            return tasks.empty() && activeTasks.load() == 0;
        });
    }

    ~ThreadPool() {
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            stop = true;
        }

        taskAvailable.notify_all();

        for (std::thread& worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }
};

// ================= GLOBAL STATS =================

std::mutex printMutex;

struct SearchStats {
    std::atomic<int> filesScanned{0};
    std::atomic<int> matchesFound{0};
};

// ================= UTILITY FUNCTIONS =================

bool hasAllowedExtension(const fs::path& filePath,
                         const std::unordered_set<std::string>& extensions) {
    if (extensions.empty()) {
        return true;
    }

    std::string ext = filePath.extension().string();
    return extensions.find(ext) != extensions.end();
}

std::unordered_set<std::string> parseExtensions(const std::string& input) {
    std::unordered_set<std::string> extensions;
    std::stringstream ss(input);
    std::string ext;

    while (ss >> ext) {
        if (!ext.empty() && ext[0] != '.') {
            ext = "." + ext;
        }

        extensions.insert(ext);
    }

    return extensions;
}

// ================= SEARCH LOGIC =================

void searchInFile(const fs::path& filePath,
                  const std::string& keyword,
                  SearchStats& stats,
                  bool printMatches) {
    std::ifstream file(filePath);

    if (!file.is_open()) {
        return;
    }

    std::string line;
    int lineNumber = 0;
    bool fileCounted = false;

    while (std::getline(file, line)) {
        lineNumber++;

        if (line.find(keyword) != std::string::npos) {
            stats.matchesFound++;

            if (printMatches) {
                std::lock_guard<std::mutex> lock(printMutex);
                std::cout << "[MATCH] " << filePath.string()
                          << " | Line " << lineNumber << "\n";
            }
        }

        fileCounted = true;
    }

    if (fileCounted) {
        stats.filesScanned++;
    }
}

std::vector<fs::path> collectFiles(const fs::path& root,
                                   const std::unordered_set<std::string>& extensions) {
    std::vector<fs::path> files;

    try {
        for (const auto& entry : fs::recursive_directory_iterator(
                 root, fs::directory_options::skip_permission_denied)) {
            if (entry.is_regular_file() && hasAllowedExtension(entry.path(), extensions)) {
                files.push_back(entry.path());
            }
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << "\n";
    }

    return files;
}

// ================= SINGLE-THREADED SEARCH =================

double runSingleThreaded(const std::vector<fs::path>& files,
                         const std::string& keyword,
                         SearchStats& stats) {
    auto start = std::chrono::high_resolution_clock::now();

    for (const auto& file : files) {
        searchInFile(file, keyword, stats, false);
    }

    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> duration = end - start;
    return duration.count();
}

// ================= MULTITHREADED SEARCH =================

double runMultithreaded(const std::vector<fs::path>& files,
                        const std::string& keyword,
                        SearchStats& stats,
                        size_t threadCount) {
    auto start = std::chrono::high_resolution_clock::now();

    ThreadPool pool(threadCount);

    for (const auto& file : files) {
        pool.enqueue([file, &keyword, &stats]() {
            searchInFile(file, keyword, stats, true);
        });
    }

    pool.waitForCompletion();

    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> duration = end - start;
    return duration.count();
}

// ================= MAIN =================

int main() {
    std::string directory;
    std::string keyword;
    std::string extensionInput;

    std::cout << "========================================\n";
    std::cout << " Multithreaded File Search Engine\n";
    std::cout << "========================================\n\n";

    std::cout << "Enter directory path: ";
    std::getline(std::cin, directory);

    std::cout << "Enter keyword to search: ";
    std::getline(std::cin, keyword);

    std::cout << "Enter file extensions separated by space ";
    std::cout << "(example: .txt .cpp .log) or press Enter for all files: ";
    std::getline(std::cin, extensionInput);

    if (!fs::exists(directory)) {
        std::cerr << "Error: Directory does not exist.\n";
        return 1;
    }

    if (keyword.empty()) {
        std::cerr << "Error: Keyword cannot be empty.\n";
        return 1;
    }

    std::unordered_set<std::string> extensions = parseExtensions(extensionInput);

    std::cout << "\nCollecting files...\n";

    std::vector<fs::path> files = collectFiles(directory, extensions);

    if (files.empty()) {
        std::cout << "No matching files found.\n";
        return 0;
    }

    size_t threadCount = std::thread::hardware_concurrency();

    if (threadCount == 0) {
        threadCount = 4;
    }

    std::cout << "Files collected: " << files.size() << "\n";
    std::cout << "Threads used: " << threadCount << "\n\n";

    SearchStats singleStats;
    SearchStats multiStats;

    std::cout << "Running single-threaded search...\n";
    double singleTime = runSingleThreaded(files, keyword, singleStats);

    std::cout << "Running multithreaded search...\n";
    double multiTime = runMultithreaded(files, keyword, multiStats, threadCount);

    std::cout << "\n========================================\n";
    std::cout << " RESULTS\n";
    std::cout << "========================================\n";

    std::cout << "Single-threaded:\n";
    std::cout << "Files scanned: " << singleStats.filesScanned.load() << "\n";
    std::cout << "Matches found: " << singleStats.matchesFound.load() << "\n";
    std::cout << "Time taken: " << singleTime << " seconds\n\n";

    std::cout << "Multithreaded:\n";
    std::cout << "Files scanned: " << multiStats.filesScanned.load() << "\n";
    std::cout << "Matches found: " << multiStats.matchesFound.load() << "\n";
    std::cout << "Time taken: " << multiTime << " seconds\n\n";

    if (multiTime > 0) {
        double speedup = singleTime / multiTime;
        std::cout << "Speedup: " << speedup << "x\n";
    }

    std::cout << "\nSearch completed successfully.\n";

    return 0;
}