#pragma once
#include "GeoDatabase.h"


namespace GraphEngine
{
    namespace GeoDatabase {

        class  COIDSet : public IOIDSet
        {
        public:
            COIDSet();
            virtual ~COIDSet();

        private:
            COIDSet(const COIDSet&);
            COIDSet& operator=(const COIDSet&);

        public:
            // IOIDSet
            virtual int  GetCount() const;
            virtual bool Find(int64_t id) const;
            virtual void Reset();
            virtual bool Next(int64_t& id);
            virtual void Add(int64_t id);
            virtual void AddList(const int64_t* id, int count);
            virtual void Remove(int64_t id);
            virtual void RemoveList(const int64_t* id, int count);
            virtual void Clear();
        private:
            typedef std::set<int64_t> TSetIDs;
            TSetIDs                 m_setIds;
            TSetIDs::iterator       m_it;
        };


    }
    }