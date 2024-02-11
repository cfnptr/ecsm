// Copyright 2022-2024 Nikita Fediuchin. All rights reserved.
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

/***********************************************************************************************************************
 * @file
 **********************************************************************************************************************/

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

/***********************************************************************************************************************
 * @brief Item identifier in the @ref LinearPool.
 * @tparam T type of the item in the linear pool
 * 
 * @details
 * Identifier or index associated with individual items within the pool. Each item in the pool 
 * can be uniquely identified by its identifier, which helps in managing and referencing items.
 */
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

	/**
	 * @brief Changes the type of the identifier item.
	 * @details Useful in cases where we need to cast item identifier type.
	 * 
	 * @tparam U is a new type of the identifier item
	 * @param id item identifier in the linear pool
	 */
	template<class U>
	explicit ID(ID<U> id) noexcept : index(*id)
	#ifdef ECSM_DEEP_ID_TRACKING
		, version(id.getVersion())
	#endif
	{ }

	/**
	 * @brief Returns item index + 1 in the linear pool.
	 * @details Used to get internal integer index.
	 */
	uint32_t operator*() const noexcept { return index; }
	/**
	 * @brief Returns reference to the item index + 1 in the linear pool.
	 * @note You can use it to set the identifier index value.
	 */
	uint32_t& operator*() noexcept { return index; }

	/**
	 * @brief Returns true if this identifier is equal to the v identifier.
	 * @param v other identifier value
	 */
	bool operator==(ID<T> v) const noexcept { return index == v.index; }
	/**
	 * @brief Returns true if this identifier is not equal to the v identifier.
	 * @param v other identifier value
	 */
	bool operator!=(ID<T> v) const noexcept { return index != v.index; }
	/**
	 * @brief Returns true if this identifier is less than the v identifier.
	 * @param v other identifier value
	 */
	bool operator<(ID<T> v) const noexcept { return index < v.index; }
	/**
	 * @brief Returns true if item is not null.
	 */
	operator bool() const noexcept { return index; }

#ifdef ECSM_DEEP_ID_TRACKING
private:
	uint64_t version = 0;
public:
	uint64_t getVersion() const noexcept { return version; }
#endif
};

/***********************************************************************************************************************
 * @brief View of the item in the @ref LinearPool.
 * @tparam T type of the item in the linear pool
 * 
 * @details
 * The view provides a way to access the contents of an item 
 * within the pool, allowing to inspect or modify its data.
 */
template<class T>
struct View final
{
private:
	T* item = nullptr;

	#ifndef NDEBUG
	/**
	 * @brief Creates a new view.
	 */
	View(T* item, const uint64_t& poolVersion) noexcept
	{
		this->item = item;
		this->poolVersion = &poolVersion;
		this->version = poolVersion;
	}
	#else
	/**
	 * @brief Creates a new view.
	 */
	View(T* item) noexcept { this->item = item; }
	#endif
	
	friend class LinearPool<T, true>;
	friend class LinearPool<T, false>;
public:
	/**
	 * @brief Creates a new null item view.
	 */
	View() = default;

	/**
	 * @brief Changes the type of the item view.
	 * @details Useful in cases where we need to cast item view type.
	 * 
	 * @tparam U is a new type of the item view
	 * @param[in] view target item view
	 */
	template<class U>
	explicit View(const View<U>& view) noexcept : item((T*)*view)
	#ifndef NDEBUG
		, poolVersion(view.getPoolVersion()), version(view.getVersion())
	#endif
	{ }
	
	/**
	 * @brief Returns true if item is not null.
	 */
	operator bool() const noexcept { return item; }

	/**
	 * @brief Item data accessor.
	 */
	T* operator->()
	{
		#ifndef NDEBUG
		if (version != *poolVersion)
			throw runtime_error("Item has been invalidated by the previous calls.");
		#endif
		return item;
	}
	/**
	 * @brief Item constant data accessor.
	 */
	const T* operator->() const
	{
		#ifndef NDEBUG
		if (version != *poolVersion)
			throw runtime_error("Item has been invalidated by the previous calls.");
		#endif
		return item;
	}

	/**
	 * @brief Returns pointer to the item memory in the pool.
	 */
	T* operator*()
	{
		#ifndef NDEBUG
		if (version != *poolVersion)
			throw runtime_error("Item has been invalidated by the previous calls.");
		#endif
		return item;
	}
	/**
	 * @brief Returns pointer to the constant item memory in the pool.
	 */
	const T* operator*() const
	{
		#ifndef NDEBUG
		if (version != *poolVersion)
			throw runtime_error("Item has been invalidated by the previous calls.");
		#endif
		return item;
	}

#ifndef NDEBUG
private:
	const uint64_t* poolVersion = nullptr;
	uint64_t version = 0;
public:
	const uint64_t* getPoolVersion() const noexcept { return poolVersion; }
	uint64_t getVersion() const noexcept { return version; }
#endif
};

/***********************************************************************************************************************
 * @brief Item identifier in the @ref LinearPool with usage counter.
 * @tparam T type of the item in the linear pool
 * 
 * @details
 * Useful in cases where we need to track item usage in the 
 * program and destroy it when it's not needed anymore.
 */
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
	
	/**
	 * @brief Returns item @ref ID.
	 */
	operator ID<T>() const noexcept { return item; }
	/**
	 * @brief Returns true if item is not null.
	 */
	operator bool() const noexcept { return item; }
	/**
	 * @brief Returns item index + 1 in the linear pool.
	 */
	uint32_t operator*() const noexcept { return *item; }

	/**
	 * @brief Returns current item reference count.
	 */
	int64_t getRefCount() const noexcept
	{
		if (!item) return 0;
		return counter->load();
	}
};

/***********************************************************************************************************************
 * @brief Item array with linear memory block.
 * 
 * @details
 * In a linear pool, a fixed-size block of memory is pre-allocated, and individual items or objects are then 
 * allocated from this pool. The linear allocation strategy means that these items are placed sequentially in memory, 
 * which can improve cache locality. Cache locality refers to the tendency of accessing nearby memory locations at 
 * the same time, which can result in better performance due to the way modern computer architectures use caches.
 * 
 * @tparam T type of the item in the linear pool
 * @tparam DestroyItems linear pool should call destroy() function of the items.
 */
template<class T, bool DestroyItems /* = true */>
class LinearPool
{
	T* items = nullptr;
	uint32_t occupancy = 0, capacity = 1;
	stack<ID<T>> freeItems;
	vector<ID<T>> garbageItems;

	#ifndef NDEBUG
	vector<bool> isAllocated;
	uint64_t version = 0;
	bool isChanging = false;
	#endif

	#ifdef ECSM_DEEP_ID_TRACKING
	vector<uint64_t> itemVersions;
	#endif
public:
	LinearPool() { items = new T[1]; }

	/**
	 * @brief Destroys linear pool items.
	 */
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

	/**
	 * @brief Creates a new item in the pool.
	 * @details Reallocates linear memory block or reuses free item slots.
	 * @warning This function can reallocate items memory and invalidate all previous @ref View.
	 * 
	 * @param args additional item arguments or empty
	 * @return A new item identifier in the linear pool.
	 */
	template<typename... Args>
	ID<T> create(Args&&... args)
	{
		#ifndef NDEBUG
		if (isChanging)
			throw runtime_error("Creation of the item inside other creation/destruction/clear is not allowed.");
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

			#ifdef ECSM_DEEP_ID_TRACKING
			freeItem.version = ++itemVersions[*freeItem - 1];
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
		auto id = ID<T>(++occupancy);

		#ifndef NDEBUG
		isAllocated.push_back(true);
		isChanging = false;
		version++;
		#endif

		#ifdef ECSM_DEEP_ID_TRACKING
		id.version = 1;
		itemVersions.push_back(1);
		#endif

		return id;
	}

	/**
	 * @brief Destroys linear pool item.
	 * @details It puts items to the garbage array, and destroys them after @ref dispose() call.
	 * 
	 * @param instance item identifier in the pool or null
	 * @tparam T type of the item in the linear pool
	 */
	void destroy(ID<T> instance)
	{
		if (!instance) return;
		#ifndef NDEBUG
		assert(*instance - 1 < occupancy);
		version++; // Protects from the use after free.
		#endif
		#ifdef ECSM_DEEP_ID_TRACKING
		assert(instance.version == itemVersions[*instance - 1]);
		#endif
		garbageItems.push_back(instance);
	}

	/*******************************************************************************************************************
	 * @brief Returns @ref View of the item in the linear pool.
	 * @warning Do not store views, use them only in place. Because item memory can be reallocated later.
	 * 
	 * @param instance item identifier in the pool
	 * @tparam T type of the item in the linear pool
	 */
	View<T> get(ID<T> instance) const noexcept
	{
		assert(instance);
		assert(*instance - 1 < occupancy);
		assert(isAllocated[*instance - 1]);
		#ifdef ECSM_DEEP_ID_TRACKING
		assert(instance.version == itemVersions[*instance - 1]);
		#endif
		#ifndef NDEBUG
		return View(&items[*instance - 1], version);
		#else
		return View(&items[*instance - 1]);
		#endif
	}
	/**
	 * @brief Returns @ref ID of the item pointer.
	 * 
	 * @param[in] instance pointer to the item data
	 * @tparam T type of the item in the linear pool
	 */
	ID<T> getID(const T* instance) const noexcept
	{
		assert(instance >= items);
		assert(instance < items + capacity);
		return ID<T>((uint32_t)(instance - items) + 1);
	}

	/**
	 * @brief Returns linear pool item memory block.
	 * @warning It contains destroyed items too. Use custom code to detect freed items.
	 * @tparam T type of the item in the linear pool
	 */
	T* getData() noexcept { return items; }
	/**
	 * @brief Returns linear pool constant item memory block.
	 * @warning It contains destroyed items too. Use custom code to detect freed items.
	 * @tparam T type of the item in the linear pool
	 */
	const T* getData() const noexcept { return items; }
	
	/**
	 * @brief Returns current created item count.
	 * @note This is not an allocated item count.
	 */
	uint32_t getCount() const noexcept { return occupancy - (uint32_t)freeItems.size(); }
	/**
	 * @brief Returns linear memory used item slots count.
	 * @warning This number also contains destroyed items.
	 */
	uint32_t getOccupancy() const noexcept { return occupancy; }

	/*******************************************************************************************************************
	 * @brief Destroys all items in the linear pool.
	 * @warning This function deallocates items memory and invalidates all previous @ref View.
	 * @param destroyItems should call destroy() function of the items
	 */
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

	/*******************************************************************************************************************
	 * @brief Actually destroys items.
	 * @details See the @ref destroy(). It marks destroyed item memory as free, and can reuse it later.
	 */
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

} // namespace ecsm