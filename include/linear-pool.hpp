//------------------------------------------------------------------------------------------------------------
// Copyright 2022-2023 Nikita Fediuchin. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//------------------------------------------------------------------------------------------------------------

#pragma once
#include <stack>
#include <mutex>
#include <vector>
#include <string>
#include <atomic>
#include <cassert>
#include <cstring>
#include <stdexcept>

namespace ecsm
{

using namespace std;

template<class T, bool Destroy = true>
class LinearPool;

//------------------------------------------------------------------------------------------------------------
template<class T>
struct ID final
{
private:
	uint32_t index = 0;

	ID(uint32_t index) noexcept { this->index = index; }
	friend class LinearPool<T, true>;
	friend class LinearPool<T, false>;
public:
	ID() = default;
	template<class U>
	explicit ID(ID<U> id) noexcept : index(*id) { }

	uint32_t operator*() const noexcept { return index; }
	uint32_t& operator*() noexcept { return index; } // Use this to set uint32 value.
	bool operator==(ID<T> v) const noexcept { return index == v.index; }
	bool operator!=(ID<T> v) const noexcept { return index != v.index; }
	bool operator<(ID<T> v) const noexcept { return index < v.index; }
	operator bool() const noexcept { return index; }
};

//------------------------------------------------------------------------------------------------------------
template<class T>
struct View final
{
private:
	T* item = nullptr;

	#ifndef NDEBUG
	const size_t* poolVersion = nullptr;
	size_t version = 0;

	View(T* item, const size_t& poolVersion) noexcept
	{
		this->item = item;
		this->poolVersion = &poolVersion;
		this->version = poolVersion;
	}
	#else
	View(T* item) noexcept { this->item = item; }
	#endif
	
	friend class LinearPool<T, true>;
	friend class LinearPool<T, false>;
public:
	View() = default;

	template<class U>
	explicit View(const View<U>& view) noexcept {
		memcpy(this, &view, sizeof(View<T>)); }
	operator bool() const noexcept { return item; }

	T* operator->()
	{
		#ifndef NDEBUG
		if (version != *poolVersion)
			throw runtime_error("Item has been invalidated by the previous calls.");
		#endif
		return item;
	}
	const T* operator->() const
	{
		#ifndef NDEBUG
		if (version != *poolVersion)
			throw runtime_error("Item has been invalidated by the previous calls.");
		#endif
		return item;
	}

	T* operator*()
	{
		#ifndef NDEBUG
		if (version != *poolVersion)
			throw runtime_error("Item has been invalidated by the previous calls.");
		#endif
		return item;
	}
	const T* operator*() const
	{
		#ifndef NDEBUG
		if (version != *poolVersion)
			throw runtime_error("Item has been invalidated by the previous calls.");
		#endif
		return item;
	}
};

//------------------------------------------------------------------------------------------------------------
template<typename T>
struct Ref final
{
private:
	atomic<int64_t>* counter = nullptr;
	ID<T> item = {};
public:
	Ref() = default;
	Ref(ID<T> item)
	{
		if (item) counter = new atomic<int64_t>(1);
		this->item = item;
	}
	~Ref()
	{
		if (!item) return;
		auto count = counter->fetch_sub(1);
		assert(count >= 1);
		if (count > 1) return;
		delete counter;
	}

	Ref(const Ref& ref)
	{
    	if (!ref.counter) return;
		counter = ref.counter;
		counter->fetch_add(1);
		item = ref.item;
	}
	Ref& operator=(const Ref& ref)
	{
		if (item)
		{
			auto count = counter->fetch_sub(1);
			assert(count >= 1);
			if (count <= 1) delete counter;
		}

		counter = ref.counter;
		item = ref.item;
		if (counter) counter->fetch_add(1);
		return *this;
	}

	Ref(Ref&& ref)
	{
		counter = ref.counter;
		item = ref.item;
		ref.counter = nullptr;
		ref.item = ID<T>();
	}
	Ref& operator=(Ref&& ref) noexcept
	{
		counter = ref.counter;
		item = ref.item;
		ref.counter = nullptr;
		ref.item = ID<T>();
		return *this;
	}
	
	operator ID<T>() const noexcept { return item; }
	operator bool() const noexcept { return item; }
	uint32_t operator*() const noexcept { return *item; }

	int64_t getRefCount() const noexcept
	{
		if (!item) return 0;
		return counter->load();
	}
};

//------------------------------------------------------------------------------------------------------------
// DestroyItems - is linear pool should call destroy() function of the item.
template<class T, bool DestroyItems /* = true */>
class LinearPool
{
	T* items = nullptr;
	uint32_t occupancy = 0, capacity = 1;
	stack<ID<T>> freeItems;
	vector<ID<T>> garbageItems;

	#ifndef NDEBUG
	vector<bool> isAllocated;
	size_t version = 0;
	bool isChanging = false;
	#endif
public:
	LinearPool() { items = new T[1]; }
	~LinearPool()
	{
		if constexpr (DestroyItems)
		{
			if (occupancy - (uint32_t)freeItems.size() > 0)
			{
				for (uint32_t i = 0; i < occupancy; i++)
					items[i].destroy();
			}
		}
		
		delete[] items;
	}

//------------------------------------------------------------------------------------------------------------
	template<typename... Args>
	ID<T> create(Args&&... args)
	{
		#ifndef NDEBUG
		if (isChanging)
		{
			throw runtime_error("Creation of the item inside "
				"other creation/destruction/clear is not allowed.");
		}
		isChanging = true;
		#endif

		if (!freeItems.empty())
		{
			auto freeItem = freeItems.top();
			freeItems.pop();
			auto& item = items[*freeItem - 1];
			item = T(std::forward<Args>(args)...);
			#ifndef NDEBUG
			isAllocated[*freeItem - 1] = true;
			isChanging = false;
			#endif
			return freeItem;
		}
		if (occupancy == capacity)
		{
			capacity *= 2;
			T* newItems = new T[capacity];
			for (uint32_t i = 0; i < occupancy; i++)
				newItems[i] = std::move(items[i]);
			delete[] items;
			items = newItems;
		}

		items[occupancy] = T(std::forward<Args>(args)...);

		#ifndef NDEBUG
		isAllocated.push_back(true);
		isChanging = false;
		version++;
		#endif
		return ID<T>(++occupancy);
	}
	void destroy(ID<T> instance)
	{
		if (!instance) return;
		#ifndef NDEBUG
		assert(*instance - 1 < occupancy);
		version++; // Protects from the use after free.
		#endif
		garbageItems.push_back(instance);
	}

//------------------------------------------------------------------------------------------------------------
	View<T> get(ID<T> instance) const
	{
		assert(instance);
		assert(*instance - 1 < occupancy);
		assert(isAllocated[*instance - 1]);
		#ifndef NDEBUG
		return View(&items[*instance - 1], version);
		#else
		return View(&items[*instance - 1]);
		#endif
	}
	ID<T> getID(const T* instance) const
	{
		#ifndef NDEBUG
		if (instance < items || instance >= items + capacity)
			throw runtime_error("Out of items array bounds.");
		#endif
		return ID<T>((uint32_t)(instance - items) + 1);
	}

	// WARNING: Contains destroyed items!
	T* getData() noexcept { return items; }
	// WARNING: Contains destroyed items!
	const T* getData() const noexcept { return items; }
	
	uint32_t getCount() const noexcept { return occupancy - (uint32_t)freeItems.size(); }
	uint32_t getOccupancy() const noexcept { return occupancy; }

//------------------------------------------------------------------------------------------------------------
	void clear(bool destroyItems = DestroyItems)
	{
		#ifndef NDEBUG
		if (destroyItems && !DestroyItems)
			throw runtime_error("Item does not have destroy function.");

		if (isChanging)
		{
			throw runtime_error("Clear of the items inside "
				"other creation/destruction/clear is not allowed.");
		}
		isChanging = true;
		#endif

		if (occupancy - (uint32_t)freeItems.size() > 0)
		{
			if (destroyItems)
			{
				for (uint32_t i = 0; i < occupancy; i++)
				{
					items[i].destroy();
					items[i] = T();
				}
			}
			else
			{
				for (uint32_t i = 0; i < occupancy; i++) items[i] = T();
			}
		}

		delete[] items;
		items = new T[1];
		occupancy = 0;
		capacity = 1;
		freeItems = {};
		garbageItems = {};

		#ifndef NDEBUG
		isAllocated = {};
		version = 0;
		isChanging = false;
		#endif
	}

//------------------------------------------------------------------------------------------------------------
	void dispose()
	{
		if constexpr (DestroyItems)
		{
			for (int64_t i = garbageItems.size() - 1; i >= 0; i--)
			{
				auto item = garbageItems[i];
				auto index = *item - 1;
				if (!items[index].destroy()) continue;

				#ifndef NDEBUG
				isAllocated[index] = false;
				#endif					
				items[index] = T();
				freeItems.push(item);
				garbageItems.erase(garbageItems.begin() + i);
			}
		}
		else
		{
			for (auto item : garbageItems)
			{
				auto index = *item - 1;
				#ifndef NDEBUG
				isAllocated[index] = false;
				#endif
				items[index] = T();
				freeItems.push(item);
			}
			garbageItems.clear();
		}
	}
};

} // ecsm