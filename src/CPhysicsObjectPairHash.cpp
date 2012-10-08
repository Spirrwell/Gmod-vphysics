#include "StdAfx.h"
#include "CPhysicsObjectPairHash.h"

// memdbgon must be the last include file in a .cpp file!!!
//#include "tier0/memdbgon.h"

/***********************************
* CLASS CPhysicsObjectPairHash
***********************************/

CPhysicsObjectPairHash::CPhysicsObjectPairHash() {
	for (int i = 0; i < 256; i++)
		m_pHashList[i] = NULL;
}

CPhysicsObjectPairHash::~CPhysicsObjectPairHash() {

}

void CPhysicsObjectPairHash::AddObjectPair(void *pObject0, void *pObject1) {
	int entry = (((int)pObject0 ^ (int)pObject1) >> 4) & 0xFF;
	pair_hash_list *last = NULL;
	for (pair_hash_list *hash = m_pHashList[entry]; hash; hash = hash->next)
		last = hash;
	pair_hash_list *hash = new pair_hash_list;
	hash->object0 = pObject0;
	hash->object1 = pObject1;
	hash->previous = last;
	hash->next = NULL;
	if (last)
		last->next = hash;
	else
		m_pHashList[entry] = hash;
}

void CPhysicsObjectPairHash::RemoveObjectPair(void *pObject0, void *pObject1) {
	int entry = (((int)pObject0 ^ (int)pObject1) >> 4) & 0xFF;

	pair_hash_list *hashnext = NULL;
	for (pair_hash_list *hash = m_pHashList[entry]; hash; hash = hashnext) {
		if (hash->object0 == pObject0 || hash->object0 == pObject1 || hash->object1 == pObject0 || hash->object1 == pObject1) {
			if (hash->previous)
				hash->previous->next = hash->next;
			else
				m_pHashList[entry] = hash->next;
			if (hash->next)
				hash->next->previous = hash->previous;

			// Fix for accessing the object after it's been deleted!
			if (hash->next)
				hashnext = hash->next;
			else
				hashnext = NULL;

			delete hash;
		}
	}
}

void CPhysicsObjectPairHash::RemoveAllPairsForObject(void *pObject0) {
	for (int i = 0; i < 256; i++) {
		pair_hash_list *hashnext = NULL;
		for (pair_hash_list *hash = m_pHashList[i]; hash; hash = hashnext) {
			if (hash->object0 == pObject0 || hash->object1 == pObject0) {
				if (hash->previous)
					hash->previous->next = hash->next;
				else
					m_pHashList[i] = hash->next;
				if (hash->next)
					hash->next->previous = hash->previous;

				// Fix for accessing the object after it's been deleted!
				if (hash->next)
					hashnext = hash->next;
				else
					hashnext = NULL;

				delete hash;
			}
		}
	}
}

bool CPhysicsObjectPairHash::IsObjectPairInHash(void *pObject0, void *pObject1) {
	for (pair_hash_list *hash = m_pHashList[(((int)pObject0 ^ (int)pObject1) >> 4) & 0xFF]; hash; hash = hash->next) {
		if (hash->object0 == pObject0 || hash->object0 == pObject1 || hash->object1 == pObject0 || hash->object1 == pObject1)
			return true;
	}
	return false;
}

bool CPhysicsObjectPairHash::IsObjectInHash(void *pObject0) {
	for (int i = 0; i < 256; i++) {
		for (pair_hash_list *hash = m_pHashList[i]; hash; hash = hash->next) {
			if (hash->object0 == pObject0 || hash->object1 == pObject0)
				return true;
		}
	}
	return false;
}

int CPhysicsObjectPairHash::GetPairCountForObject(void *pObject0) {
	int c = 0;
	for (int i = 0; i < 256; i++) {
		for (pair_hash_list *hash = m_pHashList[i]; hash; hash = hash->next) {
			if (hash->object0 == pObject0 || hash->object1 == pObject0)
				c++;
		}
	}
	return c;
}

int CPhysicsObjectPairHash::GetPairListForObject(void *pObject0, int nMaxCount, void **ppObjectList) {
	int c = 0;
	for (int i = 0; i < 256; i++) {
		for (pair_hash_list *hash = m_pHashList[i]; hash; hash = hash->next) {
			if (c < nMaxCount && hash->object0 == pObject0)
				ppObjectList[c++] = hash->object1;
			if (c < nMaxCount && hash->object1 == pObject0)
				ppObjectList[c++] = hash->object0;
		}
	}
	return c;
}
