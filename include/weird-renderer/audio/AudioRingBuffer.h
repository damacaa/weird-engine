#pragma once

#include <atomic>
#include <optional>
#include <vector>

// Single-Producer Single-Consumer (SPSC) Ring Buffer
template <typename T, size_t Capacity> class AudioRingBuffer
{
private:
	std::vector<T> buffer;
	std::atomic<size_t> head{0}; // Write index
	std::atomic<size_t> tail{0}; // Read index

public:
	AudioRingBuffer()
		: buffer(Capacity)
	{
	}

	// Called by Physics Thread
	bool push(const T& item)
	{
		const size_t currentHead = head.load(std::memory_order_relaxed);
		const size_t nextHead = (currentHead + 1) % Capacity;

		// If next head hits tail, we are full
		if (nextHead == tail.load(std::memory_order_acquire))
		{
			return false; // Drop audio event if full (better than blocking physics)
		}

		buffer[currentHead] = item;
		head.store(nextHead, std::memory_order_release);
		return true;
	}

	// Called by Audio/Render Thread
	bool pop(T& outItem)
	{
		const size_t currentTail = tail.load(std::memory_order_relaxed);

		if (empty())
		{
			return false; // Empty
		}

		outItem = buffer[currentTail];
		tail.store((currentTail + 1) % Capacity, std::memory_order_release);
		return true;
	}

	bool empty()
	{
		return head.load(std::memory_order_acquire) == tail.load(std::memory_order_acquire);
	}
};