#pragma once

#include "weird-physics/Simulation2D.h"

namespace WeirdEngine
{
	class TimeService
	{
	public:
		TimeService(Simulation2D& simulation, const float& delta)
			: m_simulation(simulation)
			, m_delta(delta)
		{
		}

		float time() const
		{
			return static_cast<float>(m_simulation.getSimulationTime());
		}

		void setTime(double time)
		{
			m_simulation.setSimulationTime(time);
		}

		float deltaTime() const
		{
			return m_delta;
		}

		double fixedDeltaTime() const
		{
			return m_simulation.getDeltaTime();
		}

	private:
		Simulation2D& m_simulation;
		const float& m_delta;
	};
} // namespace WeirdEngine
