#include "ObjectHolder.h"

namespace CommonLib {



    CObjectHolder::ObjectHandle CObjectHolder::GetHandleFromObject(void *pObjAdd) const {
        return reinterpret_cast<ObjectHandle>(pObjAdd);
    }

    void CObjectHolder::AddObject(ObjectHandle objectHandle, std::any &&object) {
        std::lock_guard<std::mutex> locker(m_Mutex);

        auto res = m_ObjectMap.emplace(objectHandle, std::move(object));
        if (!res.second)
            throw CExcBase("ObjectHolder: failed to add object, object with handle: {0} is already registered", (int64_t)objectHandle);
    }

    std::any CObjectHolder::GetObjectAny(ObjectHandle objectHandle) const {
        std::lock_guard<std::mutex> locker(m_Mutex);

        auto it = m_ObjectMap.find(objectHandle);
        if (it == m_ObjectMap.end())
            throw CExcBase("ObjectHolder: failed to get object, handle: {0} not found", (int64_t)objectHandle);

        return it->second;
    }

    void CObjectHolder::RemoveObject(ObjectHandle objectHandle) {
        std::any object;   // destroyed after the lock is released
        {
            std::lock_guard<std::mutex> locker(m_Mutex);

            auto it = m_ObjectMap.find(objectHandle);
            if (it == m_ObjectMap.end())
                throw CExcBase("ObjectHolder: failed to remove object, handle: {0} not found", (int64_t)objectHandle);

            object = std::move(it->second);
            m_ObjectMap.erase(it);
        }
    }

    bool CObjectHolder::IsExist(ObjectHandle objectHandle) const {
        std::lock_guard<std::mutex> locker(m_Mutex);
        return m_ObjectMap.find(objectHandle) != m_ObjectMap.end();
    }

    size_t CObjectHolder::GetCount() const {
        std::lock_guard<std::mutex> locker(m_Mutex);
        return m_ObjectMap.size();
    }
}
