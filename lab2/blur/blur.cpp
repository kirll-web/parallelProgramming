#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <numeric>
#include <functional>
#include <chrono>
#include <iomanip>
#include <locale>

#include <tchar.h>
#include <Windows.h>

#include "bitmap.h"

using namespace std;
using namespace bmp;

using Threads = unique_ptr<HANDLE[]>;
using GetEllapsedTimeFn = function<chrono::duration<double>()>;

struct Args
{
    string inputFileName;
    string outputFileName;
    int threadsCount;
    int coresCount;
};

struct ProcessBitmapInfo
{
    Bitmap* image;
    unsigned lineNumber;
    size_t lineHeight;
};

class comma_numpunct : public numpunct<char>
{
protected:
    virtual char do_decimal_point() const
    {
        return ',';
    }
};


optional<Args> ParseArgs(int argc, char* argv[]);
DWORD WINAPI BlurBitmap(CONST LPVOID lpParam);
Threads CreateThreads(size_t count, function<ProcessBitmapInfo* (int)> dataCreatorFn);
void SetCoresLimit(size_t limit);
void SetNumbersDecimalPoint();
GetEllapsedTimeFn StartTimer();

int main(int argc, char* argv[])
{
    auto args = ParseArgs(argc, argv);

    if (!args)
    {
        cout << "Params format: <input file name> <output file name> <cores count> <threads count>" << endl;
        return -1;
    }

    try
    {
        SetCoresLimit(args->coresCount);
        SetNumbersDecimalPoint();
        auto getEllapsedTime = StartTimer();
        Bitmap* image = new Bitmap(args->inputFileName);
        unsigned lineHeight = image->height() / args->threadsCount;

        auto threads = CreateThreads(args->threadsCount, [lineHeight, image](unsigned threadNumber) {
            return new ProcessBitmapInfo{
                image,
                threadNumber,
                lineHeight,
            };
            });

        for (int i = 0; i < args->threadsCount; i++)
        {
            ResumeThread(threads[i]);
        }

        WaitForMultipleObjects(args->threadsCount, threads.get(), true, INFINITE);

        image->save(args->outputFileName);

        cout << getEllapsedTime().count() << endl;
    }
    catch (const bmp::Exception& e)
    {
        cout << "[BMP ERROR]: " << e.what() << endl;
        return 1;
    }

    return 0;
}

optional<Args> ParseArgs(int argc, char* argv[])
{
    if (argc < 5)
    {
        return nullopt;
    }

    Args result;

    result.inputFileName = argv[1];
    result.outputFileName = argv[2];

    try
    {
        result.coresCount = stoi(argv[3]);
        result.threadsCount = stoi(argv[4]);
    }
    catch (...)
    {
        return nullopt;
    }

    return result;
}


Threads CreateThreads(size_t count, function<ProcessBitmapInfo* (int)> dataCreatorFn)
{
    auto threads = make_unique<HANDLE[]>(count);

    for (unsigned i = 0; i < count; i++)
    {
        threads[i] = CreateThread(
            NULL, 0, &BlurBitmap, dataCreatorFn(i), CREATE_SUSPENDED, NULL
        );
    }

    return threads;
}

Pixel Average(vector<optional<Pixel>> const& v)
{
    auto const count = v.size();
    int sumR = 0;
    int sumG = 0;
    int sumB = 0;
    int pixelsCount = 0;

    for (auto const& pixel : v)
    {
        if (!pixel)
        {
            continue;
        }
        pixelsCount++;
        sumR += pixel->r;
        sumG += pixel->g;
        sumB += pixel->b;
    }

    return Pixel(sumR / pixelsCount, sumG / pixelsCount, sumB / pixelsCount);
}

Pixel GetAverageColor(Bitmap const& img, int x, int y)
{
    vector<optional<Pixel>> pixels = {
        img.get(x, y),
        img.get(x, y - 1),
        img.get(x, y + 1),
        img.get(x - 1, y),
        img.get(x - 1, y - 1),
        img.get(x - 1, y + 1),
        img.get(x + 1, y),
        img.get(x + 1, y - 1),
        img.get(x + 1, y + 1),
    };

    return Average(pixels);
}

DWORD WINAPI BlurBitmap(CONST LPVOID lpParam)
{
    auto data = reinterpret_cast<ProcessBitmapInfo*>(lpParam);
    
    unsigned startY = data->lineNumber * data->lineHeight;

    auto image = data->image;
    unsigned imageWidth = image->width();

    for (unsigned i = 0; i < 30; i++)
    {
        for (unsigned y = startY; y < startY + data->lineHeight; ++y)
        {
            for (unsigned x = 0; x < imageWidth; ++x)
            {
                image->set(x, y, GetAverageColor(*image, x, y));
            }
        }
    }

    delete data;
    ExitThread(0);
}

void SetNumbersDecimalPoint()
{
    locale comma_locale(locale(), new comma_numpunct());
    cout.imbue(comma_locale);
}

void SetCoresLimit(size_t limit)
{
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    size_t maxCoresCount = sysinfo.dwNumberOfProcessors;

    if (limit > maxCoresCount)
    {
        cout << "Max cores count is " << maxCoresCount << endl;
        limit = maxCoresCount;
    }

    auto procHandle = GetCurrentProcess();
    DWORD_PTR mask = static_cast<DWORD_PTR>((pow(2, maxCoresCount) - 1) / pow(2, maxCoresCount - limit));

    SetProcessAffinityMask(procHandle, mask);
}

GetEllapsedTimeFn StartTimer()
{
    chrono::steady_clock::time_point start = chrono::steady_clock::now();

    return [start]()
        {
            return chrono::steady_clock::now() - start;
        };
}