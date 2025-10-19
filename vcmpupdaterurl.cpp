#include <iostream>
#include <string>
#include <vector>
#include <windows.h>
#include <urlmon.h>
#include <shlobj.h>
#include <fstream>
#include <sstream>
#include <direct.h>

#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "shell32.lib")

struct VersionInfo {
    std::string version;
    std::string filename;
    std::string build;
};

class VCMPDownloader {
private:
    std::string baseUrl = "https://u04.thijn.ovh";
    std::string baseTargetPath;

public:
    VCMPDownloader() {
        // Get the base target path
        char appDataPath[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, appDataPath))) {
            baseTargetPath = std::string(appDataPath) + "\\Vice City Multiplayer\\";
        }
        else {
            baseTargetPath = "C:\\Users\\%username%\\AppData\\Local\\Vice City Multiplayer\\";
        }
    }

    bool directoryExists(const std::string& path) {
        DWORD attrib = GetFileAttributesA(path.c_str());
        return (attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_DIRECTORY));
    }

    bool createDirectory(const std::string& path) {
        // Create all directories in the path
        std::string current;
        for (size_t i = 0; i < path.length(); i++) {
            current += path[i];
            if (path[i] == '\\' || i == path.length() - 1) {
                if (!directoryExists(current) && current != "C:" && current != "C:\\") {
                    if (!CreateDirectoryA(current.c_str(), NULL)) {
                        if (GetLastError() != ERROR_ALREADY_EXISTS) {
                            return false;
                        }
                    }
                }
            }
        }
        return true;
    }

    std::string downloadPage(const std::string& url) {
        std::string tempFile = "temp_page.html";

        // Download the HTML page
        std::cout << "Downloading version information..." << std::endl;
        if (URLDownloadToFileA(NULL, url.c_str(), tempFile.c_str(), 0, NULL) != S_OK) {
            std::cout << "Failed to download page from: " << url << std::endl;
            return "";
        }

        // Read the downloaded file
        std::ifstream file(tempFile);
        if (!file.is_open()) {
            std::cout << "Failed to open temporary file" << std::endl;
            return "";
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        file.close();

        // Delete temporary file
        DeleteFileA(tempFile.c_str());

        return buffer.str();
    }

    std::vector<VersionInfo> parseVersions(const std::string& html) {
        std::vector<VersionInfo> versions;
        size_t pos = 0;

        while ((pos = html.find("Version <a href=\"/files/", pos)) != std::string::npos) {
            // Extract filename
            size_t href_start = html.find("href=\"", pos) + 6;
            size_t href_end = html.find("\"", href_start);
            if (href_end == std::string::npos) break;

            std::string filepath = html.substr(href_start, href_end - href_start);

            // Extract version
            size_t version_start = html.find(">", href_end) + 1;
            size_t version_end = html.find("<", version_start);
            if (version_end == std::string::npos) break;

            std::string version = html.substr(version_start, version_end - version_start);

            // Extract build from filename
            std::string filename = filepath.substr(filepath.find_last_of("/") + 1);
            std::string build = filename.substr(6, 8); // Extract build ID

            versions.push_back({ version, filename, build });

            pos = version_end;
        }

        return versions;
    }

    bool downloadFile(const std::string& filename) {
        std::string url = baseUrl + "/files/" + filename;
        std::string localPath = filename;

        std::cout << "Downloading: " << filename << std::endl;

        if (URLDownloadToFileA(NULL, url.c_str(), localPath.c_str(), 0, NULL) == S_OK) {
            std::cout << "Download completed: " << filename << std::endl;
            return true;
        }
        else {
            std::cout << "Download failed: " << filename << std::endl;
            return false;
        }
    }

    std::string cleanVersionName(const std::string& version) {
        std::string clean = version;
        // Replace characters that are not valid in folder names
        for (size_t i = 0; i < clean.length(); i++) {
            if (clean[i] == ':' || clean[i] == '\\' || clean[i] == '/' ||
                clean[i] == '*' || clean[i] == '?' || clean[i] == '"' ||
                clean[i] == '<' || clean[i] == '>' || clean[i] == '|') {
                clean[i] = '_';
            }
        }
        return clean;
    }

    bool extractWithWinRAR(const std::string& archivePath, const std::string& version) {
        // Common WinRAR installation paths
        const char* winRARPaths[] = {
            "C:\\Program Files\\WinRAR\\WinRAR.exe",
            "C:\\Program Files (x86)\\WinRAR\\WinRAR.exe",
            "C:\\Program Files\\WinRAR\\Rar.exe",
            "C:\\Program Files (x86)\\WinRAR\\Rar.exe"
        };

        std::string winRARExe;
        bool foundWinRAR = false;

        // Find WinRAR installation
        for (int i = 0; i < 4; i++) {
            if (GetFileAttributesA(winRARPaths[i]) != INVALID_FILE_ATTRIBUTES) {
                winRARExe = winRARPaths[i];
                foundWinRAR = true;
                std::cout << "Found WinRAR at: " << winRARExe << std::endl;
                break;
            }
        }

        if (!foundWinRAR) {
            std::cout << "WinRAR not found in standard locations!" << std::endl;
            return false;
        }

        // Create version-specific folder
        std::string cleanVersion = cleanVersionName(version);
        std::string versionTargetPath = baseTargetPath + cleanVersion + "\\";

        // Create the version directory
        if (!directoryExists(versionTargetPath)) {
            std::cout << "Creating version directory: " << versionTargetPath << std::endl;
            if (!createDirectory(versionTargetPath)) {
                std::cout << "Failed to create version directory!" << std::endl;
                return false;
            }
        }

        // Build WinRAR command
        std::string command = "\"" + winRARExe + "\" x -y -s -ibck \"" + archivePath + "\" \"" + versionTargetPath + "\"";

        std::cout << "Extracting to version folder: " << versionTargetPath << std::endl;
        std::cout << "Command: " << command << std::endl;

        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        if (CreateProcessA(NULL, (LPSTR)command.c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, INFINITE);

            DWORD exitCode;
            GetExitCodeProcess(pi.hProcess, &exitCode);

            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);

            if (exitCode == 0) {
                std::cout << "WinRAR extraction successful to version folder!" << std::endl;
                return true;
            }
            else {
                std::cout << "WinRAR extraction failed with exit code: " << exitCode << std::endl;
            }
        }
        else {
            std::cout << "Failed to start WinRAR process!" << std::endl;
        }

        return false;
    }

    bool extract7z(const std::string& archivePath, const std::string& version) {
        // Create version-specific folder
        std::string cleanVersion = cleanVersionName(version);
        std::string versionTargetPath = baseTargetPath + cleanVersion + "\\";

        // Create the version directory
        if (!directoryExists(versionTargetPath)) {
            std::cout << "Creating version directory: " << versionTargetPath << std::endl;
            if (!createDirectory(versionTargetPath)) {
                std::cout << "Failed to create version directory!" << std::endl;
                return false;
            }
        }

        // Use 7z.exe to extract
        std::string command = "7z x \"" + archivePath + "\" -o\"" + versionTargetPath + "\" -y";

        std::cout << "Extracting with 7-Zip to version folder: " << versionTargetPath << std::endl;

        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        if (CreateProcessA(NULL, (LPSTR)command.c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, INFINITE);

            DWORD exitCode;
            GetExitCodeProcess(pi.hProcess, &exitCode);

            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);

            if (exitCode == 0) {
                std::cout << "7-Zip extraction successful to version folder!" << std::endl;
                return true;
            }
        }

        std::cout << "7-Zip extraction failed!" << std::endl;
        return false;
    }

    void showVersions(const std::vector<VersionInfo>& versions) {
        std::cout << "\nAvailable VC:MP Versions:\n";
        std::cout << "==========================\n";

        for (size_t i = 0; i < versions.size(); i++) {
            std::cout << i + 1 << ". " << versions[i].version << " (Build: " << versions[i].build << ")\n";
        }
    }

    void run() {
        std::cout << "VC:MP Downloader and Installer\n";
        std::cout << "==============================\n\n";

        // Create base target directory if it doesn't exist
        if (!directoryExists(baseTargetPath)) {
            std::cout << "Creating base directory: " << baseTargetPath << std::endl;
            if (!createDirectory(baseTargetPath)) {
                std::cout << "Failed to create base directory: " << baseTargetPath << std::endl;
                return;
            }
        }

        // Download and parse the page
        std::string html = downloadPage(baseUrl);

        if (html.empty()) {
            std::cout << "Failed to fetch version information!" << std::endl;
            std::cout << "Press Enter to exit...";
            std::cin.ignore();
            std::cin.get();
            return;
        }

        std::vector<VersionInfo> versions = parseVersions(html);

        if (versions.empty()) {
            std::cout << "No versions found!" << std::endl;
            std::cout << "Press Enter to exit...";
            std::cin.ignore();
            std::cin.get();
            return;
        }

        // Show available versions
        showVersions(versions);

        // Let user choose version
        int choice;
        std::cout << "\nEnter the number of the version to download (1-" << versions.size() << "): ";
        std::cin >> choice;

        if (choice < 1 || choice > versions.size()) {
            std::cout << "Invalid choice!" << std::endl;
            std::cout << "Press Enter to exit...";
            std::cin.ignore();
            std::cin.get();
            return;
        }

        VersionInfo selected = versions[choice - 1];

        std::cout << "\nSelected: " << selected.version << std::endl;
        std::cout << "File: " << selected.filename << std::endl;
        std::cout << "Base Path: " << baseTargetPath << std::endl;

        // Download the file
        if (!downloadFile(selected.filename)) {
            std::cout << "Press Enter to exit...";
            std::cin.ignore();
            std::cin.get();
            return;
        }

        // Extract the file to version-specific folder
        std::cout << "\nExtracting files to version folder..." << std::endl;

        bool extracted = false;

        // Try WinRAR first
        extracted = extractWithWinRAR(selected.filename, selected.version);

        // If WinRAR fails, try 7-Zip
        if (!extracted) {
            std::cout << "Trying 7-Zip extraction..." << std::endl;
            extracted = extract7z(selected.filename, selected.version);
        }

        // Clean up downloaded archive
        if (extracted) {
            std::cout << "Cleaning up..." << std::endl;
            if (DeleteFileA(selected.filename.c_str())) {
                std::cout << "Temporary file removed." << std::endl;
            }
            std::cout << "Installation completed successfully!" << std::endl;
            std::cout << "Version installed to: " << baseTargetPath << cleanVersionName(selected.version) << "\\" << std::endl;
        }
        else {
            std::cout << "Installation failed! Make sure WinRAR or 7-Zip is installed." << std::endl;
            std::cout << "Downloaded file kept as: " << selected.filename << std::endl;
        }

        std::cout << "Press Enter to exit...";
        std::cin.ignore();
        std::cin.get();
    }
};

int main() {
    VCMPDownloader downloader;
    downloader.run();
    return 0;
}