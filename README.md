# ConcurrentHashtable

ConcurrentHashtable is a hashtable tested under the Linux platform, which provides the following features:

1. A C++ hashtable that can work under multi-thread mode.
2. Each element in the hashtable carries a timestamp, supporting expiration. It also supports a callback function to handle expired elements.
3. Supports C/C++98 standard. Our company still doesn't allow us to use C++11 even today in 2026, which is quite speechless.

This project has been tested under the C98 standard and g++ 4.8 compiler on Linux.

Compile steps:
1.1 cmake .
1.2 make

CMake needs to be installed on the server before use.

## Usage Examples
```c++
// Create
dt::Hashtable<unsigned long, unsigned long> g_table(1024, 0.75f);
```
Create a hashtable with 1024 elements and a load factor of 0.75.
Note: Exceeding 1024 will trigger a rehash and expand the space; it does not mean you cannot insert beyond 1024.
```c++
// Add
g_table.put(key, val);
// Remove
g_table.remove(key);
// Modify
g_table.put(1000000UL, 222222UL);
// Note: putting the same key will overwrite the original value, unlike STL (where insertion fails if the key exists)
// Find
unsigned long newValue = 0;
g_table.get(1000000UL, newValue);
```
Hashtable with expiration time
```c++
// Initialize the Timer singleton
dt::Timer::getInstance();
// Create a hashtable with expiration mechanism: check for expired keys every 2 seconds
dt::Hashtable<unsigned long, unsigned long> expiredTable(100, 0.75f, 2, onExpired);
// Expiration callback function (for the hashtable with expiration time)
static void onExpired(unsigned long &key)
{
    printf("[Expired cleanup] key=%lu has expired\n", key);
}
```



Enjoy!

## Bug Fixes
- Fixed a large number of printf debug messages in the original version
- Fixed the expiration callback issue in the original version
- Added a large number of usage examples
