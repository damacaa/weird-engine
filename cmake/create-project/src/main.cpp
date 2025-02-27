
// Claude's code

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring> // For memcpy
#include <immintrin.h> // For SIMD intrinsics
#include <iostream>
#include <memory>
#include <vector>

// Forward declarations
class SpatialGrid;

// #define __AVX__

// Aligned memory allocation
void* aligned_alloc(size_t alignment, size_t size)
{
	void* ptr = nullptr;
#ifdef _WIN32
	ptr = _aligned_malloc(size, alignment);
#else
	posix_memalign(&ptr, alignment, size);
#endif
	return ptr;
}

void aligned_free(void* ptr)
{
#ifdef _WIN32
	_aligned_free(ptr);
#else
	free(ptr);
#endif
}

// Fast inverse square root approximation
inline float fastInvSqrt(float x)
{
	const float x2 = x * 0.5f;
	const float threehalfs = 1.5f;

	union
	{
		float f;
		uint32_t i;
	} conv;

	conv.f = x;
	conv.i = 0x5f3759df - (conv.i >> 1);
	conv.f = conv.f * (threehalfs - (x2 * conv.f * conv.f));
	return conv.f;
}

// Physics engine using Structure of Arrays (SoA) for better performance
class PhysicsEngine
{
private:
	// Core particle data aligned for SIMD operations
	float* posX; // x positions
	float* posY; // y positions
	float* oldPosX; // previous x positions
	float* oldPosY; // previous y positions
	float* velX; // x velocities
	float* velY; // y velocities
	float* invMass; // inverse masses (0 for static particles)
	uint8_t* isStatic; // packed boolean for static particles

	// Simulation parameters
	size_t capacity; // allocated capacity
	size_t count; // actual number of particles
	float uniformRadius; // radius used for all circles
	float gravityX, gravityY;
	float timeStep;
	int substeps;
	float worldMinX, worldMinY, worldMaxX, worldMaxY;

	// Collision detection acceleration structure
	std::unique_ptr<SpatialGrid> grid;

	// Thread hint (for future use)
	int numThreadHint;

	// Working memory for collision detection
	// (pre-allocated to avoid runtime allocations)
	std::vector<int> collisionBuffer;

public:
	PhysicsEngine(float radius, float gravX = 0.0f, float gravY = -9.8f,
		float timeStep = 1.0f / 60.0f, int substeps = 8,
		float worldMinX = -100.0f, float worldMinY = -100.0f,
		float worldMaxX = 100.0f, float worldMaxY = 100.0f,
		size_t initialCapacity = 1024, int threadHint = 1);

	~PhysicsEngine();

	// Disable copy operations
	PhysicsEngine(const PhysicsEngine&) = delete;
	PhysicsEngine& operator=(const PhysicsEngine&) = delete;

	// Particle management
	int addParticle(float x, float y, float mass, bool isStatic = false);
	void setParticlePosition(int index, float x, float y);
	void setParticleVelocity(int index, float vx, float vy);

	// Accessor methods
	size_t getParticleCount() const { return count; }
	float getParticleX(int index) const { return posX[index]; }
	float getParticleY(int index) const { return posY[index]; }
	float getParticleVelX(int index) const { return velX[index]; }
	float getParticleVelY(int index) const { return velY[index]; }
	float getParticleInvMass(int index) const { return invMass[index]; }
	bool getParticleIsStatic(int index) const { return isStatic[index] != 0; }
	float getParticleRadius() const { return uniformRadius; }

	// Main simulation step
	void update();

private:
	// Ensure we have enough memory for particles
	void ensureCapacity(size_t requiredCapacity);

	// Physics substeps
	void applyExternalForces(float dt);
	void predictPositions(float dt);
	void solveConstraints();
	void updateVelocities(float dt);

	// Constraint solvers
	void solveWorldBoundaryConstraints();
	void solveCollisionConstraints();
};

// Optimized spatial grid for collision detection
// Optimized spatial grid for collision detection
class SpatialGrid
{
private:
	// Flat array of cell start indices and counts
	int* cellStartIndices;
	int* cellCounts;

	// Particle indices sorted by cell
	int* particleIndices;
	size_t particleIndicesCapacity;

	// Cell metadata
	int gridWidth, gridHeight;
	float cellSize;
	float invCellSize; // Precomputed inverse for faster calculations
	float worldMinX, worldMinY;
	size_t maxParticles;

public:
	SpatialGrid(float cellSize, float worldMinX, float worldMinY,
		float worldMaxX, float worldMaxY, size_t maxParticles)
		: cellSize(cellSize)
		, invCellSize(1.0f / cellSize)
		, worldMinX(worldMinX)
		, worldMinY(worldMinY)
		, maxParticles(maxParticles)
	{
		gridWidth = static_cast<int>((worldMaxX - worldMinX) * invCellSize) + 1;
		gridHeight = static_cast<int>((worldMaxY - worldMinY) * invCellSize) + 1;

		size_t numCells = static_cast<size_t>(gridWidth) * gridHeight;

		// Allocate grid data
		cellStartIndices = new int[numCells];
		cellCounts = new int[numCells];

		// Each particle could potentially overlap with more cells in crowded scenarios
		// Using 16 instead of 4 to provide adequate buffer space
		particleIndicesCapacity = maxParticles * 16;
		particleIndices = new int[particleIndicesCapacity];
	}

	~SpatialGrid()
	{
		delete[] cellStartIndices;
		delete[] cellCounts;
		delete[] particleIndices;
	}

	// Clear the grid for a new frame
	void clear()
	{
		size_t numCells = static_cast<size_t>(gridWidth) * gridHeight;
		std::memset(cellCounts, 0, sizeof(int) * numCells);
	}

	// Build the grid from particle positions
	void build(const float* posX, const float* posY, float radius, size_t count)
	{
		clear();
		size_t numCells = static_cast<size_t>(gridWidth) * gridHeight;

		// First pass: count particles per cell
		for (size_t i = 0; i < count; ++i)
		{
			int minCellX = std::max(0, static_cast<int>((posX[i] - radius - worldMinX) * invCellSize));
			int maxCellX = std::min(gridWidth - 1, static_cast<int>((posX[i] + radius - worldMinX) * invCellSize));
			int minCellY = std::max(0, static_cast<int>((posY[i] - radius - worldMinY) * invCellSize));
			int maxCellY = std::min(gridHeight - 1, static_cast<int>((posY[i] + radius - worldMinY) * invCellSize));

			for (int y = minCellY; y <= maxCellY; ++y)
			{
				for (int x = minCellX; x <= maxCellX; ++x)
				{
					int cellIdx = y * gridWidth + x;
					if (cellIdx >= 0 && cellIdx < static_cast<int>(numCells))
					{
						cellCounts[cellIdx]++;
					}
				}
			}
		}

		// Compute start indices
		int currentIndex = 0;
		for (int i = 0; i < static_cast<int>(numCells); ++i)
		{
			cellStartIndices[i] = currentIndex;
			currentIndex += cellCounts[i];
			cellCounts[i] = 0; // Reset for second pass
		}

		// Check if we have enough capacity for all particle indices
		if (currentIndex > static_cast<int>(particleIndicesCapacity))
		{
			// Resize the particleIndices array if needed
			delete[] particleIndices;
			particleIndicesCapacity = static_cast<size_t>(currentIndex * 1.5); // Add some extra room
			particleIndices = new int[particleIndicesCapacity];
		}

		// Second pass: fill particle indices
		for (size_t i = 0; i < count; ++i)
		{
			int minCellX = std::max(0, static_cast<int>((posX[i] - radius - worldMinX) * invCellSize));
			int maxCellX = std::min(gridWidth - 1, static_cast<int>((posX[i] + radius - worldMinX) * invCellSize));
			int minCellY = std::max(0, static_cast<int>((posY[i] - radius - worldMinY) * invCellSize));
			int maxCellY = std::min(gridHeight - 1, static_cast<int>((posY[i] + radius - worldMinY) * invCellSize));

			for (int y = minCellY; y <= maxCellY; ++y)
			{
				for (int x = minCellX; x <= maxCellX; ++x)
				{
					int cellIdx = y * gridWidth + x;
					if (cellIdx >= 0 && cellIdx < static_cast<int>(numCells))
					{
						int insertIdx = cellStartIndices[cellIdx] + cellCounts[cellIdx];

						// Safety check to prevent out-of-bounds access
						if (insertIdx >= 0 && insertIdx < static_cast<int>(particleIndicesCapacity))
						{
							particleIndices[insertIdx] = static_cast<int>(i);
							cellCounts[cellIdx]++;
						}
					}
				}
			}
		}
	}

	// Get potential collision partners for a particle
	void getPotentialCollisions(int particleIndex, float x, float y, float radius,
		std::vector<int>& result)
	{
		result.clear();

		int minCellX = std::max(0, static_cast<int>((x - radius - worldMinX) * invCellSize));
		int maxCellX = std::min(gridWidth - 1, static_cast<int>((x + radius - worldMinX) * invCellSize));
		int minCellY = std::max(0, static_cast<int>((y - radius - worldMinY) * invCellSize));
		int maxCellY = std::min(gridHeight - 1, static_cast<int>((y + radius - worldMinY) * invCellSize));

		for (int cy = minCellY; cy <= maxCellY; ++cy)
		{
			for (int cx = minCellX; cx <= maxCellX; ++cx)
			{
				int cellIdx = cy * gridWidth + cx;
				if (cellIdx >= 0 && cellIdx < gridWidth * gridHeight)
				{
					int start = cellStartIndices[cellIdx];
					int end = start + cellCounts[cellIdx];

					// Safety check to ensure valid range
					if (start >= 0 && end <= static_cast<int>(particleIndicesCapacity))
					{
						for (int i = start; i < end; ++i)
						{
							int idx = particleIndices[i];
							if (idx != particleIndex && std::find(result.begin(), result.end(), idx) == result.end())
							{
								result.push_back(idx);
							}
						}
					}
				}
			}
		}
	}
};

// PhysicsEngine implementation
PhysicsEngine::PhysicsEngine(float radius, float gravX, float gravY,
	float timeStep, int substeps,
	float worldMinX, float worldMinY,
	float worldMaxX, float worldMaxY,
	size_t initialCapacity, int threadHint)
	: uniformRadius(radius)
	, gravityX(gravX)
	, gravityY(gravY)
	, timeStep(timeStep)
	, substeps(substeps)
	, worldMinX(worldMinX)
	, worldMinY(worldMinY)
	, worldMaxX(worldMaxX)
	, worldMaxY(worldMaxY)
	, capacity(0)
	, count(0)
	, numThreadHint(threadHint)
{

	// Allocate memory for particles
	ensureCapacity(initialCapacity);

	// Create spatial grid for collision detection
	grid = std::make_unique<SpatialGrid>(radius * 2.0f,
		worldMinX, worldMinY,
		worldMaxX, worldMaxY,
		initialCapacity);

	// Initialize collision buffer
	collisionBuffer.reserve(64); // Pre-allocate for typical collision counts
}

PhysicsEngine::~PhysicsEngine()
{
	// Free aligned memory
	aligned_free(posX);
	aligned_free(posY);
	aligned_free(oldPosX);
	aligned_free(oldPosY);
	aligned_free(velX);
	aligned_free(velY);
	aligned_free(invMass);
	aligned_free(isStatic);
}

void PhysicsEngine::ensureCapacity(size_t requiredCapacity)
{
	if (capacity >= requiredCapacity)
	{
		return;
	}

	// Calculate new capacity (1.5x growth)
	size_t newCapacity = capacity == 0 ? requiredCapacity : std::max(requiredCapacity, capacity + (capacity >> 1));

	// Allocate new arrays with 32-byte alignment for AVX
	float* newPosX = static_cast<float*>(aligned_alloc(32, newCapacity * sizeof(float)));
	float* newPosY = static_cast<float*>(aligned_alloc(32, newCapacity * sizeof(float)));
	float* newOldPosX = static_cast<float*>(aligned_alloc(32, newCapacity * sizeof(float)));
	float* newOldPosY = static_cast<float*>(aligned_alloc(32, newCapacity * sizeof(float)));
	float* newVelX = static_cast<float*>(aligned_alloc(32, newCapacity * sizeof(float)));
	float* newVelY = static_cast<float*>(aligned_alloc(32, newCapacity * sizeof(float)));
	float* newInvMass = static_cast<float*>(aligned_alloc(32, newCapacity * sizeof(float)));
	uint8_t* newIsStatic = static_cast<uint8_t*>(aligned_alloc(32, newCapacity * sizeof(uint8_t)));

	// Copy existing data if any
	if (count > 0)
	{
		std::memcpy(newPosX, posX, count * sizeof(float));
		std::memcpy(newPosY, posY, count * sizeof(float));
		std::memcpy(newOldPosX, oldPosX, count * sizeof(float));
		std::memcpy(newOldPosY, oldPosY, count * sizeof(float));
		std::memcpy(newVelX, velX, count * sizeof(float));
		std::memcpy(newVelY, velY, count * sizeof(float));
		std::memcpy(newInvMass, invMass, count * sizeof(float));
		std::memcpy(newIsStatic, isStatic, count * sizeof(uint8_t));
	}

	// Free old arrays
	if (capacity > 0)
	{
		aligned_free(posX);
		aligned_free(posY);
		aligned_free(oldPosX);
		aligned_free(oldPosY);
		aligned_free(velX);
		aligned_free(velY);
		aligned_free(invMass);
		aligned_free(isStatic);
	}

	// Update pointers and capacity
	posX = newPosX;
	posY = newPosY;
	oldPosX = newOldPosX;
	oldPosY = newOldPosY;
	velX = newVelX;
	velY = newVelY;
	invMass = newInvMass;
	isStatic = newIsStatic;
	capacity = newCapacity;
}

int PhysicsEngine::addParticle(float x, float y, float mass, bool staticObj)
{
	// Ensure we have space
	if (count >= capacity)
	{
		ensureCapacity(capacity == 0 ? 1024 : capacity * 2);
	}

	int index = count++;

	// Initialize particle data
	posX[index] = x;
	posY[index] = y;
	oldPosX[index] = x;
	oldPosY[index] = y;
	velX[index] = 0.0f;
	velY[index] = 0.0f;
	invMass[index] = staticObj ? 0.0f : 1.0f / mass;
	isStatic[index] = staticObj ? 1 : 0;

	return index;
}

void PhysicsEngine::setParticlePosition(int index, float x, float y)
{
	if (index >= 0 && index < static_cast<int>(count))
	{
		posX[index] = x;
		posY[index] = y;
		oldPosX[index] = x;
		oldPosY[index] = y;
	}
}

void PhysicsEngine::setParticleVelocity(int index, float vx, float vy)
{
	if (index >= 0 && index < static_cast<int>(count))
	{
		velX[index] = vx;
		velY[index] = vy;
	}
}

void PhysicsEngine::update()
{
	float dt = timeStep / substeps;

	for (int step = 0; step < substeps; ++step)
	{
		applyExternalForces(dt);
		predictPositions(dt);
		solveConstraints();
		updateVelocities(dt);
	}
}

void PhysicsEngine::applyExternalForces(float dt)
{
// SIMD-optimized gravity application
#ifdef __AVX__
	__m256 dtVec = _mm256_set1_ps(dt);
	__m256 gravXVec = _mm256_set1_ps(gravityX);
	__m256 gravYVec = _mm256_set1_ps(gravityY);
	__m256 zeroVec = _mm256_setzero_ps();

	for (size_t i = 0; i < count; i += 8)
	{
		// Load 8 static flags
		__m256 staticMask;
		if (i + 8 <= count)
		{
			// Create mask from isStatic flags
			alignas(32) int staticIntArray[8];
			for (int j = 0; j < 8; ++j)
			{
				staticIntArray[j] = isStatic[i + j];
			}
			__m256i staticInt = _mm256_load_si256(reinterpret_cast<__m256i*>(staticIntArray));
			staticMask = _mm256_cmp_ps(
				_mm256_castsi256_ps(staticInt),
				zeroVec,
				_CMP_EQ_OQ);
		}
		else
		{
			// Handle remainder (fewer than 8 particles)
			alignas(32) float mask[8] = { 0 };
			for (size_t j = 0; j < count - i; ++j)
			{
				mask[j] = isStatic[i + j] == 0 ? 1.0f : 0.0f;
			}
			staticMask = _mm256_load_ps(mask);
		}

		// Load velocities
		__m256 vxVec, vyVec;
		if (i + 8 <= count)
		{
			vxVec = _mm256_load_ps(&velX[i]);
			vyVec = _mm256_load_ps(&velY[i]);
		}
		else
		{
			// Handle remainder
			alignas(32) float vxTemp[8] = { 0 }, vyTemp[8] = { 0 };
			for (size_t j = 0; j < count - i; ++j)
			{
				vxTemp[j] = velX[i + j];
				vyTemp[j] = velY[i + j];
			}
			vxVec = _mm256_load_ps(vxTemp);
			vyVec = _mm256_load_ps(vyTemp);
		}

		// Apply gravity (only to non-static particles)
		__m256 dvxVec = _mm256_mul_ps(gravXVec, dtVec);
		__m256 dvyVec = _mm256_mul_ps(gravYVec, dtVec);
		dvxVec = _mm256_and_ps(dvxVec, staticMask);
		dvyVec = _mm256_and_ps(dvyVec, staticMask);

		vxVec = _mm256_add_ps(vxVec, dvxVec);
		vyVec = _mm256_add_ps(vyVec, dvyVec);

		// Store updated velocities
		if (i + 8 <= count)
		{
			_mm256_store_ps(&velX[i], vxVec);
			_mm256_store_ps(&velY[i], vyVec);
		}
		else
		{
			// Handle remainder
			alignas(32) float vxResult[8], vyResult[8];
			_mm256_store_ps(vxResult, vxVec);
			_mm256_store_ps(vyResult, vyVec);
			for (size_t j = 0; j < count - i; ++j)
			{
				velX[i + j] = vxResult[j];
				velY[i + j] = vyResult[j];
			}
		}
	}
#else
	// Fallback for non-AVX systems
	for (size_t i = 0; i < count; ++i)
	{
		if (!isStatic[i])
		{
			velX[i] += gravityX * dt;
			velY[i] += gravityY * dt;
		}
	}
#endif
}

void PhysicsEngine::predictPositions(float dt)
{
// Save old positions and predict new ones
#ifdef __AVX__
	__m256 dtVec = _mm256_set1_ps(dt);

	for (size_t i = 0; i < count; i += 8)
	{
		if (i + 8 <= count)
		{
			// Full vector load
			__m256 posXVec = _mm256_load_ps(&posX[i]);
			__m256 posYVec = _mm256_load_ps(&posY[i]);
			__m256 velXVec = _mm256_load_ps(&velX[i]);
			__m256 velYVec = _mm256_load_ps(&velY[i]);

			// Save old positions
			_mm256_store_ps(&oldPosX[i], posXVec);
			_mm256_store_ps(&oldPosY[i], posYVec);

			// Predict new positions
			posXVec = _mm256_add_ps(posXVec, _mm256_mul_ps(velXVec, dtVec));
			posYVec = _mm256_add_ps(posYVec, _mm256_mul_ps(velYVec, dtVec));

			// Store new positions
			_mm256_store_ps(&posX[i], posXVec);
			_mm256_store_ps(&posY[i], posYVec);
		}
		else
		{
			// Handle remainder with scalar code
			for (size_t j = i; j < count; ++j)
			{
				oldPosX[j] = posX[j];
				oldPosY[j] = posY[j];

				posX[j] += velX[j] * dt;
				posY[j] += velY[j] * dt;
			}
		}
	}
#else
	// Fallback for non-AVX systems
	for (size_t i = 0; i < count; ++i)
	{
		oldPosX[i] = posX[i];
		oldPosY[i] = posY[i];

		posX[i] += velX[i] * dt;
		posY[i] += velY[i] * dt;
	}
#endif
}

void PhysicsEngine::solveConstraints()
{
	// First build the spatial grid
	grid->build(posX, posY, uniformRadius, count);

	// Then solve constraints
	solveWorldBoundaryConstraints();
	solveCollisionConstraints();
}

void PhysicsEngine::solveWorldBoundaryConstraints()
{
	const float minX = worldMinX + uniformRadius;
	const float maxX = worldMaxX - uniformRadius;
	const float minY = worldMinY + uniformRadius;
	const float maxY = worldMaxY - uniformRadius;

	for (size_t i = 0; i < count; ++i)
	{
		if (isStatic[i])
		{
			continue;
		}

		// X boundary constraints
		if (posX[i] < minX)
		{
			posX[i] = minX;
		}
		else if (posX[i] > maxX)
		{
			posX[i] = maxX;
		}

		// Y boundary constraints
		if (posY[i] < minY)
		{
			posY[i] = minY;
		}
		else if (posY[i] > maxY)
		{
			posY[i] = maxY;
		}
	}
}

void PhysicsEngine::solveCollisionConstraints()
{
	const float radiusSum = uniformRadius * 2.0f;
	const float radiusSumSquared = radiusSum * radiusSum;

	for (size_t i = 0; i < count; ++i)
	{
		// Skip static particles for first loop (they don't move)
		if (isStatic[i])
		{
			continue;
		}

		// Get potential collision candidates
		grid->getPotentialCollisions(i, posX[i], posY[i], uniformRadius, collisionBuffer);

		// Check and resolve collisions
		for (int j : collisionBuffer)
		{
			// Skip if both particles are static (nothing to solve)
			if (isStatic[i] && isStatic[j])
			{
				continue;
			}

			// Calculate distance between particles
			float dx = posX[i] - posX[j];
			float dy = posY[i] - posY[j];
			float distSqr = dx * dx + dy * dy;

			// Skip if not colliding
			if (distSqr >= radiusSumSquared || distSqr < 1e-6f)
			{
				continue;
			}

			// Fast approximate inverse square root
			float invDist = fastInvSqrt(distSqr);
			float dist = 1.0f / invDist;

			// Normalize direction
			float nx = dx * invDist;
			float ny = dy * invDist;

			// Calculate penetration depth
			float penetration = radiusSum - dist;

			// Calculate position correction based on inverse mass
			float totalInvMass = invMass[i] + invMass[j];
			if (totalInvMass < 1e-6f)
			{
				continue;
			}

			float correctionFactor = penetration / totalInvMass;

			// Apply position corrections
			float correctionX = nx * correctionFactor;
			float correctionY = ny * correctionFactor;

			posX[i] += correctionX * invMass[i];
			posY[i] += correctionY * invMass[i];
			posX[j] -= correctionX * invMass[j];
			posY[j] -= correctionY * invMass[j];
		}
	}
}

void PhysicsEngine::updateVelocities(float dt)
{
	float invDt = 1.0f / dt;

#ifdef __AVX__
	__m256 invDtVec = _mm256_set1_ps(invDt);

	for (size_t i = 0; i < count; i += 8)
	{
		if (i + 8 <= count)
		{
			// Full vector load
			__m256 posXVec = _mm256_load_ps(&posX[i]);
			__m256 posYVec = _mm256_load_ps(&posY[i]);
			__m256 oldPosXVec = _mm256_load_ps(&oldPosX[i]);
			__m256 oldPosYVec = _mm256_load_ps(&oldPosY[i]);

			// Calculate new velocities
			__m256 velXVec = _mm256_mul_ps(_mm256_sub_ps(posXVec, oldPosXVec), invDtVec);
			__m256 velYVec = _mm256_mul_ps(_mm256_sub_ps(posYVec, oldPosYVec), invDtVec);

			// Store new velocities
			_mm256_store_ps(&velX[i], velXVec);
			_mm256_store_ps(&velY[i], velYVec);
		}
		else
		{
			// Handle remainder with scalar code
			for (size_t j = i; j < count; ++j)
			{
				velX[j] = (posX[j] - oldPosX[j]) * invDt;
				velY[j] = (posY[j] - oldPosY[j]) * invDt;
			}
		}
	}
#else
	// Fallback for non-AVX systems
	for (size_t i = 0; i < count; ++i)
	{
		velX[i] = (posX[i] - oldPosX[i]) * invDt;
		velY[i] = (posY[i] - oldPosY[i]) * invDt;
	}
#endif
}

// Example usage demonstrating performance
void testSimulator()
{
	// Create physics engine
	PhysicsEngine engine(1.0f, 0.0f, -9.8f, 1.0f / 60.0f, 8);

	// Add particles
	const int numParticles = 10000;
	for (int i = 0; i < numParticles; ++i)
	{
		float x = static_cast<float>(rand() % 190 - 95);
		float y = static_cast<float>(rand() % 190 - 95);
		float mass = 1.0f + static_cast<float>(rand() % 10) / 5.0f;
		engine.addParticle(x, y, mass);
	}

	// Add boundaries as static particles
	for (int i = -90; i <= 90; i += 10)
	{
		engine.addParticle(i, -98.0f, 100.0f, true); // bottom
		engine.addParticle(i, 98.0f, 100.0f, true); // top
		engine.addParticle(-98.0f, i, 100.0f, true); // left
		engine.addParticle(98.0f, i, 100.0f, true); // right
	}

	// Simulation benchmark
	const int numFrames = 300;
	auto startTime = std::chrono::high_resolution_clock::now();

	for (int frame = 0; frame < numFrames; ++frame)
	{
		engine.update();
	}

	auto endTime = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

	std::cout << "Simulated " << numParticles << " particles for " << numFrames
			  << " frames in " << duration << "ms" << std::endl;
	std::cout << "Average: " << static_cast<float>(duration) / numFrames
			  << "ms per frame";
}

////////

#include <iostream>

#include "weird-engine.h"

using namespace WeirdEngine;

class RopeScene : public Scene
{
public:
	RopeScene()
		: Scene() {};

private:
	Entity m_star;
	double m_lastSpawnTime = 0.f;

	// Inherited via Scene
	void onStart() override
	{
		constexpr int circles = 60;

		constexpr float startY = 20 + (circles / 30);

		// Spawn 2d balls
		for (size_t i = 0; i < circles; i++)
		{
			float x = i % 30;
			float y = startY - (int)(i / 30);

			int material = 4 + (i % 12);

			Transform t;
			t.position = vec3(x + 0.5f, y + 0.5f, 0);

			Entity entity = m_ecs.createEntity();
			m_ecs.addComponent(entity, t);

			m_ecs.addComponent(entity, SDFRenderer(material));

			RigidBody2D rb(m_simulation2D);
			m_ecs.addComponent(entity, rb);
		}

		constexpr float stiffness = 20000000.0f;

		for (size_t i = 0; i < circles; i++)
		{
			// Check it's not last row
			if (i + 30 < circles)
			{
				// Connect down
				m_simulation2D.addSpring(i, i + 30, stiffness);
			}

			// Last column
			if ((i + 1) % 30 == 0)
			{
				continue;
			}
			m_simulation2D.addSpring(i, i + 1, stiffness);

			/*if (i + 30 < circles)
			{
				simulation.addSpring(i, i + 31, stiffness, 1.42f);
				simulation.addSpring(i + 1, i + 30, stiffness, 1.42f);
			}*/
		}

		if (circles >= 30)
		{
			m_simulation2D.fix(0);
			m_simulation2D.fix(29);
		}

		// Add shapes

		{
			float variables[8]{ 1.0f, 0.5f };
			addShape(0, variables);
		}

		{
			float variables[8]{ 25.0f, 10.0f, 5.0f, 0.5f, 13.0f, 5.0f };
			m_star = addShape(1, variables);
		}
	}

	void throwBalls(ECSManager& ecs, Simulation2D& simulation2D)
	{
		if (simulation2D.getSimulationTime() > m_lastSpawnTime + 0.1)
		{
			int amount = 10;
			for (size_t i = 0; i < amount; i++)
			{
				float x = 0.f;
				float y = 60 + (1.2 * i);
				float z = 0;

				Transform t;
				t.position = vec3(x + 0.5f, y + 0.5f, z);
				Entity entity = ecs.createEntity();
				ecs.addComponent(entity, t);

				ecs.addComponent(entity, SDFRenderer(4 + ecs.getComponentArray<SDFRenderer>()->getSize() % 12));

				RigidBody2D rb(simulation2D);
				ecs.addComponent(entity, rb);
				simulation2D.addForce(rb.simulationId, vec2(20, 0));

				// std::cout << rb.simulationId << std::endl;
			}

			m_lastSpawnTime = simulation2D.getSimulationTime();
		}
	}

	void onUpdate() override
	{
		// Change shape
		{
			CustomShape& cs = m_ecs.getComponent<CustomShape>(m_star);
			cs.m_parameters[4] = (static_cast<int>(std::floor(m_simulation2D.getSimulationTime())) % 5) + 2;
			cs.m_parameters[3] = sin(3.1416 * m_simulation2D.getSimulationTime());
			cs.m_isDirty = true;
		}

		// Add balls
		if (Input::GetKey(Input::E))
		{
			throwBalls(m_ecs, m_simulation2D);
		}

		if (Input::GetKeyDown(Input::M))
		{

			// Get mouse coordinates
			auto& cameraTransform = m_ecs.getComponent<Transform>(m_mainCamera);
			float x = Input::GetMouseX();
			float y = Input::GetMouseY();

			// Transform mouse coordinates to world space
			vec2 mousePositionInWorld = ECS::Camera::screenPositionToWorldPosition2D(cameraTransform, vec2(x, y));

			Entity star = m_ecs.createEntity();

			float variables[8]{ mousePositionInWorld.x, mousePositionInWorld.y, 5.0f, 0.5f, 13.0f, 5.0f };
			CustomShape shape(1, variables);
			m_ecs.addComponent(star, shape);


		}

		if (Input::GetKeyDown(Input::N))
		{
			// Test
			{
				m_sdfs.push_back(m_sdfs[m_sdfs.size() - 1]);
			}

			m_simulation2D.setSDFs(m_sdfs);

			auto& cameraTransform = m_ecs.getComponent<Transform>(m_mainCamera);
			float x = Input::GetMouseX();
			float y = Input::GetMouseY();

			// Transform mouse coordinates to world space
			vec2 mousePositionInWorld = ECS::Camera::screenPositionToWorldPosition2D(cameraTransform, vec2(x, y));

			float variables[8]{ mousePositionInWorld.x, mousePositionInWorld.y, 5.0f, 7.5f, 1.0f };

			Entity test = m_ecs.createEntity();

			CustomShape shape(m_sdfs.size() - 1, variables);
			m_ecs.addComponent(test, shape);

			//newShapeAdded = true;
		}

		if (Input::GetKeyDown(Input::K))
		{
			auto components = m_ecs.getComponentArray<CustomShape>();
			auto id = components->getSize() - 1;

			m_simulation2D.removeShape(components->getDataAtIdx(id));
			m_ecs.destroyEntity(components->getDataAtIdx(id).Owner);

			//newShapeAdded = true;
		}
	}

	void onRender() override
	{
	}
};

class MouseCollisionScene : public Scene
{
public:
	MouseCollisionScene()
		: Scene() {};

private:
	Entity m_cursorShape;

	// Inherited via Scene
	void onStart() override
	{
		for (size_t i = 0; i < 600; i++)
		{

			float y = (int)(i / 20);
			float x = 5 + (i % 20) + sin(y);

			int material = 4 + (i % 12);

			float z = 0;

			Transform t;
			t.position = vec3(x + 0.5f, y + 0.5f, z);

			Entity entity = m_ecs.createEntity();
			m_ecs.addComponent(entity, t);

			if (i < 100)
			{
			}
			m_ecs.addComponent(entity, SDFRenderer(material));

			RigidBody2D rb(m_simulation2D);
			m_ecs.addComponent(entity, rb);
		}

		// Floor
		{
			Entity floor = m_ecs.createEntity();

			float variables[8]{ 0.5f, 1.5f, -1.0f };
			CustomShape shape(0, variables);
			m_ecs.addComponent(floor, shape);
		}

		{
			Entity star = m_ecs.createEntity();

			float variables[8]{ -15.0f, 50.0f, 5.0f, 5.0f, 2.0f, 10.0f };
			CustomShape shape(1, variables);
			m_ecs.addComponent(star, shape);

			m_cursorShape = star;
		}
	}

	void onUpdate() override
	{
		// Move wall to mouse
		{
			CustomShape& cs = m_ecs.getComponent<CustomShape>(m_cursorShape);
			auto& cameraTransform = m_ecs.getComponent<Transform>(m_mainCamera);
			float x = Input::GetMouseX();
			float y = Input::GetMouseY();

			// Transform mouse coordinates to world space
			vec2 mousePositionInWorld = ECS::Camera::screenPositionToWorldPosition2D(cameraTransform, vec2(x, y));

			cs.m_parameters[0] = mousePositionInWorld.x;
			cs.m_parameters[1] = mousePositionInWorld.y;
			cs.m_isDirty = true;
		}
	}

	void onRender() override
	{
	}
};

class ImageScene : public Scene
{
public:
	ImageScene()
		: Scene() {};

private:
	std::string binaryString;
	std::string filePath = "cache/image.txt";
	std::string imagePath = "SampleProject/Resources/Textures/image.jpg";

	// Inherited via Scene
	void onStart() override
	{
		// Check if the folder exists
		if (!std::filesystem::exists("cache/"))
		{
			// If it doesn't exist, create the folder
			std::filesystem::create_directory("cache/");
		}

		if (checkIfFileExists(filePath.c_str()))
		{
			binaryString = get_file_contents(filePath.c_str());
		}
		else
		{
			binaryString = "0";
		}

		uint32_t currentChar = 0;

		// Spawn 2d balls
		for (size_t i = 0; i < 1200; i++)
		{
			float x;
			float y;
			int material = 0;

			x = 15 + sin(i);
			y = 10 + (1.0f * i);

			std::string materialId;
			while (currentChar < binaryString.size() && binaryString[currentChar] != '-')
			{
				materialId += binaryString[currentChar++];
			}
			currentChar++;

			material = (materialId.size() > 0 && materialId.size() <= 2) ? std::stoi(materialId) : 0;

			Transform t;
			t.position = vec3(x + 0.5f, y + 0.5f, 0);

			Entity entity = m_ecs.createEntity();
			m_ecs.addComponent(entity, t);

			m_ecs.addComponent(entity, SDFRenderer(material));

			RigidBody2D rb(m_simulation2D);
			m_ecs.addComponent(entity, rb);
		}

		// Floor
		{
			float variables[8]{ 15, -5, 25.0f, 5.0f, 0.0f };

			Entity floor = m_ecs.createEntity();

			CustomShape shape(m_sdfs.size() - 1, variables);
			m_ecs.addComponent(floor, shape);


		}

		// Wall right
		{
			float variables[8]{ 30 + 5, 20, 5.0f, 30.0f, 0.0f };

			Entity floor = m_ecs.createEntity();

			CustomShape shape(m_sdfs.size() - 1, variables);
			m_ecs.addComponent(floor, shape);


		}

		// Wall left
		{
			float variables[8]{ 0 - 5, 20, 5.0f, 30.0f, 0.0f };

			Entity floor = m_ecs.createEntity();

			CustomShape shape(m_sdfs.size() - 1, variables);
			m_ecs.addComponent(floor, shape);


		}
	}

	vec3 getColor(const char* path, int x, int y)
	{
		// Load the image
		int width, height, channels;
		unsigned char* img = wstbi_load(path, &width, &height, &channels, 0);

		if (img == nullptr)
		{
			std::cerr << "Error: could not load image." << std::endl;
			return vec3();
		}

		if (x < 0)
		{
			x = 0;
		}
		else if (x >= width)
		{
			x = width - 1;
		}

		if (y < 0)
		{
			y = 0;
		}
		else if (y >= height)
		{
			y = height - 1;
		}

		// Calculate the index of the pixel in the image data
		int index = (y * width + x) * channels;

		if (index < 0 || index >= width * height * channels)
		{
			return vec3();
		}

		// Get the color values
		unsigned char r = img[index];
		unsigned char g = img[index + 1];
		unsigned char b = img[index + 2];
		unsigned char a = (channels == 4) ? img[index + 3] : 255; // Alpha channel (if present)

		// Free the image memory
		wstbi_image_free(img);

		return vec3(
			static_cast<int>(r) / 255.0f,
			static_cast<int>(g) / 255.0f,
			static_cast<int>(b) / 255.0f);
	}

	void onUpdate() override
	{
		// Get colors
		if (Input::GetKeyDown(Input::P))
		{
			auto components = m_ecs.getComponentArray<RigidBody2D>();

			// Result string
			std::string result;
			result.reserve(components->getSize());
			for (size_t i = 0; i < components->getSize(); i++)
			{
				RigidBody2D& rb = components->getDataAtIdx(i);
				Transform& t = m_ecs.getComponent<Transform>(rb.Owner);

				int x = floor(t.position.x);
				int y = floor(30 - t.position.y);

				vec3 color = getColor(imagePath.c_str(), x * 10, y * 10);
				int id = m_sdfRenderSystem2D.findClosestColorInPalette(color);

				result += std::to_string(id) + "-";
			}

			saveToFile(filePath.c_str(), result);
			std::cout << "Image saved" << std::endl;
		}
	}

	void onRender() override
	{
	}
};

#include <random>
class FireworksScene : public Scene
{
public:
	FireworksScene()
		: Scene() {};

private:
	// Inherited via Scene
	void onStart() override
	{
		// Create a random number generator engine
		std::random_device rd;
		std::mt19937 gen(rd());
		float range = 0.5f;
		std::uniform_real_distribution<> distrib(-range, range);

		for (size_t i = 0; i < 60; i++)
		{
			float y = 50 + distrib(gen);
			float x = 15 + distrib(gen);

			int material = 4 + (i % 12);

			float z = 0;

			Transform t;
			t.position = vec3(x + 0.5f, y + 0.5f, z);

			Entity entity = m_ecs.createEntity();
			m_ecs.addComponent(entity, t);

			if (i < 100)
			{
			}
			m_ecs.addComponent(entity, SDFRenderer(material));

			RigidBody2D rb(m_simulation2D);
			m_ecs.addComponent(entity, rb);
		}

		// Floor
		{
			Entity floor = m_ecs.createEntity();

			float variables[8]{ 0.5f, 1.5f, -1.0f };
			CustomShape shape(0, variables);
			m_ecs.addComponent(floor, shape);
		}
	}

	void onUpdate() override
	{
	}

	void onRender() override
	{
	}
};


class SpaceScene : public Scene
{
private:
	std::vector<Entity> m_celestialBodies;
	uint16_t m_current = 0;
	bool m_lookAtBody = true;

	// Inherited via Scene
	void onStart() override
	{
		//m_debugFly = false;

		m_simulation2D.setGravity(0);
		m_simulation2D.setDamping(0);

		loadRandomSystem();
	}

	void loadRandomSystem() {

		std::random_device rd;
		std::mt19937 gen(rd());

		std::uniform_real_distribution<float> floatDistrib(-1, 1);
		std::uniform_real_distribution<float> massDistrib(0.1f, 100.0f);
		std::uniform_int_distribution<int> colorDistrib(2, 15);

		size_t bodyCount = 1000;
		float r = 0;
		for (size_t i = 0; i < bodyCount; i++)
		{

			Entity body = m_ecs.createEntity();

			Transform t;
			vec2 pos(floatDistrib(gen), floatDistrib(gen));
			t.position = 500.0f * vec3(pos.x, pos.y, 0);
			m_ecs.addComponent(body, t);

			m_ecs.addComponent(body, SDFRenderer(4 + (i % 3)));

			RigidBody2D rb(m_simulation2D);
			m_simulation2D.setMass(rb.simulationId, 1.0f);
			m_ecs.addComponent(body, rb);


			for (auto b : m_celestialBodies)
			{
				m_simulation2D.addGravitationalConstraint(
					m_ecs.getComponent<RigidBody2D>(body).simulationId,
					m_ecs.getComponent<RigidBody2D>(b).simulationId,
					100.0f);
			}

			m_simulation2D.addForce(m_ecs.getComponent<RigidBody2D>(body).simulationId, (10.f * vec2(-pos.y, pos.x)) - (10.0f * vec2(pos.x, pos.y)));


			m_celestialBodies.push_back(body);
		}
		
		lookAt(m_celestialBodies[0]);
	}

	void loadSolarSystem()
	{
		Entity sun = m_ecs.createEntity();
		Entity earth = m_ecs.createEntity();
		Entity moon = m_ecs.createEntity();

		{
			Transform t;
			t.position = vec3(15, 35, 0);
			m_ecs.addComponent(sun, t);

			m_ecs.addComponent(sun, SDFRenderer(8));

			RigidBody2D rb(m_simulation2D);
			m_simulation2D.setMass(rb.simulationId, 1000.f);
			m_ecs.addComponent(sun, rb);
		}

		{
			Transform t;
			t.position = vec3(45, 35, 0);
			m_ecs.addComponent(earth, t);

			m_ecs.addComponent(earth, SDFRenderer(6));

			RigidBody2D rb(m_simulation2D);
			m_simulation2D.setMass(rb.simulationId, 1.0f);
			m_ecs.addComponent(earth, rb);
		}

		m_simulation2D.fix(m_ecs.getComponent<RigidBody2D>(sun).simulationId);
		m_simulation2D.addGravitationalConstraint(
			m_ecs.getComponent<RigidBody2D>(sun).simulationId,
			m_ecs.getComponent<RigidBody2D>(earth).simulationId,
			1.0f);

		m_simulation2D.addForce(m_ecs.getComponent<RigidBody2D>(earth).simulationId, vec2(0, 5));

		{
			Transform t;
			t.position = vec3(55, 35, 0);
			m_ecs.addComponent(moon, t);

			m_ecs.addComponent(moon, SDFRenderer(2));

			RigidBody2D rb(m_simulation2D);
			m_simulation2D.setMass(rb.simulationId, 0.001f);
			m_ecs.addComponent(moon, rb);
		}

		m_simulation2D.addGravitationalConstraint(
			m_ecs.getComponent<RigidBody2D>(earth).simulationId,
			m_ecs.getComponent<RigidBody2D>(moon).simulationId,
			1000.0f);

		m_simulation2D.addForce(m_ecs.getComponent<RigidBody2D>(moon).simulationId, vec2(0, 10));

		m_celestialBodies.resize(3);
		m_celestialBodies[0] = sun;
		m_celestialBodies[1] = earth;
		m_celestialBodies[2] = moon;
	}



	void onUpdate() override
	{
		if (Input::GetKeyDown(Input::E)) {
			m_current = (m_current + 1) % m_celestialBodies.size();
		}
		
		if (Input::GetKeyDown(Input::F)) {
			m_lookAtBody = !m_lookAtBody;
		}

		if (m_lookAtBody && m_celestialBodies.size() > 0)
			lookAt(m_celestialBodies[m_current]);
	}

	void onRender() override
	{
	}
};

class ApparentCircularMotionScene : public Scene
{
private:
	// Inherited via Scene
	uint16_t m_count = 8;
	std::vector<Entity> m_circles;

	void onStart() override
	{
		m_simulation2D.setDamping(0.0f);
		m_simulation2D.setGravity(0.0f);

		for (size_t i = 0; i < m_count; i++)
		{
			Entity entity = m_ecs.createEntity();

			Transform t;
			t.position = vec3(0, 0, 0);
			m_ecs.addComponent(entity, t);

			m_ecs.addComponent(entity, SDFRenderer(i % 16));
			m_ecs.addComponent(entity, RigidBody2D(m_simulation2D));

			m_circles.push_back(entity);

		}
	}

	void onUpdate() override
	{
		auto time = getTime();

		if (time > 1)
		{
			// return;
		}

		for (size_t i = 0; i < m_count; i++)
		{
			float angle = 1 * 3.1416 * i / m_count;
			auto& t = m_ecs.getComponent<Transform>(m_circles[i]);
			t.position = (10.0f * ((float)sin(time + angle)) * vec3(cos(angle), sin(angle), 0)) + vec3(15, 15, 0);
			t.isDirty = true;
		}
	}

	void onRender() override
	{
	}
};

int main()
{
	testSimulator();
	return 0;


	const char* projectPath = "weird-engine/SampleProject/";

	SceneManager& sceneManager = SceneManager::getInstance();
	sceneManager.registerScene<RopeScene>("rope");
	sceneManager.registerScene<MouseCollisionScene>("cursor-collision");
	sceneManager.registerScene<ImageScene>("image");
	sceneManager.registerScene<FireworksScene>("fireworks");
	sceneManager.registerScene<ApparentCircularMotionScene>("circle");
	sceneManager.registerScene<SpaceScene>("space");

	start(sceneManager);
}