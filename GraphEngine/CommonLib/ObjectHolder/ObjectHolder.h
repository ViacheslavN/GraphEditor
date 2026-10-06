#pragma once
#include "../CommonLib.h"
#include "../exception/exc_base.h"
#include <any>
#include <map>
#include <mutex>


 namespace CommonLib {

     // Keeps shared_ptr objects alive and hands out opaque handles for them
     // (e.g. to pass native objects through C# P/Invoke or Kotlin JNI).
     // Thread-safe.
     class CObjectHolder {
     public:
         typedef intptr_t ObjectHandle;

         // Registers the object and returns its handle.
         // Throws CExcBase if ptrObj is null or the object is already registered.
         template<class ObjectType>
         ObjectHandle AddObject(std::shared_ptr<ObjectType> ptrObj) {
             if (!ptrObj)
                 throw CExcBase("ObjectHolder: failed to add object, object is null");

             ObjectHandle objectHandle = GetHandleFromObject(ptrObj.get());
             AddObject(objectHandle, std::any(std::move(ptrObj)));

             return objectHandle;
         }

         // Returns the object registered under the handle.
         // ObjectType must be exactly the type it was added with.
         // Throws CExcBase if the handle is unknown or the type does not match.
         template<class ObjectType>
         std::shared_ptr<ObjectType> GetObjectByHandle(ObjectHandle objectHandle) const {
             std::any object = GetObjectAny(objectHandle);

             const std::shared_ptr<ObjectType>* pPtrObj = std::any_cast<std::shared_ptr<ObjectType>>(&object);
             if (pPtrObj == nullptr)
                 throw CExcBase("ObjectHolder: failed to get object, handle: {0}, type mismatch, stored type: {1}",
                                (int64_t)objectHandle, std::string(object.type().name()));

             return *pPtrObj;
         }

         // Removes the object from the holder (releases the holder's reference).
         // Throws CExcBase if the handle is unknown.
         void RemoveObject(ObjectHandle objectHandle);

         bool IsExist(ObjectHandle objectHandle) const;
         size_t GetCount() const;

     private:

         void AddObject(ObjectHandle objectHandle, std::any&& object);
         std::any GetObjectAny(ObjectHandle objectHandle) const;
         ObjectHandle GetHandleFromObject(void *pObjAdd) const;

         typedef std::map<ObjectHandle, std::any> TObjectMap;

         TObjectMap m_ObjectMap;
         mutable std::mutex m_Mutex;
     };


 }
