#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <stdbool.h>

// Function prototypes
void printFolderStructure(const char *path, int maxDepth, int currentDepth, const char *indentation, bool showFiles, const char *excludePattern, FILE *output);
bool matchesPattern(const char *string, const char *pattern);
void printUsage(void);
char* getLastPathComponent(const char *path);

int main(int argc, char *argv[]) {
    char rootPath[MAX_PATH] = ".";
    char resolvedPath[MAX_PATH];
    int maxDepth = -1;
    bool showFiles = true;
    char excludePattern[256] = "";
    char outputFile[MAX_PATH] = "";
    FILE *output = stdout;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "-rp") == 0 || strcmp(argv[i], "-rootpath") == 0 || strcmp(argv[i], "-rootPath") == 0) && i + 1 < argc) {
            strcpy(rootPath, argv[++i]);
        } else if (strcmp(argv[i], "-maxDepth") == 0 && i + 1 < argc) {
            maxDepth = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-showFiles") == 0 && i + 1 < argc) {
            if (strcmp(argv[++i], "false") == 0 || strcmp(argv[i], "0") == 0) {
                showFiles = false;
            }
        } else if (strcmp(argv[i], "-excludePattern") == 0 && i + 1 < argc) {
            strcpy(excludePattern, argv[++i]);
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            strcpy(outputFile, argv[++i]);
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printUsage();
            return 0;
        }
    }
    
    // Open output file if specified
    if (outputFile[0] != '\0') {
        output = fopen(outputFile, "w");
        if (output == NULL) {
            fprintf(stderr, "Error opening output file: %s\n", outputFile);
            return 1;
        }
    }
    
    // Resolve the path to handle relative paths like ".." or drive roots
    if (GetFullPathName(rootPath, MAX_PATH, resolvedPath, NULL) == 0) {
        fprintf(stderr, "Error resolving path: %s\n", rootPath);
        return 1;
    }
    
    // Display the folder name
    char *folderName = getLastPathComponent(rootPath);
    
    fprintf(output, "📁 %s\n", folderName);
    // Display the folder structure
    printFolderStructure(rootPath, maxDepth, 0, "", showFiles, excludePattern, output);
    
    // Print usage examples to stdout always
    if (output != stdout) {
        fclose(output);
    }
    
    // Only print usage to console if we're not writing to a file
    if (outputFile[0] == '\0') {
        printUsage();
    }
    
    return 0;
}

char* getLastPathComponent(const char *path) {
    static char result[MAX_PATH];
    
    // Handle root paths like "C:\" or "D:\"
    if ((strlen(path) == 2 && path[1] == ':') || 
        (strlen(path) == 3 && path[1] == ':' && (path[2] == '\\' || path[2] == '/'))) {
        strcpy(result, path);
        return result;
    }
    
    // Handle current directory "." or parent directory ".."
    if (strcmp(path, ".") == 0) {
        char currentDir[MAX_PATH];
        GetCurrentDirectory(MAX_PATH, currentDir);
        return getLastPathComponent(currentDir);
    } else if (strcmp(path, "..") == 0 || strcmp(path, "../") == 0 || strcmp(path, "..\\") == 0) {
        char currentDir[MAX_PATH];
        char parentDir[MAX_PATH];
        GetCurrentDirectory(MAX_PATH, currentDir);
        
        // Get parent directory path
        char* lastSlash = strrchr(currentDir, '\\');
        if (lastSlash) {
            *lastSlash = '\0';  // Truncate at last backslash
            // If we're at root level (e.g., C:\), keep the backslash
            if (strlen(currentDir) == 2 && currentDir[1] == ':') {
                strcat(currentDir, "\\");
            }
        }
        
        return getLastPathComponent(currentDir);
    }
    
    // Remove trailing slashes
    char cleanPath[MAX_PATH];
    strcpy(cleanPath, path);
    size_t len = strlen(cleanPath);
    if (len > 0 && (cleanPath[len-1] == '\\' || cleanPath[len-1] == '/')) {
        cleanPath[len-1] = '\0';
    }
    
    // Find the last backslash or forward slash
    const char *lastSlash = strrchr(cleanPath, '\\');
    if (!lastSlash) {
        lastSlash = strrchr(cleanPath, '/');
    }
    
    if (lastSlash) {
        strcpy(result, lastSlash + 1);
    } else {
        strcpy(result, cleanPath);
    }
    
    return result;
}

void printFolderStructure(const char *path, int maxDepth, int currentDepth, const char *indentation, bool showFiles, const char *excludePattern, FILE *output) {
    char searchPath[MAX_PATH];
    char resolvedPath[MAX_PATH];
    WIN32_FIND_DATA findData;
    HANDLE hFind;
    
    // Convert relative path to absolute if needed
    if (GetFullPathName(path, MAX_PATH, resolvedPath, NULL) == 0) {
        fprintf(output, "Error resolving path: %s\n", path);
        return;
    }
    
    // Build the search path
    sprintf(searchPath, "%s\\*", resolvedPath);
    
    // Start finding files/directories
    hFind = FindFirstFile(searchPath, &findData);
    if (hFind == INVALID_HANDLE_VALUE) {
        fprintf(output, "Error opening directory: %s\n", resolvedPath);
        return;
    }
    
    // Create arrays to store directories and files separately
    char directoryNames[1024][MAX_PATH];
    char fileNames[1024][MAX_PATH];
    int directoryCount = 0;
    int fileCount = 0;
    
    // Process all files and directories
    do {
        // Skip "." and ".." directories
        if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0) {
            continue;
        }
        
        // Skip if it matches exclude pattern
        if (excludePattern[0] != '\0' && matchesPattern(findData.cFileName, excludePattern)) {
            continue;
        }
        
        // Check if it's a directory
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            strcpy(directoryNames[directoryCount++], findData.cFileName);
        } else if (showFiles) {
            strcpy(fileNames[fileCount++], findData.cFileName);
        }
    } while (FindNextFile(hFind, &findData));
    
    FindClose(hFind);
    
    // Process directories first
    for (int i = 0; i < directoryCount; i++) {
        bool isLastDir = (i == directoryCount - 1) && (fileCount == 0);
        
        if (isLastDir) {
            fprintf(output, "%s╰── 📁 %s\n", indentation, directoryNames[i]);
        } else {
            fprintf(output, "%s├── 📁 %s\n", indentation, directoryNames[i]);
        }
        
        // Recursively process subdirectories if not at max depth
        if (maxDepth == -1 || currentDepth < maxDepth) {
            char newPath[MAX_PATH];
            char newIndentation[1024];
            
            sprintf(newPath, "%s\\%s", path, directoryNames[i]);
            
            if (isLastDir) {
                sprintf(newIndentation, "%s    ", indentation);
            } else {
                sprintf(newIndentation, "%s│   ", indentation);
            }
            
            printFolderStructure(newPath, maxDepth, currentDepth + 1, newIndentation, showFiles, excludePattern, output);
        }
    }
    
    // Then process files
    for (int i = 0; i < fileCount; i++) {
        if (i == fileCount - 1) {
            fprintf(output, "%s╰── 📄 %s\n", indentation, fileNames[i]);
        } else {
            fprintf(output, "%s├── 📄 %s\n", indentation, fileNames[i]);
        }
    }
}

// Simple pattern matching function for exclude patterns
bool matchesPattern(const char *string, const char *pattern) {
    // Very simple implementation - just check if pattern appears in string
    // For a real implementation, you'd need a proper regex library
    return strstr(string, pattern) != NULL;
}

void printUsage(void) {
    printf("\n╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                      Usage Examples                          ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    printf("folder_structure.exe -rootPath \"C:\\YourFolder\"\n");
    printf("folder_structure.exe -rootPath \"C:\\YourFolder\" -maxDepth 2\n");
    printf("folder_structure.exe -rootPath \"C:\\YourFolder\" -showFiles false\n");
    printf("folder_structure.exe -rootPath \"C:\\YourFolder\" -excludePattern \".git\"\n");
    printf("folder_structure.exe -rootPath \"C:\\YourFolder\" -o \"output.txt\"\n");
}