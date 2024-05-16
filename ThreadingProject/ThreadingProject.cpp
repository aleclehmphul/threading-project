// ThreadingProject.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <fstream>
#include <thread>
#include <mutex>
#include <chrono>

// Two allowed header files needed to get Memory Consumption
// These libraries are only used for that purpose
#include <windows.h>
#include <psapi.h>

// My own created helper classes
#include "ArrayListString.h"
#include "WordCounter.h"

#define MAX_THREADS 4
#define IN_FILENAME "../../TestCases/alice.txt"
#define OUT_FILENAME "../../TestCases/output/output_alice.txt"


ArrayListString fileLines = ArrayListString(32);
WordCounter wordCounter = WordCounter(32);
std::mutex mtx;

/* Manually converts a string to lowercase */
std::string toLowercase(std::string str) {
    std::string strLowercase;

    for (char c : str) {
        if (c >= 'A' && c <= 'Z') {
            strLowercase += (c - 'A' + 'a');
        }
        else {
            strLowercase += c;
        }
    }

    return strLowercase;
}

/* Reads file line by line */
void readFile(std::string filename) {
    std::ifstream inFile;
    inFile.open(filename);

    std::string line, lowercaseLine;

    while (getline(inFile, line)) {
        lowercaseLine = toLowercase(line);

        fileLines.addEntry(lowercaseLine);
    }

    inFile.close();
}


/* Checks if the character is an escape sequence character */
bool isEscapeSequence(char c) {
    return c == '\n' || c == '\t' || c == '\r'
        || c == '\\' || c == '\"'
        || c == '\?' || c == '\0';
}


/* Illegal characters are spaces, digits, punctuation, and escape characters */
bool isIllegalCharacter(char c, char previous, char next) {
    // For cases of apostrophe in words like they're OR I'll
    // For cases of hyphen in words like jury-men OR co-worker
    if ((c == '\'' || c == '-') && (previous >= 'a' && previous <= 'z')
        && ((next >= 'a' && next <= 'z'))) {
        return false;
    }

    return isspace(c) || isdigit(c) || ispunct(c) || isEscapeSequence(c);
}


/* 
 * NEW FUNCTION TO MERGE RESULTS FROM THREADS INTO MAIN DATA STRUCTURE
 * - New critical function -
 * Mutex solves race condition.
 */
void mergeThreadResults(int threadId) {
    // Start of merge results
    mtx.lock();
    WordCounter counter;
    counter.sortLists();

    for (int j = 0; j < counter.getSize(); j++) {
        std::string currentWord = counter.getWord(j);
        int countCorrectWord = counter.getCount(j);
        int index = wordCounter.contains(currentWord);

        if (index == -1) {
            wordCounter.addWordWithCount(currentWord, countCorrectWord);
        }
        else {
            wordCounter.addToCount(index, countCorrectWord);
        }
    }

    mtx.unlock();
    // End of merge results
}


void countWords(int startIndex, int endIndex, int threadId) {
    std::string line, word;
    WordCounter counter = WordCounter(32);

    for (int i = startIndex; i < endIndex; i++) {
        line = fileLines.getEntry(i);
        char previous = ' ', next;
        int position = -1;

        for (char c : line) {
            if (position >= line.size())
                next = ' ';
            else {
                next = line[position];
            }

            if (isIllegalCharacter(c, previous, next)) {
                if (!word.empty()) {
                    // START OF WORD COUNTING
                    int index = counter.contains(word);
                    if (index == -1) {
                        counter.addWord(word);
                    }
                    else {
                        counter.incrementCount(index);
                    }
                    // END OF WORD COUNTING

                    word = "";
                }
            }
            else {
                word += c;
            }
            previous = c;
            position++;
        }
        if (!word.empty()) {
            // START OF WORD COUNTING
            int index = counter.contains(word);
            if (index == -1) {
                counter.addWord(word);
            }
            else {
                counter.incrementCount(index);
            }
            // END OF WORD COUNTING

            word = "";
        }
    }

    counter.sortLists();

    // Start of merge results
    mtx.lock();

    for (int j = 0; j < counter.getSize(); j++) {
        std::string currentWord = counter.getWord(j);
        int index = wordCounter.contains(currentWord);

        if (index == -1) {
            wordCounter.addWordWithCount(currentWord, counter.getCount(j));
        }
        else {
            wordCounter.addToCount(index, counter.getCount(j));
        }
    }

    mtx.unlock();
    // End of merge results
}


/* Generates output.txt file displaying word and count */
static void generateOutput(const char* filename) {
    std::ofstream outFile(filename);

    if (!outFile.is_open()) {
        fprintf(stderr, "Error opening output file!\n");
    }

    for (int i = 0; i < wordCounter.getSize(); i++) {
        std::string spaceForFormat;

        if (wordCounter.getWord(i).length() >= 16)
            spaceForFormat = "\t";
        else if (wordCounter.getWord(i).length() >= 8) {
            spaceForFormat = "\t\t";
        }
        else {
            spaceForFormat = "\t\t\t";
        }

        outFile << wordCounter.getWord(i) << spaceForFormat << wordCounter.getCount(i) << std::endl;
    }

    outFile.close();
}

// Gets Process Information for Memory Usage
void getMemoryUsage() {
    PROCESS_MEMORY_COUNTERS pMemoryCounter;
    HANDLE myProcess = GetCurrentProcess();

    if (GetProcessMemoryInfo(myProcess, &pMemoryCounter, sizeof(pMemoryCounter))) {
        std::cout << "Memomy Consumption --> Peak working set size: " << pMemoryCounter.PeakWorkingSetSize / 1024 << " KB" << std::endl;
    }
    else {
        std::cerr << "Failed to get process memory information." << std::endl;
    }
}


int main()
{
    auto start = std::chrono::steady_clock::now();

    std::cout << "Number of threads = " << MAX_THREADS << std::endl << std::endl;

    std::cout << "Begin reading input file " << IN_FILENAME << "." << std::endl;
    readFile(IN_FILENAME);
    std::cout << "Finished reading file." << std::endl;

    int limit = fileLines.getSize() / MAX_THREADS;

    std::cout << "Begin getting word count results." << std::endl;
    if (MAX_THREADS == 1) {
        std::thread t1(countWords, 0, fileLines.getSize(), 0);
        t1.join();
    }
    else {
        std::thread threads[MAX_THREADS];
        int start = 0,
            linesPerThread = fileLines.getSize() / MAX_THREADS,
            end = 0;

        for (int i = 0; i < MAX_THREADS; i++) {
            if (start >= fileLines.getSize()) {
                break;
            }
            else if (end >= fileLines.getSize() || i == MAX_THREADS - 1) {
                threads[i] = std::thread(countWords, start, fileLines.getSize(), i);
                break;
            }
            else {
                end = start + linesPerThread;
                threads[i] = std::thread(countWords, start, end, i);
                start = end;
            }
        }

        for (int i = 0; i < MAX_THREADS; i++) {
            threads[i].join();
        }
    }
    std::cout << "Finish getting word count results." << std::endl;


    std::cout << "Begin sorting results." << std::endl;
    wordCounter.sortLists();
    std::cout << "Finished sorting results." << std::endl;

    std::cout << "Begin generating " << OUT_FILENAME << " file." << std::endl;
    generateOutput(OUT_FILENAME);
    std::cout << "Finished generating " << OUT_FILENAME << " file." << std::endl;
    std::cout << "\nProgram finished. Results can be found within the " << OUT_FILENAME << " file." << std::endl;

    int sum = 0;
    for (int i = 0; i < wordCounter.getSize(); i++) {
        sum += wordCounter.getCount(i);
    }

    std::cout << "\nTotal words in input file: " << sum << std::endl;
    std::cout << "Total unique words: " << wordCounter.getSize() << std::endl;

    auto end = std::chrono::steady_clock::now();
    auto elapsed = end - start;
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    std::cout << "\nProgram runtime: " << milliseconds << " milliseconds" << std::endl;

    getMemoryUsage();
}
