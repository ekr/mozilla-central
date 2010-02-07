/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99 ft=cpp:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla SpiderMonkey JavaScript 1.9 code, released
 * November 13, 2009.
 *
 * The Initial Developer of the Original Code is
 *   the Mozilla Corporation.
 *
 * Contributor(s):
 *   Brendan Eich <brendan@mozilla.org> (Original Author)
 *   Chris Waterson <waterson@netscape.com>
 *   L. David Baron <dbaron@dbaron.org>, Mozilla Corporation
 *   Luke Wagner <lw@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef jshashtable_h_
#define jshashtable_h_

#include "jstl.h"

namespace js {

/* Integral types for all hash functions. */
typedef uint32 HashNumber;

namespace detail {

/* Reusable implementation of HashMap and HashSet. */
template <class T, class HashPolicy, class AllocPolicy>
class HashTable : AllocPolicy
{
    typedef typename HashPolicy::KeyType Key;
    typedef typename HashPolicy::Lookup Lookup;

    class Entry {
        HashNumber keyHash;
      public:
        Entry() : keyHash(0), t() {}
        T t;

        bool isFree() const           { return keyHash == 0; }
        void setFree()                { keyHash = 0; t = T(); }
        bool isRemoved() const        { return keyHash == 1; }
        void setRemoved()             { keyHash = 1; t = T(); }
        bool isLive() const           { return keyHash > 1; }
        void setLive(HashNumber hn)   { JS_ASSERT(hn > 1); keyHash = hn; }

        void setCollision()           { JS_ASSERT(keyHash > 1); keyHash |= sCollisionBit; }
        void unsetCollision()         { JS_ASSERT(keyHash > 1); keyHash &= ~sCollisionBit; }
        bool hasCollision() const     { JS_ASSERT(keyHash > 1); return keyHash & sCollisionBit; }
        bool matchHash(HashNumber hn) { return (keyHash & ~sCollisionBit) == hn; }
        HashNumber getKeyHash() const { JS_ASSERT(!hasCollision()); return keyHash; }
    };

  public:
    /*
     * A nullable pointer to a hash table element. A Ptr |p| can be tested
     * either explicitly |if (p.found()) p->...| or using boolean conversion
     * |if (p) p->...|. Ptr objects must not be used after any mutating hash
     * table operations unless |generation()| is tested.
     */
    class Ptr
    {
        friend class HashTable;
        typedef void (Ptr::* ConvertibleToBool)();
        void nonNull() {}

        Ptr(Entry &entry) : entry(&entry) {}
        Entry *entry;

      public:
        bool found() const           { return entry->isLive(); }
        operator ConvertibleToBool() { return found() ? &Ptr::nonNull : 0; }

        const T &operator*() const         { return entry->t; }
        const T *operator->() const        { return &entry->t; }
    };

    /* A Ptr that can be used to add a key after a failed lookup. */
    class AddPtr : public Ptr
    {
        friend class HashTable;
        AddPtr(Entry &entry, HashNumber hn) : Ptr(entry), keyHash(hn) {}
        HashNumber keyHash;
    };

    /*
     * A collection of hash table entries. The collection is enumerated by
     * calling |front()| followed by |popFront()| as long as |!empty()|. As
     * with Ptr/AddPtr, Range objects must not be used after any mutating hash
     * table operation unless the |generation()| is tested.
     */
    class Range
    {
      protected:
        friend class HashTable;

        Range(Entry *c, Entry *e) : cur(c), end(e) {
            while (cur != end && !cur->isLive())
                ++cur;
        }

        Entry *cur, * const end;

      public:
        bool empty() const {
            return cur == end;
        }

        const T &front() const {
            JS_ASSERT(!empty());
            return cur->t;
        }

        void popFront() {
            JS_ASSERT(!empty());
            while (++cur != end && !cur->isLive());
        }
    };

    /*
     * A Range whose lifetime delimits a mutating enumeration of a hash table.
     * Since rehashing when elements were removed during enumeration would be
     * bad, it is postponed until |endEnumeration()| is called. If
     * |endEnumeration()| is not called before an Enum's constructor, it
     * will be called automatically. Since |endEnumeration()| touches the hash
     * table, the user must ensure that the hash table is still alive when this
     * happens.
     */
    class Enum : public Range
    {
        friend class HashTable;

        HashTable &table;
        bool removed;

        /* Not copyable. */
        Enum(const Enum &);
        void operator=(const Enum &);

      public:
        /* Type returned from hash table used to initialize Enum object. */
        struct Init {
            Init(Range r, HashTable &t) : range(r), table(t) {}
            Range range;
            HashTable &table;
        };

        /* Initialize with the return value of enumerate. */
        Enum(Init i) : Range(i.range), table(i.table), removed(false) {}

        /*
         * Removes the |front()| element from the table, leaving |front()|
         * invalid until the next call to |popFront()|. For example:
         *
         *   HashSet<int> s;
         *   for (HashSet<int>::Enum e(s.enumerate()); !e.empty(); e.popFront())
         *     if (e.front() == 42)
         *       e.removeFront();
         */
        void removeFront() {
            table.remove(*this->cur);
            removed = true;
        }

        /* Potentially rehashes the table. */
        ~Enum() {
            if (removed)
                table.checkUnderloaded();
        }

        /* Can be used to endEnumeration before the destructor. */
        void endEnumeration() {
            if (removed) {
                table.checkUnderloaded();
                removed = false;
            }
        }
    };

  private:
    uint32      hashShift;      /* multiplicative hash shift */
    uint32      tableCapacity;  /* = JS_BIT(sHashBits - hashShift) */
    uint32      entryCount;     /* number of entries in table */
    uint32      gen;            /* entry storage generation number */
    uint32      removedCount;   /* removed entry sentinels in table */
    Entry       *table;         /* entry storage */

    void setTableSizeLog2(unsigned sizeLog2) {
        hashShift = sHashBits - sizeLog2;
        tableCapacity = JS_BIT(sizeLog2);
    }

#ifdef DEBUG
    mutable struct Stats {
        uint32          searches;       /* total number of table searches */
        uint32          steps;          /* hash chain links traversed */
        uint32          hits;           /* searches that found key */
        uint32          misses;         /* searches that didn't find key */
        uint32          addOverRemoved; /* adds that recycled a removed entry */
        uint32          removes;        /* calls to remove */
        uint32          removeFrees;    /* calls to remove that freed the entry */
        uint32          grows;          /* table expansions */
        uint32          shrinks;        /* table contractions */
        uint32          compresses;     /* table compressions */
    } stats;
#   define METER(x) x
#else
#   define METER(x)
#endif

#ifdef DEBUG
    friend class js::ReentrancyGuard;
    mutable bool entered;
#endif

    static const unsigned sMinSize      = 16;
    static const size_t   sSizeLimit    = tl::NBitMask<24>::result;
    static const unsigned sHashBits     = tl::BitSize<HashNumber>::result;
    static const unsigned sGoldenRatio  = 0x9E3779B9U;       /* taken from jsdhash.h */
    static const uint8    sMinAlphaFrac = 64;  /* (0x100 * .25) taken from jsdhash.h */
    static const uint8    sMaxAlphaFrac = 192; /* (0x100 * .75) taken from jsdhash.h */
    static const unsigned sCollisionBit = 1;

    static Entry *createTable(AllocPolicy &alloc, uint32 capacity)
    {
        Entry *newTable = (Entry *)alloc.malloc(capacity * sizeof(Entry));
        if (!newTable)
            return NULL;
        for (Entry *e = newTable, *end = e + capacity; e != end; ++e)
            new(e) Entry();
        return newTable;
    }

    static void destroyTable(AllocPolicy &alloc, Entry *oldTable, uint32 capacity)
    {
        for (Entry *e = oldTable, *end = e + capacity; e != end; ++e)
            e->~Entry();
        alloc.free(oldTable);
    }

  public:
    HashTable(AllocPolicy ap)
      : AllocPolicy(ap),
        entryCount(0),
        gen(0),
        removedCount(0),
        table(NULL)
#ifdef DEBUG
        , entered(false)
#endif
    {}

    bool init(uint32 capacity)
    {
        if (capacity < sMinSize)
            capacity = sMinSize;
        int log2;
        JS_CEILING_LOG2(log2, capacity);
        capacity = JS_BIT(log2);
        if (capacity >= sSizeLimit)
            return false;

        table = createTable(*this, capacity);
        if (!table)
            return false;

        setTableSizeLog2(log2);
        METER(memset(&stats, 0, sizeof(stats)));
        return true;
    }

    ~HashTable()
    {
        if (table)
            destroyTable(*this, table, tableCapacity);
    }

  private:
    static uint32 hash1(uint32 hash0, uint32 shift) {
        return hash0 >> shift;
    }

    static uint32 hash2(uint32 hash0, uint32 log2, uint32 shift) {
        return ((hash0 << log2) >> shift) | 1;
    }

    bool overloaded() {
        return entryCount + removedCount >= ((sMaxAlphaFrac * tableCapacity) >> 8);
    }

    bool underloaded() {
        return tableCapacity > sMinSize &&
               entryCount <= ((sMinAlphaFrac * tableCapacity) >> 8);
    }

    static bool match(Entry &e, const Lookup &l) {
        return HashPolicy::match(HashPolicy::getKey(e.t), l);
    }

    struct SetCollisions {
        static void collide(Entry &e) { e.setCollision(); }
    };

    struct IgnoreCollisions {
        static void collide(Entry &) {}
    };

    template <class Op>
    AddPtr lookup(const Lookup &l, HashNumber keyHash) const
    {
        JS_ASSERT(table);
        METER(stats.searches++);

        /* Improve keyHash distribution. */
        keyHash *= sGoldenRatio;

        /* Avoid reserved hash codes. */
        if (keyHash < 2)
            keyHash -= 2;
        keyHash &= ~sCollisionBit;

        /* Compute the primary hash address. */
        uint32 h1 = hash1(keyHash, hashShift);
        Entry *entry = &table[h1];

        /* Miss: return space for a new entry. */
        if (entry->isFree()) {
            METER(stats.misses++);
            return AddPtr(*entry, keyHash);
        }

        /* Hit: return entry. */
        if (entry->matchHash(keyHash) && match(*entry, l)) {
            METER(stats.hits++);
            return AddPtr(*entry, keyHash);
        }

        /* Collision: double hash. */
        unsigned sizeLog2 = sHashBits - hashShift;
        uint32 h2 = hash2(keyHash, sizeLog2, hashShift);
        uint32 sizeMask = JS_BITMASK(sizeLog2);

        /* Save the first removed entry pointer so we can recycle later. */
        Entry *firstRemoved = NULL;

        while(true) {
            if (JS_UNLIKELY(entry->isRemoved())) {
                if (!firstRemoved)
                    firstRemoved = entry;
            } else {
                Op::collide(*entry);
            }

            METER(stats.steps++);
            h1 -= h2;
            h1 &= sizeMask;

            entry = &table[h1];
            if (entry->isFree()) {
                METER(stats.misses++);
                return AddPtr(*(firstRemoved ? firstRemoved : entry), keyHash);
            }

            if (entry->matchHash(keyHash) && match(*entry, l)) {
                METER(stats.hits++);
                return AddPtr(*entry, keyHash);
            }
        }
    }

    /*
     * This is a copy of lookup hardcoded to the assumptions:
     *   1. the lookup is a looupForAdd
     *   2. the key, whose |keyHash| has been passed is not in the table,
     *   3. no entries have been removed from the table.
     * This specialized search avoids the need for recovering lookup values
     * from entries, which allows more flexible Lookup/Key types.
     */
    Entry &findFreeEntry(HashNumber keyHash)
    {
        METER(stats.searches++);
        JS_ASSERT(!(keyHash & sCollisionBit));

        /* N.B. the |keyHash| has already been distributed. */

        /* Compute the primary hash address. */
        uint32 h1 = hash1(keyHash, hashShift);
        Entry *entry = &table[h1];

        /* Miss: return space for a new entry. */
        if (entry->isFree()) {
            METER(stats.misses++);
            return *entry;
        }

        /* Collision: double hash. */
        unsigned sizeLog2 = sHashBits - hashShift;
        uint32 h2 = hash2(keyHash, sizeLog2, hashShift);
        uint32 sizeMask = JS_BITMASK(sizeLog2);

        while(true) {
            JS_ASSERT(!entry->isRemoved());
            entry->setCollision();

            METER(stats.steps++);
            h1 -= h2;
            h1 &= sizeMask;

            entry = &table[h1];
            if (entry->isFree()) {
                METER(stats.misses++);
                return *entry;
            }
        }
    }

    bool changeTableSize(int deltaLog2)
    {
        /* Look, but don't touch, until we succeed in getting new entry store. */
        Entry *oldTable = table;
        uint32 oldCap = tableCapacity;
        uint32 newLog2 = sHashBits - hashShift + deltaLog2;
        uint32 newCapacity = JS_BIT(newLog2);
        if (newCapacity >= sSizeLimit)
            return false;

        Entry *newTable = createTable(*this, newCapacity);
        if (!newTable)
            return false;

        /* We can't fail from here on, so update table parameters. */
        setTableSizeLog2(newLog2);
        removedCount = 0;
        gen++;
        table = newTable;

        /* Copy only live entries, leaving removed ones behind. */
        for (Entry *src = oldTable, *end = src + oldCap; src != end; ++src) {
            if (src->isLive()) {
                src->unsetCollision();
                findFreeEntry(src->getKeyHash()) = *src;
            }
        }

        destroyTable(*this, oldTable, oldCap);
        return true;
    }

    void remove(Entry &e)
    {
        METER(stats.removes++);
        if (e.hasCollision()) {
            e.setRemoved();
            removedCount++;
        } else {
            METER(stats.removeFrees++);
            e.setFree();
        }
        entryCount--;
    }

    void checkUnderloaded()
    {
        if (underloaded()) {
            METER(stats.shrinks++);
            (void) changeTableSize(-1);
        }
    }

  public:
    void clear()
    {
        for (Entry *e = table, *end = table + tableCapacity; e != end; ++e)
            *e = Entry();
        removedCount = 0;
        entryCount = 0;
    }

    Range all() const {
        return Range(table, table + tableCapacity);
    }

    typename Enum::Init enumerate() {
        return typename Enum::Init(all(), *this);
    }

    bool empty() const {
        return !entryCount;
    }

    uint32 count() const{
        return entryCount;
    }

    uint32 generation() const {
        return gen;
    }

    Ptr lookup(const Lookup &l) const {
        ReentrancyGuard g(*this);
        return lookup<IgnoreCollisions>(l, HashPolicy::hash(l));
    }

    AddPtr lookupForAdd(const Lookup &l) const {
        ReentrancyGuard g(*this);
        return lookup<SetCollisions>(l, HashPolicy::hash(l));
    }

    bool add(AddPtr &p, const T &t)
    {
        ReentrancyGuard g(*this);
        JS_ASSERT(table);
        JS_ASSERT(!p.found());
        JS_ASSERT(!(p.keyHash & sCollisionBit));

        /*
         * Changing an entry from removed to live does not affect whether we
         * are overloaded and can be handled separately.
         */
        if (p.entry->isRemoved()) {
            METER(stats.addOverRemoved++);
            removedCount--;
            p.keyHash |= sCollisionBit;
        } else {
            /* If alpha is >= .75, grow or compress the table. */
            if (overloaded()) {
                /* Compress if a quarter or more of all entries are removed. */
                int deltaLog2;
                if (removedCount >= (tableCapacity >> 2)) {
                    METER(stats.compresses++);
                    deltaLog2 = 0;
                } else {
                    METER(stats.grows++);
                    deltaLog2 = 1;
                }

                if (!changeTableSize(deltaLog2))
                    return false;

                /* Preserve the validity of |p.entry|. */
                p.entry = &findFreeEntry(p.keyHash);
            }
        }

        p.entry->t = t;
        p.entry->setLive(p.keyHash);
        entryCount++;
        return true;
    }

    void remove(Ptr p)
    {
        ReentrancyGuard g(*this);
        JS_ASSERT(p.found());
        remove(*p.entry);
        checkUnderloaded();
    }
#undef METER
};

}

/*
 * Hash policy
 *
 * A hash policy P for a hash table with key-type Key must provide:
 *  - a type |P::Lookup| to use to lookup table entries;
 *  - a static member function |P::hash| with signature
 *
 *      static js::HashNumber hash(Lookup)
 *
 *    to use to hash the lookup type; and
 *  - a static member function |P::match| with signature
 *
 *      static bool match(Key, Lookup)
 *
 *    to use to test equality of key and lookup values.
 *
 * Normally, Lookup = Key. In the general, though, different values and types
 * of values can be used to lookup and store. If a Lookup value |l| is != to
 * the added Key value |k|, the user must ensure that |P::match(k,l)|. E.g.:
 *
 *   js::HashSet<Key, P>::AddPtr p = h.lookup(l);
 *   if (!p) {
 *     assert(P::match(k, l));  // must hold
 *     h.add(p, k);
 *   }
 */

/* Default hashing policies. */
template <class Key>
struct DefaultHasher
{
    typedef Key Lookup;
    static uint32 hash(const Lookup &l) {
        /* Hash if can implicitly cast to hash number type. */
        return l;
    }
    static bool match(const Key &k, const Lookup &l) {
        /* Use builtin or overloaded operator==. */
        return k == l;
    }
};

/* Specialized hashing policy for pointer types. */
template <class T>
struct DefaultHasher<T *>
{
    typedef T *Lookup;
    static uint32 hash(T *l) {
        /*
         * Strip often-0 lower bits for better distribution after multiplying
         * by the sGoldenRatio.
         */
        return (uint32)(unsigned long)l >> 2;
    }
    static bool match(T *k, T *l) {
        return k == l;
    }
};

/*
 * JS-friendly, STL-like container providing a hash-based map from keys to
 * values. In particular, HashMap calls constructors and destructors of all
 * objects added so non-PODs may be used safely.
 *
 * Key/Value requirements:
 *  - default constructible, copyable, destructible, assignable
 * HashPolicy requirements:
 *  - see "Hash policy" above (default js::DefaultHasher<Key>)
 * AllocPolicy:
 *  - see "Allocation policies" in jstl.h (default js::ContextAllocPolicy)
 *
 * N.B: HashMap is not reentrant: Key/Value/HashPolicy/AllocPolicy members
 *      called by HashMap must not call back into the same HashMap object.
 * N.B: Due to the lack of exception handling, the user must call |init()|.
 */
template <class Key, class Value, class HashPolicy, class AllocPolicy>
class HashMap
{
  public:
    typedef typename HashPolicy::Lookup Lookup;

    /*
     * The type of an entry in a HashMap. Internally, the |key| is not const
     * since it gets overwritten on add/remove operations. To the caller,
     * though, it is const, hence Entry_ is used internally and Entry is
     * returned to callers.
     */
    typedef struct Entry_
    {
        Entry_() : key(), value() {}
        Entry_(const Key &k, const Value &v) : key(k), value(v) {}

        Key key;
        mutable Value value;
    } const Entry;

  private:
    /* Implement HashMap using HashTable. Lift |Key| operations to |Entry|. */
    struct MapHashPolicy : HashPolicy
    {
        typedef Key KeyType;
        static const Key &getKey(Entry &e) { return e.key; }
    };
    typedef detail::HashTable<Entry_, MapHashPolicy, AllocPolicy> Impl;

    /* Not implicitly copyable (expensive). May add explicit |clone| later. */
    HashMap(const HashMap &);
    HashMap &operator=(const HashMap &);

    Impl impl;

  public:
    /*
     * HashMap construction is fallible (due to OOM); thus the user must call
     * init after constructing a HashMap and check the return value.
     */
    HashMap(AllocPolicy a = AllocPolicy()) : impl(a) {}
    bool init(uint32 cap = 0)                         { return impl.init(cap); }

    /*
     * Return whether the given lookup value is present in the map. E.g.:
     *
     *   typedef HashMap<int,char> HM;
     *   HM h;
     *   if (HM::Ptr p = h.lookup(3)) {
     *     const HM::Entry &e = *p; // p acts like a pointer to Entry
     *     assert(p->key == 3);     // Entry contains the key
     *     char val = p->value;     // and value
     *   }
     *
     * Also see the definition of Ptr in HashTable above (with T = Entry).
     */
    typedef typename Impl::Ptr Ptr;
    Ptr lookup(const Lookup &l) const                 { return impl.lookup(l); }

    /* Assuming |p.found()|, remove |*p|. */
    void remove(Ptr p)                                { impl.remove(p); }

    /*
     * Like |lookup(l)|, but on miss, |p = lookupForAdd(l)| allows efficient
     * insertion of Key |k| (where |HashPolicy::match(k,l) == true|) using
     * |add(p,k,v)|.  After |add(p,k,v)|, |p| points to the new Entry. E.g.:
     *
     *   typedef HashMap<int,char> HM;
     *   HM h;
     *   HM::AddPtr p = h.lookupForAdd(3);
     *   if (!p) {
     *     if (!h.add(p, 3, 'a'))
     *       return false;
     *   }
     *   const HM::Entry &e = *p;   // p acts like a pointer to Entry
     *   assert(p->key == 3);       // Entry contains the key
     *   char val = p->value;       // and value
     *
     * Also see the definition of AddPtr in HashTable above (with T = Entry).
     */
    typedef typename Impl::AddPtr AddPtr;
    AddPtr lookupForAdd(const Lookup &l) const        { return impl.lookupForAdd(l); }
    bool add(AddPtr &p, const Key &k, const Value &v) { return impl.add(p,Entry(k,v)); }

    /*
     * |all()| returns a Range containing |count()| elements. E.g.:
     *
     *   typedef HashMap<int,char> HM;
     *   HM h;
     *   for (HM::Range r = h.all(); !r.empty(); r.popFront())
     *     char c = r.front().value;
     *
     * Also see the definition of Range in HashTable above (with T = Entry).
     */
    typedef typename Impl::Range Range;
    Range all() const                                 { return impl.all(); }
    size_t count() const                              { return impl.count(); }

    /*
     * Returns a value that may be used to initialize an Enum. An Enum may be
     * used to examine and remove table entries:
     *
     *   typedef HashMap<int,char> HM;
     *   HM s;
     *   for (HM::Enum e(s.enumerate()); !e.empty(); e.popFront())
     *     if (e.front().value == 'l')
     *       e.removeFront();
     *
     * Table resize may occur in Enum's destructor. Also see the definition of
     * Enum in HashTable above (with T = Entry).
     */
    typedef typename Impl::Enum Enum;
    typename Enum::Init enumerate()                   { return impl.enumerate(); }

    /* Remove all entries. */
    void clear()                                      { impl.clear(); }

    /* Does the table contain any entries? */
    bool empty() const                                { return impl.empty(); }

    /*
     * If |generation()| is the same before and after a HashMap operation,
     * pointers into the table remain valid.
     */
    unsigned generation() const                       { return impl.generation(); }

    /* Shorthand operations: */

    bool has(const Lookup &l) const {
        return impl.lookup(l) != NULL;
    }

    Entry *put(const Key &k, const Value &v) {
        AddPtr p = lookupForAdd(k);
        return p ? &*p : (add(p, k, v) ? &*p : NULL);
    }

    void remove(const Lookup &l) {
        if (Ptr p = lookup(l))
            remove(p);
    }
};

/*
 * JS-friendly, STL-like container providing a hash-based set of values. In
 * particular, HashSet calls constructors and destructors of all objects added
 * so non-PODs may be used safely.
 *
 * T requirements:
 *  - default constructible, copyable, destructible, assignable
 * HashPolicy requirements:
 *  - see "Hash policy" above (default js::DefaultHasher<Key>)
 * AllocPolicy:
 *  - see "Allocation policies" in jstl.h (default js::ContextAllocPolicy)
 *
 * N.B: HashSet is not reentrant: T/HashPolicy/AllocPolicy members called by
 *      HashSet must not call back into the same HashSet object.
 * N.B: Due to the lack of exception handling, the user must call |init()|.
 */
template <class T, class HashPolicy, class AllocPolicy>
class HashSet
{
    typedef typename HashPolicy::Lookup Lookup;

    /* Implement HashSet in terms of HashTable. */
    struct SetOps : HashPolicy {
        typedef T KeyType;
        static const KeyType &getKey(const T &t) { return t; }
    };
    typedef detail::HashTable<T, SetOps, AllocPolicy> Impl;

    /* Not implicitly copyable (expensive). May add explicit |clone| later. */
    HashSet(const HashSet &);
    HashSet &operator=(const HashSet &);

    Impl impl;

  public:
    /*
     * HashSet construction is fallible (due to OOM); thus the user must call
     * init after constructing a HashSet and check the return value.
     */
    HashSet(AllocPolicy a = AllocPolicy()) : impl(a) {}
    bool init(uint32 cap = 0)                         { return impl.init(cap); }

    /*
     * Return whether the given lookup value is present in the map. E.g.:
     *
     *   typedef HashSet<int> HS;
     *   HS h;
     *   if (HS::Ptr p = h.lookup(3)) {
     *     assert(*p == 3);   // p acts like a pointer to int
     *   }
     *
     * Also see the definition of Ptr in HashTable above.
     */
    typedef typename Impl::Ptr Ptr;
    Ptr lookup(const Lookup &l) const                 { return impl.lookup(l); }

    /* Assuming |p.found()|, remove |*p|. */
    void remove(Ptr p)                                { impl.remove(p); }

    /*
     * Like |lookup(l)|, but on miss, |p = lookupForAdd(l)| allows efficient
     * insertion of T value |t| (where |HashPolicy::match(t,l) == true|) using
     * |add(p,t)|.  After |add(p,t)|, |p| points to the new element. E.g.:
     *
     *   typedef HashSet<int> HS;
     *   HS h;
     *   HS::AddPtr p = h.lookupForAdd(3);
     *   if (!p) {
     *     if (!h.add(p, 3))
     *       return false;
     *   }
     *   assert(*p == 3);   // p acts like a pointer to int
     *
     * Also see the definition of AddPtr in HashTable above.
     */
    typedef typename Impl::AddPtr AddPtr;
    AddPtr lookupForAdd(const Lookup &l) const        { return impl.lookupForAdd(l); }
    bool add(AddPtr &p, const T &t)                   { return impl.add(p,t); }

    /*
     * |all()| returns a Range containing |count()| elements:
     *
     *   typedef HashSet<int> HS;
     *   HS h;
     *   for (HS::Range r = h.all(); !r.empty(); r.popFront())
     *     int i = r.front();
     *
     * Also see the definition of Range in HashTable above.
     */
    typedef typename Impl::Range Range;
    Range all() const                                 { return impl.all(); }
    size_t count() const                              { return impl.count(); }

    /*
     * Returns a value that may be used to initialize an Enum. An Enum may be
     * used to examine and remove table entries.
     *
     *   typedef HashSet<int> HS;
     *   HS s;
     *   for (HS::Enum e(s.enumerate()); !e.empty(); e.popFront())
     *     if (e.front() == 42)
     *       e.removeFront();
     *
     * Table resize may occur in Enum's destructor. Also see the definition of
     * Enum in HashTable above.
     */
    typedef typename Impl::Enum Enum;
    typename Enum::Init enumerate()                   { return impl.enumerate(); }

    /* Remove all entries. */
    void clear()                                      { impl.clear(); }

    /* Does the table contain any entries? */
    bool empty() const                                { return impl.empty(); }

    /*
     * If |generation()| is the same before and after a HashSet operation,
     * pointers into the table remain valid.
     */
    unsigned generation() const                       { return impl.generation(); }

    /* Shorthand operations: */

    bool has(const Lookup &l) const {
        return impl.lookup(l) != NULL;
    }

    const T *put(const T &t) {
        AddPtr p = lookupForAdd(t);
        return p ? &*p : (add(p, t) ? &*p : NULL);
    }

    void remove(const Lookup &l) {
        if (Ptr p = lookup(l))
            remove(p);
    }
};

}  /* namespace js */

#endif
