#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        printf("Niste zadali putanju do direktorija u ulaznoj datoteci!\n");
        return 1;
    }

    LPCWSTR dir_path = NULL;
    if (argc > 1)
    {
        int size = MultiByteToWideChar(CP_UTF8, 0, argv[1], -1, NULL, 0);
        if (size > 0)
        {
            LPWSTR wide_path = new wchar_t[size];
            int check = MultiByteToWideChar(CP_UTF8, 0, argv[1], -1, wide_path, size);
            if (check > 0)
            {
                dir_path = wide_path;
            }
            else
            {
                printf("Pogreška pri pretvaranju ulazne putanje u široke znakove. Greška %ld", GetLastError());
                return 1;
            }
        }
        else
        {
            printf("Pogreška pri izraèunavanju velièine ulazne putanje u širokim znakovima. Greška %ld", GetLastError());
            return 1;
        }
    }
    else
    {
        printf("Nije navedena ulazna putanja.");
        return 1;
    }


    if (argc < 3)
    {
        printf("Niste zadali vremenski period T u ulaznoj datoteci!\n");
        return 1;
    }

    int time_period = atoi(argv[2]);

    HANDLE timer = CreateWaitableTimer(NULL, FALSE, NULL);

    if (timer == NULL)
    {
        printf("Neuspjeh pri stvaranju tajmera. Greska %ld\n", GetLastError());
        return 1;
    }

    LARGE_INTEGER due_time;
    due_time.QuadPart = -300000000; // 30min u nanosekundama

    if (!SetWaitableTimer(timer, &due_time, 1800000, NULL, NULL, FALSE))
    {
        printf("Neuspjeh pri postavljanju tajmera. Greska %ld\n", GetLastError());
        return 1;
    }

    HANDLE change_handle = FindFirstChangeNotification(
        dir_path,
        FALSE,
        FILE_NOTIFY_CHANGE_LAST_WRITE);

    if (change_handle == INVALID_HANDLE_VALUE)
    {
        printf("Neuspjeh pri stvaranju handle-a za promjenu direktorija. Greska %ld\n", GetLastError());
        return 1;
    }

    FILETIME current_time;
    SYSTEMTIME system_time;
    GetSystemTime(&system_time);

    while (TRUE)
    {
        if (WaitForSingleObject(timer, INFINITE) != WAIT_OBJECT_0)
        {
            printf("Neuspjeh pri èekanju na tajmer. Greska %ld\n", GetLastError());
            break;
        }

        if (!SystemTimeToFileTime(&system_time, &current_time))
        {
            printf("Neuspjeh pri konvertiranju vremena. Greska %ld\n", GetLastError());
            break;
        }

        int deleted_files = 0;

        HANDLE dir_handle = CreateFile(
            dir_path,
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS,
            NULL);

         if (dir_handle == INVALID_HANDLE_VALUE)
        {
            printf("Neuspjeh pri stvaranju handle-a za direktorij. Greska %ld\n", GetLastError());
            break;
        }
        printf("Uspjesno kreiran handle za direktorij\n");

        WIN32_FIND_DATA find_data;
        HANDLE file_handle = FindFirstFile(dir_path, &find_data);
        
        if (file_handle == INVALID_HANDLE_VALUE)
        {
            printf("Neuspjeh pri pronalasku prvog fajla u direktoriju. Greska %ld\n", GetLastError());
            break;
        }

        printf("Uspjesno pronaden fajl u direktoriju\n");

        do
        {
            if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                continue;
            }

            FILETIME file_time = find_data.ftLastWriteTime;
            ULARGE_INTEGER large_int;
            large_int.LowPart = file_time.dwLowDateTime;
            large_int.HighPart = file_time.dwHighDateTime;
            ULONGLONG file_time_ticks = large_int.QuadPart;

            FILETIME difference_time;
            ULARGE_INTEGER difference_int; 
            LARGE_INTEGER current_time_int, file_time_ticks_int;
            memcpy(&current_time_int, &current_time, sizeof(current_time));
            memcpy(&file_time_ticks_int, &file_time_ticks, sizeof(file_time_ticks));
            difference_int.QuadPart = current_time_int.QuadPart - file_time_ticks_int.QuadPart;

            difference_time.dwLowDateTime = difference_int.LowPart;
            difference_time.dwHighDateTime = difference_int.HighPart;

            if (difference_time.dwHighDateTime != 0)
            {
                continue;
            }

            DWORD time_difference = difference_time.dwLowDateTime / 10000000;

            if (time_difference >= time_period)
            {
                WCHAR file_path[MAX_PATH];
                _snwprintf_s(file_path, MAX_PATH, _TRUNCATE, L"%s\\%s", dir_path, find_data.cFileName);

                if (DeleteFile(file_path))
                {
                    deleted_files++;
                }
            }
        } while (FindNextFile(file_handle, &find_data) != 0);

        if (GetLastError() != ERROR_NO_MORE_FILES)
        {
            printf("Neuspjeh pri pronalasku fajlova u direktoriju. Greska %ld\n", GetLastError());
            break;
        }

        FindClose(file_handle);

        if (!CloseHandle(dir_handle))
        {
            printf("Neuspjeh pri zatvaranju handle-a za direktorij. Greska %ld\n", GetLastError());
            break;
        }

        if (!FindNextChangeNotification(change_handle))
        {
            printf("Neuspjeh pri promjeni direktorija. Greska %ld\n", GetLastError());
            break;
        }

        SYSTEMTIME local_time;
        GetLocalTime(&local_time);

        char output_str[MAX_PATH];
        snprintf(output_str, MAX_PATH, "%04d-%02d-%02d %02d:%02d:%02d - %d\n",
            local_time.wYear,
            local_time.wMonth,
            local_time.wDay,
            local_time.wHour,
            local_time.wMinute,
            local_time.wSecond,
            deleted_files);

        FILE* out_file = fopen("out.txt", "a");

        if (out_file == NULL)
        {
            printf("Neuspjeh pri otvaranju datoteke za pisanje. Greska %ld\n", GetLastError());
            break;
        }

        fprintf(out_file, "%s", output_str);
        fclose(out_file);

        if (!SetWaitableTimer(timer, &due_time, 1800000, NULL, NULL, FALSE))
        {
            printf("Neuspjeh pri postavljanju tajmera. Greska %ld\n", GetLastError());
            return 1;
        }
    }

    FindCloseChangeNotification(change_handle);
    printf("Uspjesno odradeno runnanje");
    return 0;
}
