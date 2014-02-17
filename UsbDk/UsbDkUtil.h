#pragma once

#include <ntddk.h>

class CWdmSpinLock
{
public:
    CWdmSpinLock()
    { KeInitializeSpinLock(&m_Lock); }
    void Lock()
    { KeAcquireSpinLock(&m_Lock, &m_OldIrql); }
    void Unlock()
    { KeReleaseSpinLock(&m_Lock, m_OldIrql); }
private:
    KSPIN_LOCK m_Lock;
    KIRQL m_OldIrql;
};

template <typename T>
class CLockedContext
{
public:
    CLockedContext(T &LockObject)
        : m_LockObject(LockObject)
    { m_LockObject.Lock(); }

    ~CLockedContext()
    { m_LockObject.Unlock(); }

private:
    T &m_LockObject;

    CLockedContext(const CLockedContext&) = delete;
    CLockedContext& operator= (const CLockedContext&) = delete;
};

typedef CLockedContext<CWdmSpinLock> TSpinLocker;

class CWdmRefCounter
{
public:
    void AddRef() { InterlockedIncrement(&m_Counter); }
    void AddRef(LONG RefCnt) { InterlockedAdd(&m_Counter, RefCnt); }
    LONG Release() { return InterlockedDecrement(&m_Counter); }
    LONG Release(LONG RefCnt) { AddRef(-RefCnt); }
    operator LONG () { return m_Counter; }
private:
    LONG m_Counter = 0;
};

class CLockedAccess
{
public:
    void Lock() { m_Lock.Lock(); }
    void Unlock() { m_Lock.Unlock(); }
private:
    CWdmSpinLock m_Lock;
};

class CRawAccess
{
public:
    void Lock() { }
    void Unlock() { }
};

class CCountingObject
{
public:
    void CounterIncrement() { m_Counter++; }
    void CounterDecrement() { m_Counter--; }
    ULONG GetCount() { return m_Counter; }
private:
    ULONG m_Counter = 0;
};

class CNonCountingObject
{
public:
    void CounterIncrement() { }
    void CounterDecrement() { }
protected:
    ULONG GetCount() { return 0; }
};

template <typename TEntryType, typename TAccessStrategy, typename TCountingStrategy>
class CWdmList : private TAccessStrategy, public TCountingStrategy
{
public:
    CWdmList()
    { InitializeListHead(&m_List); }

    bool IsEmpty()
    { return IsListEmpty(&m_List) ? true : false; }

    TEntryType *Pop()
    {
        CLockedContext<TAccessStrategy> LockedContext(*this);
        return Pop_LockLess();
    }

    ULONG Push(TEntryType *Entry)
    {
        CLockedContext<TAccessStrategy> LockedContext(*this);
        InsertHeadList(&m_List, Entry->GetListEntry());
        CounterIncrement();
        return GetCount();
    }

    ULONG PushBack(TEntryType *Entry)
    {
        CLockedContext<TAccessStrategy> LockedContext(*this);
        InsertTailList(&m_List, Entry->GetListEntry());
        CounterIncrement();
        return GetCount();
    }

    void Remove(TEntryType *Entry)
    {
        CLockedContext<TAccessStrategy> LockedContext(*this);
        RemoveEntryList(Entry->GetListEntry());
        CounterDecrement();
    }

    template <typename TFunctor>
    void ForEachDetached(TFunctor Functor)
    {
        CLockedContext<TAccessStrategy> LockedContext(*this);
        while (!IsListEmpty(&m_List))
        {
            Functor(Pop_LockLess());
        }
    }

    template <typename TPredicate, typename TFunctor>
    void ForEachDetachedIf(TPredicate Predicate, TFunctor Functor)
    {
        ForEachPrepareIf(Predicate, [](PLIST_ENTRY Entry){ RemoveEntryList(Entry); }, Functor);
    }

    template <typename TFunctor>
    void ForEach(TFunctor Functor)
    {
        ForEachPrepareIf([](TEntryType*) { return true; }, [](PLIST_ENTRY){}, Functor);
    }

private:
    template <typename TPredicate, typename TPrepareFunctor, typename TFunctor>
    void ForEachPrepareIf(TPredicate Predicate, TPrepareFunctor Prepare, TFunctor Functor)
    {
        CLockedContext<TAccessStrategy> LockedContext(*this);

        PLIST_ENTRY NextEntry = nullptr;

        for (auto CurrEntry = m_List.Flink; CurrEntry != &m_List; CurrEntry = NextEntry)
        {
            NextEntry = CurrEntry->Flink;
            auto Object = TEntryType::GetByListEntry(CurrEntry);

            if (Predicate(Object))
            {
                Prepare(CurrEntry);
                Functor(Object);
            }
        }
    }

    TEntryType *Pop_LockLess()
    {
        CounterDecrement();
        return TEntryType::GetByListEntry(RemoveHeadList(&m_List));
    }

    LIST_ENTRY m_List;
    ULONG m_NumEntries = 0;
};
