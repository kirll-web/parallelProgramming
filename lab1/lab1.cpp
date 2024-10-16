
#include <windows.h>
#include <string>
#include <iostream>
#include <tchar.h>
#include <mutex>
using namespace std;

static mutex mtx;

DWORD WINAPI ThreadProc(CONST LPVOID lpParam)
{
	mtx.lock();
	int numberThread = *static_cast<int*>(lpParam);
	cout << "Working thread " << numberThread << endl;
	mtx.unlock();
	ExitThread(0); // функция устанавливает код завершения потока в 0
}

int _tmain(int argc, _TCHAR* argv[])
{
	int n = 0;
	cout << "Input amount thread: ";
	cin >> n;

	// создание двух потоков
	HANDLE* handles = new HANDLE[n];
	for (int i = 0; i < n; i++) {
		int* numberThread = new int(i + 1);
		handles[i] = CreateThread(NULL, 0, &ThreadProc, &i, CREATE_SUSPENDED, NULL);
	}

	for (int i = 0; i < n; i++) {
		ResumeThread(handles[i]);
	}

	// ожидание окончания работы двух потоков
	WaitForMultipleObjects(n, handles, true, INFINITE);

	for (int i = 0; i < n; i++) {
		CloseHandle(handles[i]);
	}

	return 0;
}

