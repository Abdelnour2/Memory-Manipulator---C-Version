#include <stdio.h>
#include <windows.h>
#include <string.h>
#include <tlHelp32.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>

typedef struct {
    uintptr_t *addrs;
    size_t count;
    size_t capacity;
} AddrList;

char* GetNameOfAProgram(char* nameOfTheProgram, int size);
DWORD GetProcessIdOfAProgram(char* ProgramName);
HANDLE OpenAProgrambyId(DWORD programId);
void ReadFromASpecificMemory(HANDLE process);
void WriteToASpecificMemory(HANDLE process);

void addrlist_init(AddrList *l);
void addrlist_free(AddrList *l);
int addrlist_push(AddrList *l, uintptr_t a);
AddrList initial_scan_int_aligned(HANDLE hProcess, int target);
AddrList next_scan_int(HANDLE hProcess, const AddrList *prev, int newValue);
void print_results(const AddrList *l, size_t maxToShow);

int main() {
    char nameOfTheProgram[100];
    GetNameOfAProgram(nameOfTheProgram, sizeof(nameOfTheProgram));
    DWORD processId = GetProcessIdOfAProgram(nameOfTheProgram);

    printf("Process ID: %d\n\n", processId);

    HANDLE processToOpen = OpenAProgrambyId(processId);

    if (processToOpen == NULL) {
        printf("Couldn't open the process\n");
    } else {
        printf("Access Gained\n");

        bool isRunning = true;
        char userChoice = '\0';

        while (isRunning)
        {
            printf("What Do You Want to Do:\n");
            printf("1. Read Specific Memory\n");
            printf("2. Search Memory for a Specific Value\n");
            printf("3. Write to a Specific Memory\n");
            printf("4. Close\n");

            printf("Enter your choice: ");
            scanf(" %c", &userChoice);

            switch (userChoice) {
                case '1':
                    ReadFromASpecificMemory(processToOpen);
                    break;
                case '2':
                    int targetValue = 0;
                    printf("Enter the initial value to search for: ");
                    if (scanf("%d", &targetValue) != 1) {
                        printf("Invalid input.\n\n");
                        break;
                    }

                    printf("Scanning memory for value %d...\n", targetValue);
                    AddrList results = initial_scan_int_aligned(processToOpen, targetValue);
                    print_results(&results, 20); // show only first 20

                    if (results.count == 0) {
                        printf("No matching values found.\n\n");
                        addrlist_free(&results);
                        break;
                    }

                    // Next-scan refinement loop
                    while (1) {
                        printf("\nEnter the next value to filter (-1 to stop scanning): ");
                        int newValue = 0;
                        if (scanf("%d", &newValue) != 1) {
                            printf("Invalid input.\n");
                            // Clear invalid input
                            while (getchar() != '\n');
                            continue;
                        }

                        if (newValue == -1) {
                            printf("Exiting scan mode...\n\n");
                            break;
                        }

                        AddrList filtered = next_scan_int(processToOpen, &results, newValue);
                        addrlist_free(&results);
                        results = filtered;
                        print_results(&results, 20);
                    }

                    addrlist_free(&results);
                    break;
                case '3':
                    WriteToASpecificMemory(processToOpen);
                    break;
                case '4':
                    printf("Process Closed\n");
                    isRunning = false;
                    break;
                default:
                    printf("Invalid Input!\n\n");
                    break;
            }
        }
        
        CloseHandle(processToOpen);
    }

    return 0;
}

char* GetNameOfAProgram(char* nameOfTheProgram, int size) {
    printf("Please enter the name of a running program: ");
    fgets(nameOfTheProgram, size, stdin);
    nameOfTheProgram[strlen(nameOfTheProgram) - 1] = '\0';
}

DWORD GetProcessIdOfAProgram(char* ProgramName) {
    // Step 1: Take a snapshot of all processes
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    
    // Step 2: Checking for failure
    if (snapshot == INVALID_HANDLE_VALUE) {
        printf("Couldn't get a snapshot\n");
        return 0;
    }

    // Step 3: Looping through the snapshot
    // first we need to create an object of the struct that holds the information of the process
    PROCESSENTRY32 processEntry;
    
    // then we need to define its size, it's size is basically the size of the struct
    processEntry.dwSize = sizeof(PROCESSENTRY32);

    // Now to start we need to take our first process from the snapshot and put it in the struct object
    // We put it inside an if in case there is any errors so we skip the loop entirely
    if (Process32First(snapshot, &processEntry)) {
        // now start the loop, we need a do while since we already got the first process
        do {
            // using _stricmp to test the names
            if (_stricmp(processEntry.szExeFile, ProgramName) == 0) {
                // Get the id of that process
                DWORD pid = processEntry.th32ProcessID;

                // closw the snapshot
                CloseHandle(snapshot);

                // return the process id
                return pid;
            }
        } while (Process32Next(snapshot, &processEntry)); // while there is a next snapshot to test
    }

    // If we got nothing, we need to close the snapshot anyways and return 0
    CloseHandle(snapshot);
    return 0;
}

HANDLE OpenAProgrambyId(DWORD programId) {
    DWORD access = PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION;
    HANDLE processToOpen = OpenProcess(access, FALSE, programId);
    return processToOpen;
}

void ReadFromASpecificMemory(HANDLE process) {
    printf("Enter the memory address: ");
    uintptr_t temp_address_value;

    if (scanf("%llx", &temp_address_value) == 1) {
        LPCVOID targetAddress = (LPCVOID)temp_address_value;

        int data = 0;
        SIZE_T bytesRead;

        bool success = ReadProcessMemory(process, targetAddress, &data, sizeof(data), &bytesRead);
        
        if (success) {
            printf("Read %zu bytes. Value: %d\n\n", bytesRead, data);
        } else {
            printf("Failed: %lu\n\n", GetLastError());
        }
    } else {
        printf("Invalid Input\n\n");
    }
}

void WriteToASpecificMemory(HANDLE process) {
    printf("Enter the memory address: ");
    uintptr_t temp_address_value;

    if (scanf("%llx", &temp_address_value) != 1) {
        printf("Invalid address input\n\n");
        return;
    }

    printf("Enter the new value: ");
    int newValue = 0;
    
    if (scanf("%d", &newValue) != 1) {
        printf("Invalid value input\n\n");
        return;
    }

    LPVOID targetAddress = (LPVOID)temp_address_value;
    SIZE_T bytesWritten = 0;

    bool success = WriteProcessMemory(process, targetAddress, &newValue, sizeof(newValue), &bytesWritten);

    if (success) {
        printf("Wrote %zu bytes. New value: %d\n\n", bytesWritten, newValue);
    } else {
        printf("Failed to write memory. Error: %lu\n\n", GetLastError());
    }
}

void addrlist_init(AddrList *l) {
    l->addrs = NULL; l->count = 0; l->capacity = 0;
}

void addrlist_free(AddrList *l) {
    if (l->addrs) free(l->addrs);
    l->addrs = NULL; l->count = l->capacity = 0;
}

int addrlist_push(AddrList *l, uintptr_t a) {
    if (l->count + 1 > l->capacity) {
        size_t newcap = (l->capacity == 0) ? 1024 : l->capacity * 2;
        uintptr_t *tmp = (uintptr_t*)realloc(l->addrs, newcap * sizeof(uintptr_t));
        if (!tmp) return 0;
        l->addrs = tmp;
        l->capacity = newcap;
    }
    l->addrs[l->count++] = a;
    return 1;
}

// Aligned initial scan for 32-bit int values (step by 4 bytes).
// Returns a populated AddrList (caller must free using addrlist_free).
AddrList initial_scan_int_aligned(HANDLE hProcess, int target) {
    AddrList out;
    addrlist_init(&out);

    SYSTEM_INFO si;
    GetSystemInfo(&si);

    uintptr_t current = (uintptr_t)si.lpMinimumApplicationAddress;
    uintptr_t maxAddr = (uintptr_t)si.lpMaximumApplicationAddress;
    MEMORY_BASIC_INFORMATION mbi;
    const SIZE_T chunkLimit = 1 << 20; // 1 MB chunks for reading

    while (current < maxAddr) {
        SIZE_T q = VirtualQueryEx(hProcess, (LPCVOID)current, &mbi, sizeof(mbi));
        if (q == 0) break;

        // Only committed pages and readable/writable pages
        if (mbi.State == MEM_COMMIT) {
            DWORD prot = mbi.Protect & ~(PAGE_GUARD | PAGE_NOCACHE | PAGE_WRITECOMBINE);
            int readableWritable = 0;
            if (prot == PAGE_READWRITE || prot == PAGE_WRITECOPY ||
                prot == PAGE_EXECUTE_READWRITE || prot == PAGE_EXECUTE_WRITECOPY) {
                readableWritable = 1;
            }
            if (readableWritable) {
                uintptr_t regionBase = (uintptr_t)mbi.BaseAddress;
                SIZE_T regionSize = mbi.RegionSize;
                SIZE_T offset = 0;

                while (offset < regionSize) {
                    SIZE_T toRead = regionSize - offset;
                    if (toRead > chunkLimit) toRead = chunkLimit;

                    unsigned char *buffer = (unsigned char*)malloc(toRead);
                    if (!buffer) break;

                    SIZE_T bytesRead = 0;
                    if (ReadProcessMemory(hProcess, (LPCVOID)(regionBase + offset),
                                          buffer, toRead, &bytesRead) && bytesRead >= sizeof(int)) {

                        // Walk aligned by sizeof(int) to avoid overlapping matches
                        for (SIZE_T i = 0; i + sizeof(int) <= bytesRead; i += sizeof(int)) {
                            int v;
                            memcpy(&v, buffer + i, sizeof(int));
                            if (v == target) {
                                uintptr_t found = regionBase + offset + i;
                                addrlist_push(&out, found);
                            }
                        }
                    }
                    free(buffer);
                    offset += toRead;
                }
            }
        }

        // move to the next region
        current = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
    }

    return out;
}

// next_scan: filter an existing AddrList using a new value.
// Returns a new AddrList (caller must free both lists appropriately).
AddrList next_scan_int(HANDLE hProcess, const AddrList *prev, int newValue) {
    AddrList out;
    addrlist_init(&out);

    for (size_t i = 0; i < prev->count; ++i) {
        uintptr_t a = prev->addrs[i];
        int v = 0;
        SIZE_T bytesRead = 0;
        if (ReadProcessMemory(hProcess, (LPCVOID)a, &v, sizeof(v), &bytesRead) && bytesRead == sizeof(v)) {
            if (v == newValue) {
                addrlist_push(&out, a);
            }
        }
    }

    return out;
}

// helper to print a page of results and total
void print_results(const AddrList *l, size_t maxToShow) {
    printf("Total matches: %zu\n", l->count);
    size_t toShow = (l->count < maxToShow) ? l->count : maxToShow;
    for (size_t i = 0; i < toShow; ++i) {
        printf("  [%zu] 0x%p\n", i, (void*)l->addrs[i]);
    }
    if (l->count > toShow) {
        printf("  ... %zu more not shown\n", l->count - toShow);
    }
}