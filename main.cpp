#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <future>

#include "include/cxxopts/cxxopts.hpp"

#if defined(__cplusplus) && __cplusplus >= 201703L && defined(__has_include)
#if __has_include(<filesystem>)
#define GHC_USE_STD_FS
#include <filesystem>
namespace fs = std::filesystem;
#endif
#endif
#ifndef GHC_USE_STD_FS
#include "include/ghc/filesystem.hpp"
namespace fs = ghc::filesystem;
#endif

std::vector<std::string> filesWithMatches{};
int matches{0};
std::mutex pushLock;

bool filetypeInString(const std::string_view sv, const std::vector<std::string>& filetypes)
{
    if (filetypes.empty())
        return true;

    bool typefound{false};
    for (const auto& type : filetypes)
    {
        if (sv.find(type) != std::string_view::npos)
            typefound = true;
    }
    return typefound;
}


std::vector<std::string> getFiles(const fs::path& path, bool recursive, const std::vector<std::string>& filetypes)
{
    std::vector<std::string> files;

    try
    {
        if (fs::exists(path) && !fs::is_directory(path) && filetypeInString(path.string(), filetypes))
            files.push_back(fs::absolute(path).string());

        if (fs::is_directory(path))
        {
            using iterator = fs::directory_iterator;
            for (iterator iter(path); iter != iterator(); ++iter)
            {
                if (!recursive)
                {
                    if (filetypeInString(iter->path().string(), filetypes))
                        files.push_back(fs::absolute(iter->path()).string());
                }
                else
                {
                    for (std::string& p : getFiles(iter->path(), true, filetypes))
                        if (filetypeInString(p, filetypes))
                            files.push_back(std::move(p));
                }
            }
        }
    }
    catch (const std::exception&) { std::cout << "Error reading the file!\n"; }

    return files;
}

bool getFileContent(const char* filename, std::string& rContents)
{
    std::ifstream in(filename, std::ios::in | std::ios::binary);
    if (in)
    {
        in.seekg(0, std::ios::end);
        rContents.resize(in.tellg());
        in.seekg(0, std::ios::beg);
        in.read(&rContents[0], rContents.size() + 1);
        in.close();
        return true;
    }
    return false;
}

void findPattern(const std::string& filepath, const std::string_view pattern)
{
    std::string contents;
    bool foundMatch{false};
    int numberOfMatchesInFile{0};
    if (getFileContent(filepath.c_str(), contents))
    {
        if (std::size_t pos = contents.find(pattern))
        {
            while (pos != std::string::npos)
            {
                matches++;
                numberOfMatchesInFile++;
                foundMatch = true;
                pos = contents.find(pattern, pos + pattern.size());
            }
        }
    }
    if (foundMatch)
    {
        std::lock_guard<std::mutex> lock(pushLock);
        if (numberOfMatchesInFile == 1)
        {
            filesWithMatches.emplace_back(filepath + " : " + std::to_string(numberOfMatchesInFile) + " match");
        }
        else
        {
            filesWithMatches.emplace_back(filepath + " : " + std::to_string(numberOfMatchesInFile) + " matches");

        }
    }
}


int main(int argc, char* argv[])
{
    bool recursiveMode{true};
    cxxopts::Options options("StringFinder", "");

    options.add_options()
            ("h,help", "List possible actions")
            ("p,path", "The search path", cxxopts::value<std::string>())
            ("s,string", "The string to be searched", cxxopts::value<std::string>())
            ("f,filetypes", "Targeted filetypes separated with comma",
             cxxopts::value<std::vector<std::string>>()->default_value(""))
            ("n,nonrec", "Disable resursive search")
            ("positional", "Positional arguments",
             cxxopts::value<std::vector<std::string>>()->default_value(""));

    options.parse_positional({"path", "string", "positional"});

    auto result = options.parse(argc, argv);

    auto& positional = result["positional"].as<std::vector<std::string>>();

    if (result.count("help"))
    {
        std::cout << options.help() << std::endl;
        std::cout << "Positional parameters: 1. path 2. string\n\n";

        std::cout << "Example 1: /home/user npm\n";
        std::cout << "Example 2: /home/user/downloads flower -f .txt,.md,.csv -n\n\n";
        exit(0);

    }
    else if (result.count("path") && result.count("string") || positional.size() == 2)
    {
        if (result.count("nonrec"))
            recursiveMode = false;

        std::string path;
        std::string pattern;
        if (result.count("path") && result.count("string"))
        {
            path = result["path"].as<std::string>();
            pattern = result["string"].as<std::string>();
        }
        else
        {
            path = positional[0];
            pattern = positional[1];
        }

        std::vector<std::string> files = getFiles(path, recursiveMode,
                                                  result["filetypes"].as<std::vector<std::string>>());

        for (auto e : files)
            std::cout << e << '\n';

        std::vector<std::future<void>> futures;
        futures.reserve(files.size());
        for (auto& filepaths : files)
        {
            futures.push_back(std::async(std::launch::async, findPattern, filepaths, pattern));
        }

        //print results
        if (files.size() == 1)
            std::cout << "\nSearched " << files.size() << " file.\n";
        else
            std::cout << "\nSearched " << files.size() << " files.\n";

        if (!filesWithMatches.empty())
        {
            std::cout << "\nSearch results:\n";
            for (auto& file : filesWithMatches)
                std::cout << file << '\n';

            if (filesWithMatches.size() == 1)
            {
                std::cout << "\nFound " << matches << " matches for \"" << pattern << "\" in " << filesWithMatches.size()
                          << " file.\n";
            }
            else
            {
                std::cout << "Found " << matches << " matches for \"" << pattern << "\" in " << filesWithMatches.size()
                          << " different files.\n";
            }
        }
        else
        {
            std::cout << "No matches found for \"" << pattern << "\".\n";
        }
    }
    else
    {
        std::cout << "Not valid arguments given. Please refer to --help if you're lost.\n";
    }
}

