// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2022 Julio Cesar Ziviani Alvarez

#pragma once

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#include "Userenv.h"
#pragma comment(lib, "userenv.lib")
#elif __linux__
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#else
#error Operating System not supported
#endif

#include "StringUtils.h"
#include "lib/Result.h"

namespace makeland {
    using namespace std;

    // fu = FileUtils
    namespace fu {
        static int __fu_mkdir(const string& path) {
#ifdef _WIN32
            return ::mkdir(path.data());
#elif __linux__
            return ::mkdir(path.data(), S_IRWXU);
#endif
        }

        Result list(vector<string>& files, const string& path) {
#ifdef _WIN32
            string searchPath = path + "/*.*";
            WIN32_FIND_DATAA fd;
            HANDLE hFind = ::FindFirstFileA(searchPath.c_str(), &fd);

            if (hFind == INVALID_HANDLE_VALUE) {
                if (GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_PATH_NOT_FOUND) {
                    return Result::ok();
                }
                return Result("could_not_list_files", "could not list files in path `{}`: {}", path, GetLastError());
            }
            do {
                if ((fd.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_ARCHIVE)) != 0 &&
                    (fd.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) == 0) {
                    string file = (char*)fd.cFileName;

                    if (file != "." && file != "..") {
                        files.push_back(file);
                    }
                }
            } while (::FindNextFileA(hFind, &fd));
            ::FindClose(hFind);
#elif __linux__
            DIR* dir = opendir(path.data());

            if (dir == NULL) {
                if (errno == ENOENT) {
                    return Result::ok();
                }
                return Result("could_not_list_files", "could not list files in path `{}`: {}", path, su::format("opendir(\"{}\") error {} ({})", path, errno, strerror(errno)));
            }
            dirent* item;

            while ((item = readdir(dir)) != NULL) {
                string file = item->d_name;

                if (file != "." && file != "..") {
                    files.push_back(file);
                }
            }
            closedir(dir);
#endif

            return Result::ok();
        }

        Result loadFromFile(string& output, const string& path) {
            FILE* file = fopen(path.data(), "rb");

            if (file == NULL) {
                return Result("could_not_open_file", "could not open file `{}`: error {} ({})", path, errno, strerror(errno));
            }
            if (fseek(file, 0, SEEK_END) != 0) {
                return Result("could_not_seek_file", "could not seek file `{}`: error {} ({})", path, errno, strerror(errno));
            }
            long size = ftell(file);

            if (size == -1) {
                return Result("could_not_tell_file", "could not tell file `{}`: error {} ({})", path, errno, strerror(errno));
            }
            rewind(file);
            unique_ptr<char[]> buffer(new char[(size_t)size]);

            if (fread(buffer.get(), 1, (size_t)size, file) != (size_t)size) {
                return Result("could_not_read_file", "could not read file `{}`: error {} ({})", path, errno, strerror(errno));
            }
            fclose(file);
            output = string(buffer.get(), (size_t)size);
            return Result::ok();
        }

        Result saveToFile(const string& path, const string& text) {
            FILE* file = fopen(path.data(), "wb");

            if (file == NULL) {
                return Result("could_not_open_file", "could not open file `{}`: error {} ({})", path, errno, strerror(errno));
            }
            if (fwrite(text.data(), 1, text.size(), file) != text.size()) {
                return Result("could_not_write_file", "could not write file `{}`: error {} ({})", path, errno, strerror(errno));
            }
            fclose(file);

            return Result::ok();
        }

        bool isDirectory(const string& path) {
            struct stat statbuf;

            if (stat(path.data(), &statbuf) != 0)
                return true;

            return (statbuf.st_mode & S_IFDIR);
        }

        bool isFile(const string& path) {
            struct stat statbuf;

            if (stat(path.data(), &statbuf) != 0)
                return true;

            return (statbuf.st_mode & S_IFREG);
        }

        bool exists(const string& path) {
            struct stat sTemp;
            return stat(path.c_str(), &sTemp) == 0;
        }

        Result mkdir(const string& path, bool recursive) {
            if (recursive) {
                vector<string> paths = su::split(path, '/', true);
                string curPath = paths[0];

                for (size_t n = 1; n < paths.size(); n++) {
                    if (paths[n].size() > 0) {
                        curPath += "/" + paths[n];

                        if (__fu_mkdir(curPath) != 0) {
                            if (errno != EEXIST) {
                                return Result("could_not_create_dir", "could not create dir `{}`: error {} ({})", path, errno, strerror(errno));
                            }
                        }
                    }
                }
                return Result::ok();
            }
            if (__fu_mkdir(path) != 0) {
                if (errno != EEXIST) {
                    return Result("could_not_create_dir", "could not create dir `{}`: error {} ({})", path, errno, strerror(errno));
                }
            }
            return Result::ok();
        }

        string getFileName(const string& path) {
            vector<string> v = su::split(su::replaceAll(path, "\\", "/"), '/', false);
            return v[v.size() - 1];
        }

        Result deleteFile(const string& file) {
            if (!exists(file)) {
                return Result::ok();
            }
            if (!isFile(file)) {
                return Result("is_not_file", "path `{}` is not a file", file);
            }
            if (::remove(file.data()) == -1) {
                return Result("could_not_delete_file", "could not delete file `{}`: error {} ({})", file, errno, strerror(errno));
            }
            return Result::ok();
        }

        Result lastChange(const string& file, uint64_t& timestampSeconds) {
            struct stat buf;

            if (stat(file.data(), &buf) == -1) {
                return Result("could_not_read_last_change", "could not read last change in file `{}`: error {} ({})", file, errno, strerror(errno));
            }
            timestampSeconds = (uint64_t)buf.st_mtime;

            return Result::ok();
        }

        Result getCwd(string& path) {
#ifdef _WIN32
            char* cwd;

            if ((cwd = _getcwd(NULL, 0)) == NULL) {
                return Result("could_not_get_cwd", "could get cwd `{}`: error {} ({})", errno, strerror(errno));
            }
            path = cwd;
            return Result::ok();
#elif __linux__
            char cwd[PATH_MAX];

            if (getcwd(cwd, sizeof(cwd)) == NULL) {
                return Result("could_not_get_cwd", "could get cwd `{}`: error {} ({})", errno, strerror(errno));
            }
            path = cwd;
            return Result::ok();
#endif
        }

        string normalize(const string& path) {
#ifdef _WIN32
            return su::replaceAll(path, "/", "\\");
#elif __linux__
            return su::replaceAll(path, "\\", "/");
#endif
        }
    }
}