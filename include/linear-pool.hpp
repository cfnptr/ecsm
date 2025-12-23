// Copyright 2022-2025 Nikita Fediuchin. All rights reserved.
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
 */

#pragma once
#include "ecsm-error.hpp"

#include <stack>
#include <vector>
#include <atomic>
#include <cstddef>
#include <cassert>
#include <cstring>
#include <iterator>

namespace ecsm
{

template<class T>
struct OptView;
template<class T, bool Destroy = true>
class LinearPool;

/**
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

	constexpr ID(uint32_t index) noexcept : index(index) { }

	friend class LinearPool<T, true>;
	friend class LinearPool<T, false>;
public:
	constexpr ID() noexcept = default;

	/**
	 * @brief Changes the type of the identifier item.
	 * @details Useful in cases where we need to cast item identifier type.
	 * 
	 * @tparam U is a new type of the identifier item
	 * @param id item identifier in the linear pool
	 */
	template<class U>
	constexpr explicit ID(ID<U> id) noexcept : index(*id) { }

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
	constexpr bool operator==(ID v) const noexcept { return index == v.index; }
	/**
	 * @brief Returns true if this identifier is not equal to the v identifier.
	 * @param v other identifier value
	 */
	constexpr bool operator!=(ID v) const noexcept { return index != v.index; }
	/**
	 * @brief Returns true if this identifier is less than the v identifier.
	 * @param v other identifier value
	 */
	constexpr bool operator<(ID v) const noexcept { return index < v.index; }

	/**
	 * @brief Returns true if item is not null.
	 */
	constexpr explicit operator bool() const noexcept { return index; }
};

/***********************************************************************************************************************
 * @brief View of the item in the @ref LinearPool. (Non-nullable)
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

	constexpr View() noexcept = default;
	#ifndef NDEBUG
	constexpr View(T* item, const std::atomic_uint64_t& poolVersion) noexcept :
		item(item), poolVersion(&poolVersion), version(poolVersion) { }
	#else
	constexpr View(T* item) noexcept : item(item) { }
	#endif
	
	friend struct OptView<T>;
	friend class LinearPool<T, true>;
	friend class LinearPool<T, false>;
public:
	/**
	 * @brief Changes the type of the item view.
	 * @details Useful in cases where we need to cast item view type.
	 * 
	 * @tparam U is a new type of the item view
	 * @param[in] view target item view
	 */
	template<class U>
	constexpr explicit View(const View<U>& view) noexcept : item((T*)*view)
	#ifndef NDEBUG
		, poolVersion(view.getPoolVersion()), version(view.getViewVersion())
	#endif
	{ }

	/**
	 * @brief View item data accessor.
	 */
	T* operator->()
	{
		#ifndef NDEBUG
		if (poolVersion && version != *poolVersion)
			throw EcsmError("Item has been invalidated by the previous calls.");
		#endif
		return item;
	}
	/**
	 * @brief View item constant data accessor.
	 */
	const T* operator->() const
	{
		#ifndef NDEBUG
		if (poolVersion && version != *poolVersion)
			throw EcsmError("Item has been invalidated by the previous calls.");
		#endif
		return item;
	}

	/**
	 * @brief Returns pointer to the item memory in the pool.
	 */
	T* operator*()
	{
		#ifndef NDEBUG
		if (poolVersion && version != *poolVersion)
			throw EcsmError("Item has been invalidated by the previous calls.");
		#endif
		return item;
	}
	/**
	 * @brief Returns pointer to the constant item memory in the pool.
	 */
	const T* operator*() const
	{
		#ifndef NDEBUG
		if (poolVersion && version != *poolVersion)
			throw EcsmError("Item has been invalidated by the previous calls.");
		#endif
		return item;
	}

	#ifndef NDEBUG
private:
	const std::atomic_uint64_t* poolVersion = nullptr;
	uint64_t version = 0;
public:
	const std::atomic_uint64_t* getPoolVersion() const noexcept { return poolVersion; }
	uint64_t getViewVersion() const noexcept { return version; }
	#endif
};

/***********************************************************************************************************************
 * @brief Optional view of the item in the @ref LinearPool. (Nullable)
 * @tparam T type of the item in the linear pool
 * @details See the @ref View<T>.
 */
template<class T>
struct OptView final
{
private:
	View<T> view = {};
public:
	/**
	 * @brief Creates a new null optional view.
	 */
	constexpr OptView() noexcept = default;

	/**
	 * @brief Creates a new optional view. (Nullable)
	 *
	 * @tparam U is a new type of the item view
	 * @param[in] view target item view
	 */
	template<class U>
	constexpr explicit OptView(const View<U>& view) noexcept : view(view)
	#ifndef NDEBUG
		, nullChecked(true)
	#endif
	{ }
	/**
	 * @brief Changes the type of the item optional view. (Nullable)
	 * @details Useful in cases where we need to cast item view type.
	 * 
	 * @tparam U is a new type of the item view
	 * @param[in] view target item view
	 */
	template<class U>
	constexpr explicit OptView(const OptView<U>& view) noexcept : view(view.getView_())
	#ifndef NDEBUG
		, nullChecked(view.isNullChecked())
	#endif
	{ }

	/**
	 * @brief Returns true if item optional view is not null.
	 */
	constexpr explicit operator bool() noexcept
	{
		#ifndef NDEBUG
		nullChecked = true;
		#endif
		return view.item;
	}
	/**
	 * @brief Converts optional view to the non-nullable view.
	 */
	constexpr explicit operator const View<T>&() const
	{
		#ifndef NDEBUG
		if (!nullChecked)
			throw EcsmError("Item was not checked for null.");
		if (!view.item)
			throw EcsmError("Item is null.");
		#endif
		return view;
	}

	/**
	 * @brief Optional view item data accessor.
	 */
	T* operator->()
	{
		#ifndef NDEBUG
		if (!nullChecked)
			throw EcsmError("Item was not checked for null.");
		if (!view.item)
			throw EcsmError("Item is null.");
		#endif
		return view.operator->();
	}
	/**
	 * @brief Optional view item constant data accessor.
	 */
	const T* operator->() const
	{
		#ifndef NDEBUG
		if (!nullChecked)
			throw EcsmError("Item was not checked for null.");
		if (!view.item)
			throw EcsmError("Item is null.");
		#endif
		return view.operator->();
	}

	/**
	 * @brief Returns pointer to the item memory in the pool.
	 */
	T* operator*()
	{
		#ifndef NDEBUG
		if (!nullChecked)
			throw EcsmError("Item was not checked for null.");
		if (!view.item)
			throw EcsmError("Item is null.");
		#endif
		return view.operator*();
	}
	/**
	 * @brief Returns pointer to the constant item memory in the pool.
	 */
	const T* operator*() const
	{
		#ifndef NDEBUG
		if (!nullChecked)
			throw EcsmError("Item was not checked for null.");
		if (!view.item)
			throw EcsmError("Item is null.");
		#endif
		return view.operator*();
	}

	#ifndef NDEBUG
private:
	volatile bool nullChecked = false;
public:
	bool isNullChecked() const noexcept { return nullChecked; }
	#endif
	const View<T>& getView_() const noexcept { return view; }
};

/***********************************************************************************************************************
 * @brief Item identifier in the @ref LinearPool with usage counter.
 * @tparam T type of the item in the linear pool
 * 
 * @details
 * Useful in cases where we need to track item usage in the program and 
 * destroy it when it's not needed anymore. Also see the @ref ID<T>.
 */
template<typename T>
struct Ref final
{
private:
	std::atomic_int64_t* counter = nullptr;
	ID<T> item = {};
public:
	constexpr Ref() noexcept = default;

	/**
	 * @brief Creates a new item reference. (Allocates counter)
	 * @param item target item instance
	 * @tparam T type of the item in the linear pool
	 */
	constexpr explicit Ref(ID<T> item) : item(item)
	{
		if (item)
			counter = new std::atomic<int64_t>(1);
	}
	/**
	 * @brief Destroys item reference. (Decrements or deallocates counter)
	 */
	~Ref()
	{
		if (!item)
			return;
		if (counter->fetch_sub(1, std::memory_order_release) == 1)
		{
			std::atomic_thread_fence(std::memory_order_acquire);
			delete counter;
		}
	}

	Ref(const Ref& ref) noexcept
	{
    	if (!ref.item)
			return;
		ref.counter->fetch_add(1, std::memory_order_relaxed);
		counter = ref.counter;
		item = ref.item;
	}
	Ref(Ref&& ref) noexcept
	{
		counter = ref.counter;
		item = ref.item;
		ref.counter = nullptr;
		ref.item = ID<T>();
	}

	Ref& operator=(const Ref& ref) noexcept
	{
		if (this != &ref)
		{
			if (item && counter->fetch_sub(1, std::memory_order_release) == 1)
			{
				std::atomic_thread_fence(std::memory_order_acquire);
				delete counter;
			}

			if (ref.item)
				ref.counter->fetch_add(1, std::memory_order_relaxed);
			counter = ref.counter;
			item = ref.item;
		}
		return *this;
	}
	Ref& operator=(Ref&& ref) noexcept
	{
		if (this != &ref)
		{
			if (item && counter->fetch_sub(1, std::memory_order_release) == 1)
			{
				std::atomic_thread_fence(std::memory_order_acquire);
				delete counter;
			}

			counter = ref.counter;
			item = ref.item;
			ref.counter = nullptr;
			ref.item = ID<T>();
		}
		return *this;
	}

	/*******************************************************************************************************************
	 * @brief Returns current item reference count.
	 */
	int64_t getRefCount() const noexcept
	{
		if (!item)
			return 0;
		return counter->load(std::memory_order_relaxed);
	}
	/**
	 * @brief Returns true if this is last item reference.
	 */
	bool isLastRef() const noexcept
	{
		if (!item)
			return false;
		return counter->load(std::memory_order_relaxed) == 1;
	}

	/**
	 * @brief Returns true if this reference item is equal to the v reference item.
	 * @param v other reference value
	 */
	constexpr bool operator==(const Ref& v) const noexcept { return item == v.item; }
	/**
	 * @brief Returns true if this reference item is not equal to the v reference item.
	 * @param v other reference value
	 */
	constexpr bool operator!=(const Ref& v) const noexcept { return item != v.item; }
	/**
	 * @brief Returns true if this reference item is less than the v reference item.
	 * @param v other reference value
	 */
	constexpr bool operator<(const Ref& v) const noexcept { return item < v.item; }

	/**
	 * @brief Returns true if this reference item is equal to the v identifier.
	 * @param v other reference value
	 */
	constexpr bool operator==(ID<T> v) const noexcept { return item == v; }
	/**
	 * @brief Returns true if this reference item is not equal to the v identifier.
	 * @param v other reference value
	 */
	constexpr bool operator!=(ID<T> v) const noexcept { return item != v; }
	/**
	 * @brief Returns true if this reference item is less than the v identifier.
	 * @param v other reference value
	 */
	constexpr bool operator<(ID<T> v) const noexcept { return item < v; }

	/**
	 * @brief Returns reference item @ref ID.
	 * @tparam T type of the item in the linear pool
	 */
	constexpr explicit operator ID<T>() const noexcept { return item; }
	/**
	 * @brief Returns true if reference item is not null.
	 */
	constexpr explicit operator bool() const noexcept { return (bool)item; }
	/**
	 * @brief Returns reference item index + 1 in the linear pool.
	 */
	uint32_t operator*() const noexcept { return *item; }
};

/***********************************************************************************************************************
 * @brief Returns true if i identifier is equal to the r reference item.
 * @param r reference value
 * @param i identifier value
 * @tparam T type of the item in the linear pool
 */
template<typename T>
static constexpr bool operator==(ID<T> i, const Ref<T>& r) noexcept { return i == ID<T>(r); }
/**
 * @brief Returns true if i identifier is not equal to the t reference item.
 * @param r reference value
 * @param i identifier value
 * @tparam T type of the item in the linear pool
 */
template<typename T>
static constexpr bool operator!=(ID<T> i, const Ref<T>& r) noexcept { return i != ID<T>(r); }
/**
 * @brief Returns true if i identifier is less than the t reference item.
 * @param r reference value
 * @param i identifier value
 * @tparam T type of the item in the linear pool
 */
template<typename T>
static constexpr bool operator<(ID<T> i, const Ref<T>& r) noexcept { return i < ID<T>(r); }

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
 * @tparam DestroyItems linear pool should call destroy() function of the items
 */
template<class T, bool DestroyItems /* = true */>
class LinearPool final
{
	T* items = nullptr;
	uint32_t occupancy = 0, capacity = 1;
	std::stack<ID<T>, std::vector<ID<T>>> freeItems;
	std::vector<ID<T>> garbageItems;

	#ifndef NDEBUG
	std::atomic_uint64_t version = 0;
	bool isChanging = false;
	#endif
public:
	typedef T ItemType; /**< Type of the item in the linear pool. */

	/**
	 * @brief Creates a new empty linear pool.
	 * @details It pre-allocates items array.
	 */
	LinearPool() { items = new T[1]; }

	/**
	 * @brief Destroys linear pool.
	 * @details It destroys all items and deallocates array memory.
	 */
	~LinearPool()
	{
		#ifndef NDEBUG
		if (isChanging)
			abort(); // Destruction of the items inside other create/destroy is not allowed.
		isChanging = true;
		#endif
		
		if constexpr (DestroyItems)
		{
			if (occupancy - (uint32_t)freeItems.size() != 0)
			{
				for (uint32_t i = 0; i < occupancy; i++)
					items[i].destroy();
			}
		}
		
		delete[] items;

		#ifndef NDEBUG
		isChanging = false;
		#endif
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
			throw EcsmError("Creation of the item inside other create/dispose/clear is not allowed.");
		isChanging = true;
		#endif

		if (!freeItems.empty())
		{
			auto freeItem = freeItems.top();
			freeItems.pop();
			auto& item = items[*freeItem - 1];
			item = T(std::forward<Args>(args)...);

			#ifndef NDEBUG
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
		isChanging = false;
		version++; // Protects from the use after free.
		#endif

		return ID<T>(++occupancy);
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
		if (!instance)
			return;

		#ifndef NDEBUG
		assert(*instance - 1 < occupancy);
		for (auto item : garbageItems)
			assert(item != instance); // Second item destroy detected.
		version++; // Protects from the use after free.
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
		#ifndef NDEBUG
		return View(&items[*instance - 1], version);
		#else
		return View(&items[*instance - 1]);
		#endif
	}
	/**
	 * @brief Returns @ref View of the item in the linear pool.
	 * @warning Do not store views, use them only in place. Because item memory can be reallocated later.
	 * 
	 * @param instance item identifier in the pool
	 * @tparam T type of the item in the linear pool
	 */
	View<T> get(const Ref<T>& instance) const noexcept
	{
		assert(instance);
		assert(*instance - 1 < occupancy);
		#ifndef NDEBUG
		return View(&items[*instance - 1], version);
		#else
		return View(&items[*instance - 1]);
		#endif
	}

	/**
	 * @brief Returns @ref ID of the item pointer.
	 * @warning Use with extreme caution!
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
			throw EcsmError("Item does not have destroy function.");
		if (isChanging)
			throw EcsmError("Clear of the items inside other create/dispose/clear is not allowed.");
		isChanging = true;
		#endif

		if constexpr (DestroyItems)
		{
			if (occupancy - (uint32_t)freeItems.size() != 0 && destroyItems)
			{
				for (uint32_t i = 0; i < occupancy; i++)
					items[i].destroy();
			}
		}

		delete[] items;
		items = new T[1];
		occupancy = 0;
		capacity = 1;
		freeItems = {};
		garbageItems = {};

		#ifndef NDEBUG
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
		#ifndef NDEBUG
		if (isChanging)
			throw EcsmError("Destruction of the items inside other create/dispose/clear is not allowed.");
		isChanging = true;
		#endif

		if constexpr (DestroyItems)
		{
			auto garbageData = garbageItems.data();
			size_t i = 0, n = garbageItems.size();

			while (i < n)
			{
				auto item = garbageData[i];
				if (items[*item - 1].destroy())
				{
					items[*item - 1] = T();
					freeItems.push(item);
					garbageData[i] = garbageData[n - 1];
					n--;
				}
				else
				{
					i++;
				}
			}

			garbageItems.resize(n);
		}
		else
		{
			for (auto item : garbageItems)
			{
				items[*item - 1] = T();
				freeItems.push(item);
			}
			garbageItems.clear();
		}

		#ifndef NDEBUG
		isChanging = false;
		#endif
	}

	/*******************************************************************************************************************
	 * @brief Linear pool iterator class.
	 */
	struct Iterator
	{
		using iterator_category = std::random_access_iterator_tag;
		using value_type = T;
		using difference_type = ptrdiff_t;
		using pointer = value_type*;
		using reference = value_type&;
	private:
		pointer ptr = nullptr;
	public:
		Iterator(pointer ptr) noexcept : ptr(ptr) { }
		Iterator& operator=(const Iterator& i) noexcept = default;
		Iterator& operator=(pointer ptr) noexcept { this->ptr = ptr; return (*this); }

		operator bool() const noexcept { return ptr; }

		bool operator==(const Iterator& i) const noexcept { return ptr == i.ptr; }
		bool operator!=(const Iterator& i) const noexcept { return ptr != i.ptr; }

		Iterator& operator+=(const difference_type& m) noexcept { ptr += m; return *this; }
		Iterator& operator-=(const difference_type& m) noexcept { ptr -= m; return *this; }
		Iterator& operator++() noexcept { ++ptr; return *this; }
		Iterator& operator--() noexcept { --ptr; return *this; }
		Iterator operator++(int) noexcept { auto tmp = *this; ++ptr; return tmp; }
		Iterator operator--(int) noexcept { auto tmp = *this; --ptr; return tmp; }
		Iterator operator+(const difference_type& m) noexcept { return Iterator(ptr + m); }
		Iterator operator-(const difference_type& m) noexcept { return Iterator(ptr - m); }
		difference_type operator-(const Iterator& i) noexcept { return std::distance(i.ptr, ptr); }

		reference operator*() const noexcept { return *ptr; }
		pointer operator->() noexcept { return ptr; }
	};

	/*******************************************************************************************************************
	 * @brief Linear pool constant iterator class.
	 */
	struct ConstantIterator
	{
		using iterator_category = std::random_access_iterator_tag;
		using value_type = const T;
		using difference_type = ptrdiff_t;
		using pointer = value_type*;
		using reference = value_type&;
	private:
		pointer ptr = nullptr;
	public:
		ConstantIterator(pointer ptr) noexcept : ptr(ptr) { }
		ConstantIterator& operator=(const ConstantIterator& i) noexcept = default;
		ConstantIterator& operator=(pointer ptr) noexcept { this->ptr = ptr; return (*this); }

		operator bool() const noexcept { return ptr; }

		bool operator==(const ConstantIterator& i) const noexcept { return ptr == i.ptr; }
		bool operator!=(const ConstantIterator& i) const noexcept { return ptr != i.ptr; }

		ConstantIterator& operator+=(const difference_type& m) noexcept { ptr += m; return *this; }
		ConstantIterator& operator-=(const difference_type& m) noexcept { ptr -= m; return *this; }
		ConstantIterator& operator++() noexcept { ++ptr; return *this; }
		ConstantIterator& operator--() noexcept { --ptr; return *this; }
		ConstantIterator operator++(int) noexcept { auto tmp = *this; ++ptr; return tmp; }
		ConstantIterator operator--(int) noexcept { auto tmp = *this; --ptr; return tmp; }
		ConstantIterator operator+(const difference_type& m) noexcept { return ConstantIterator(ptr + m); }
		ConstantIterator operator-(const difference_type& m) noexcept { return ConstantIterator(ptr - m); }
		difference_type operator-(const ConstantIterator& i) noexcept { return std::distance(i.ptr, ptr); }

		reference operator*() const noexcept { return *ptr; }
		pointer operator->() noexcept { return ptr; }
	};

	/**
	 * @brief Returns an iterator pointing to the first element in the items array.
	 */
	Iterator begin() noexcept { return Iterator(&items[0]); }
	/**
	 * @brief Returns an iterator pointing to the past-the-end element in the items array.
	 */
	Iterator end() noexcept { return Iterator(&items[capacity]); }

	/**
	 * @brief Returns a constant iterator pointing to the first element in the items array.
	 */
	ConstantIterator begin() const noexcept { return ConstantIterator(&items[0]); }
	/**
	 * @brief Returns a constant iterator pointing to the past-the-end element in the items array.
	 */
	ConstantIterator end() const noexcept { return ConstantIterator(&items[capacity]); }
};

} // namespace ecsm

namespace std
{
	template<typename T>
	struct hash<ecsm::ID<T>>
	{
		size_t operator()(const ecsm::ID<T>& id) const noexcept
		{
			return std::hash<uint32_t>{}(*id);
		}
	};
} // namespace std