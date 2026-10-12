#pragma once


#ifdef _WIN32
	#include <Winsock2.h>
	#include <ws2tcpip.h>
	#include <windows.h>
	#include <wininet.h>
	#include <process.h>
	#include <rpc.h>
	#include <Objbase.h>
	#include <bcrypt.h>

	#pragma comment(lib, "bcrypt.lib")
#else
	#include <stdio.h>
	#include <dirent.h>
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <fcntl.h>
	#include <unistd.h>
	#include <time.h>
	#include <wchar.h>
	#include <pthread.h>
	#include <mutex>
	#include <sys/ioctl.h>
	#include <sys/time.h>
	#include <sys/ioctl.h>
	#include <sys/types.h>
	#include <netdb.h>
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
//	#include <uuid/uuid.h>
#endif

#include <stdint.h>
#include <memory>
#include <vector>
#include <map>
#include <set>
#include <list>
#include <set>
#include <sstream>
#include <time.h>
#include <functional>
#include <queue>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <iterator>
#include <cstring>
#include <stack>
#include <algorithm>
#include <string>
#include <cmath>
#include <cfloat>
#include <climits>
#include <cstdlib>
#include <stdexcept>



typedef uint8_t byte_t;

#ifndef _WIN32
	// On Windows these come from the SDK (rpcndr.h) or are not used by the Windows code path;
	// POSIX/Android code and some shared templates still rely on them.
	typedef unsigned char byte;
	typedef std::string astr;
	typedef std::wstring wstr;
	typedef std::vector<std::string> astrvec;
	typedef std::vector<std::wstring> wstrvec;

	// <windows.h> provides global min/max macros which the code relies on
	using std::min;
	using std::max;
#endif



#ifdef _WIN32
	typedef int thread_id_t;
#else
	typedef pthread_t thread_id_t;
    	typedef struct tagSIZE
	{
		long        cx;
		long        cy;
	} SIZE, *PSIZE, *LPSIZE;
#endif