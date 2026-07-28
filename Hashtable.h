#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <cstddef>
#include <utility>
#include <vector>
#include <stdio.h>
#include <malloc.h>

#include "Common.h"
#include "Threads.h"
#include "HashNode.h"

namespace dt { 
         
    const int defaultCapacity = 100;
    const float defaultLoadFactor = 0.75f;
    
    extern timemilliseconds getMilliseconds(void) ;

    template<typename K, typename V, typename F = KeyHash<K> >
    class Hashtable;

    template <typename K, typename V, typename F = KeyHash<K> >
    class Iterator
    {
      public :
        Iterator(Hashtable<K, V, F> & table):hashtable(table),position (0), current(NULL) {       
        }
        
        Iterator (const Iterator & itr):hashtable(itr.hashtable), position(0), current(NULL) {            
        }
        
        void operator = ( const Iterator & itr) {
            hashtable = itr.hashtable;
            position = itr.position;
            current = itr.current;
        }
        
        bool hasNext() {
            ReadLock(hashtable.mutex);
         
            if (position >= hashtable.capacity)
                return false;
            
            if (current != NULL) {
                if (current->getNext() != NULL) {
                    current = current->getNext();
                    
                    return true;
                }
            }
            for (int j = position ; j < hashtable.capacity; j++) {
                HashNode<K, V > * node = hashtable.table[j];
                if (node != NULL) {
                    current = node;
                    position = j + 1;
                    return true;
                }
            }
            
            return false;
        }
        
        void next(K & k, V & v){
            if (current == NULL)
                return;
            else {
                k = current->getKey();
                v = current->getValue();
            }
        } 
        
        void reset() {
           current = NULL;
           position = 0;
        }
        
        private:
            Hashtable<K, V, F> & hashtable;
            HashNode<K, V> * current;
            size_t position;
    };
    
    
    template <typename K, typename V, typename F = KeyHash<K> >
    class ExpiredIterator
    {
      public :
        ExpiredIterator(Hashtable<K, V, F> & table, timemilliseconds & base):hashtable(table),position (0), current(NULL), basetime(base) {
                
        }
        
        ExpiredIterator (const ExpiredIterator & itr):hashtable(itr.hashtable), position(0), current(NULL), basetime(itr.basetime) {
            
        }
        
        void operator = ( const ExpiredIterator & itr) {
            hashtable = itr.hashtable;
            position = itr.position;
            current = itr.current;
        }
        
        bool hasNext() {
            ReadLock(hashtable.mutex);
         
            if (hashtable.periodSeconds == 0)
                return false;
            
            if (position >= hashtable.capacity)
                return false;
            
            if (current != NULL) {
                for (HashNode<K, V> * c = current->getNext(); c != NULL; c = c->getNext()) {
                    if (isExpired(c)) {
                       current = c;
                       return true;
                    }
                }
            }
            for (int j = position ; j < hashtable.capacity; j++) {
                HashNode<K, V > * node = hashtable.table[j];
                for (HashNode<K, V> * c = node; c != NULL; c = c->getNext()) {
                    if (isExpired(c)) {
                        current = c;
                        position = j + 1;
                        return true;
                    }
                }
            }
            
            return false;
        }
        
        
        void next(K & k, V & v){
                if (current == NULL)
                    return;
                else {
                    k = current->getKey();
                    v = current->getValue();
                }
        } 
        
        void reset() {
           current = NULL;
           position = 0;
        }
        
        private:
            
            Hashtable<K, V, F> & hashtable;
            HashNode<K, V> * current;
            timemilliseconds basetime;
            size_t position;
            
            bool isExpired(HashNode<K, V> * node) {
                return basetime - node->getTime() > hashtable.periodSeconds * 1000;
            }
    };
    
    template <typename K, typename V, typename F>
    void expire(void * para);
    
    template <typename K, typename V, typename F >
    class Hashtable : noncopyable
    {
            template <typename X, typename Y, typename Z>
            friend class Iterator;
            
            template <typename X, typename Y, typename Z>
            friend class ExpiredIterator;
        
            template <typename X, typename Y, typename Z>
            friend void expire(void * para);
       public :
            Hashtable(): timerId(NULL), m_size(0), capacity(defaultCapacity), loadFactor(defaultLoadFactor), threshold(defaultCapacity * defaultLoadFactor), periodSeconds(0)
            {
                mutex = new ReadWriteMutex();
                initTable();
            }
            
            Hashtable(int initCapability):timerId(NULL), m_size(0), capacity(initCapability), loadFactor(defaultLoadFactor), threshold(initCapability * defaultLoadFactor), periodSeconds(0)
            {
                mutex = new ReadWriteMutex();
                initTable();
            }
            
            Hashtable(int initCapability, float factor, int p = 0, void (*func)(K &) = NULL):timerId(NULL), m_size(0), capacity(initCapability), loadFactor(factor), threshold(initCapability * factor), periodSeconds(p), expiredFunc(func)
            {
                mutex = new ReadWriteMutex();
                initTable();
                if (periodSeconds == 0)
                    timerId = NULL;
                else {
                ;
                    timerId = Timer::getInstance().create(periodSeconds, periodSeconds, expire<K, V, F>, this);
                }
            }
            
            ~Hashtable()
            {
                clear();
                delete [] table;
                delete mutex;
                if (timerId != NULL) {
                    // printf ("timer = %p\n", timerId);
                    Timer::getInstance().remove(timerId);
                }
            }
        
            size_t size()
            {
                ReadLock lock(mutex);
                return m_size;
            }
            
            void        put(const K & key, const V & val)
            {
                WriteLock lock(mutex);
                timemilliseconds mill = getMilliseconds();
                bool update = putEntryInternal(table, capacity, key , val, mill);
                if (!update)
                    m_size += 1;
                if (m_size >= threshold) {
                    size_t newcapacity = capacity <<2;
                    rehash(newcapacity);
                }
            }
            
            bool        get(const K & key, V & val)
            {
                    ReadLock lock(mutex);
                    unsigned long hashValue = hashFunc(key, capacity);
                    HashNode<K, V> *entry = table[hashValue];

                    while (entry != NULL) {
                        if (entry->getKey() == key) {
                            val =  entry->getValue();
                            return true;
                        }

                        entry = entry->getNext();
                    }

                    return false;
            }
            
            bool        contain(const K & key)
            {
                ReadLock lock(mutex);
                unsigned long hashValue = hashFunc(key, capacity);
                HashNode<K, V> *entry = table[hashValue];

                while (entry != NULL) {
                    if (entry->getKey() == key) {
                        return true;
                    }

                    entry = entry->getNext();
                }

                return false;
            }
            
            bool        remove(const K & key)
            {
                WriteLock lock(mutex);
                unsigned long hashValue = hashFunc(key, capacity);
                HashNode<K, V> *prev = NULL;
                HashNode<K, V> *entry = table[hashValue];

                while (entry != NULL && entry->getKey() != key) {
                    prev = entry;
                    entry = entry->getNext();
                }

                if (entry == NULL) {
                    // key not found
                    return false;

                } else {
                    if (prev == NULL) {
                        // remove first bucket of the list
                        table[hashValue] = entry->getNext();

                    } else {
                        prev->setNext(entry->getNext());
                    }

                    delete entry;
                    m_size -= 1;
                    return true;
                }   
            }
            
            void        clear()
            {
                WriteLock lock(mutex);
                for (size_t j = 0; j<capacity; j++)
                {
                    HashNode<K, V> * node = table[j];
                    while (node != NULL) {
                        HashNode<K, V> * prev = node;
                        node = node->getNext();
                        delete prev;
                    }
                    table[j] = NULL;
                }
                m_size = 0;
            }
            void        shrink_to_fit()
            {
                WriteLock lock(mutex);
                // 当元素数量远低于阈值时触发缩容，避免内存浪费
                // 阈值 / 4 对应负载因子约 0.1875
                if (m_size < threshold / 4 && capacity > defaultCapacity) {
                    size_t newCapacity = capacity >> 2;
                    if (newCapacity < defaultCapacity) {
                        newCapacity = defaultCapacity;
                    }
                    rehash(newCapacity);
                    // 缩容后强制 glibc 将空闲内存归还给操作系统，降低 RES 占用
                    malloc_trim(0);
                }
            }
            
            Iterator<K, V, F> keys()
            {
                return Iterator<K,V, F>(*this);
            }
            
            ExpiredIterator<K, V, F> expiredKeys()
            {
                 timemilliseconds milliseconds = getMilliseconds();
                return ExpiredIterator<K,V, F>(*this, milliseconds);
            }
            
        private :
            timer_t  timerId;
            ReadWriteMutex * mutex;
            size_t  m_size;
            int     capacity;
            float   loadFactor;
            int     threshold;
            int     periodSeconds;
            F       hashFormula;
            void    (*expiredFunc)(K &);
            // hash table
            HashNode<K, V> ** table ;
            
            void          initTable()
            {
              table = new HashNode<K, V> * [capacity];
              for (size_t i = 0; i<capacity; i++) {
                    table[i] = NULL;
              }   
            }
            
            void  rehash(const size_t newCapacity)
            {
                
                HashNode<K, V> ** newTable = new HashNode<K, V> * [newCapacity];
                for (size_t i = 0; i < newCapacity; i++) {
                    newTable[i] = NULL;
                }
                for (int j = 0; j< capacity; j++) {
                    HashNode<K, V> * entry = table[j];
                    while (entry != NULL) {
                        HashNode<K, V> * next = entry->getNext();
                        putEntryInternal(newTable, newCapacity, entry->getKey(), entry->getValue(), entry->getTime());
                        delete entry;
                        entry = next;
                    }
                }
                
                delete [] table;
                table = newTable;
                capacity = newCapacity;
                threshold =  capacity * loadFactor;
            }
            
            bool          putEntryInternal(HashNode<K, V> ** targetTable, const size_t & size, const K & key, const V & val, const timemilliseconds & time)
            {
                 HashNode<K, V> *prev = NULL;
                unsigned long hashValue = hashFunc(key, size);
                HashNode<K, V> *entry = targetTable[hashValue];

                while (entry != NULL && entry->getKey() != key) {
                    prev = entry;
                    entry = entry->getNext();
                }

                if (entry == NULL) {
                    entry = new HashNode<K, V>(key, val, time);

                    if (prev == NULL) {
                        // insert as first bucket
                        targetTable[hashValue] = entry;

                    } else {
                        prev->setNext(entry);
                    }
                } else {
                    // just update the value
                    entry->setValue(val);
                    entry->setTime(time);
                    return true;
                } 
                return false;
            }
            
            unsigned long hashFunc(const K & key, const size_t & size)
            {
                 unsigned long hashVal = hashFormula(key);
                return hashVal % size;
            }
           
    };
    
    template <typename K, typename V, typename F>
    void expire(void * para) {
        Hashtable<K, V, F> * table = (Hashtable<K, V, F> * )para;
        // 先在持读锁期间收集所有过期 key，避免在持读锁时调用 remove（请求写锁）导致死锁
        std::vector<K> expiredKeys;
        {
            ExpiredIterator<K, V, F> itr = table->expiredKeys();
            while (itr.hasNext()) {
                K  key;
                V  val;
                itr.next(key, val);
                expiredKeys.push_back(key);
            }
        }
        // 释放读锁后，逐个删除已过期的 key（remove 内部自带写锁）
        for (size_t i = 0; i < expiredKeys.size(); ++i) {
            K & key = expiredKeys[i];
            if (table->expiredFunc != NULL) {
                table->expiredFunc(key);
            }
            table->remove(key);
        }
    }
}
#endif // HASHTABLE_H
